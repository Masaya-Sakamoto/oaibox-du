/*
 * beam_spi_schedule.c — Producer: extract beam schedule from beam_allocation table
 *
 * Called at the end of gNB_dlsch_ulsch_scheduler(), after all sub-schedulers
 * have written to the beam_allocation table. Scans the table for the current
 * slot, detects beam transitions at symbol group boundaries, and enqueues
 * a beam_spi_slot_batch_t for the SPI worker thread.
 *
 * When ENABLE_TMYTEK_UD_BBOX is not defined, beam_spi_schedule_slot() is a no-op.
 */

#include "beam_spi_schedule.h"
#include "common/utils/nr/nr_common.h"
#include "common/utils/LOG/log.h"
#include "NR_MAC_gNB/mac_proto.h"

#include <stdlib.h>
#include <string.h>

#ifdef ENABLE_TMYTEK_UD_BBOX

#include "radio/USRP/beam_spi/beam_spi_worker.h"

/* ── Data structures (duplicated from beam_spi_task.h for MAC-layer use) ── */

#define BEAM_SPI_TX 0
#define BEAM_SPI_RX 1

// typedef struct {
//     int frame;
//     int slot;
//     int symbol;
//     int16_t beam_id;
//     uint8_t direction;
// } beam_spi_entry_t;

#define BEAM_SPI_MAX_ENTRIES_PER_SLOT 14

// typedef struct {
//     int n_entries;
//     beam_spi_entry_t entries[BEAM_SPI_MAX_ENTRIES_PER_SLOT];
// } beam_spi_slot_batch_t;

/* ── External function provided by beam_spi_worker.cpp (linked at runtime) ── */

extern bool beam_spi_enqueue(beam_spi_slot_batch_t *batch);

/*
 * Determine beam switch direction from TDD slot structure.
 * DL symbols → TX, UL symbols → RX.
 * Guard/flexible symbols default to TX.
 */
static uint8_t get_beam_direction(const frame_structure_t *fs, int slot, int symbol)
{
    if (fs->frame_type == FDD)
        return BEAM_SPI_TX;

    int slot_in_period = get_slot_idx_in_period(slot, fs);
    const tdd_bitmap_t *bm = &fs->period_cfg.tdd_slot_bitmap[slot_in_period];

    switch (bm->slot_type) {
        case TDD_NR_DOWNLINK_SLOT:
            return BEAM_SPI_TX;
        case TDD_NR_UPLINK_SLOT:
            return BEAM_SPI_RX;
        case TDD_NR_MIXED_SLOT:
            if (symbol < bm->num_dl_symbols)
                return BEAM_SPI_TX;
            else if (symbol >= NR_NUMBER_OF_SYMBOLS_PER_SLOT - bm->num_ul_symbols)
                return BEAM_SPI_RX;
            else
                return BEAM_SPI_TX;
        default:
            return BEAM_SPI_TX;
    }
}

/*
 * Compute beam_allocation table slot index.
 * Duplicated from gNB_scheduler_primitives.c (static inline there).
 */
static inline int get_beam_slot_index(const NR_beam_info_t *beam_info,
                                      int frame, int slot, int slots_per_frame)
{
    return ((frame * slots_per_frame + slot) / beam_info->beam_slot_duration)
           % beam_info->beam_allocation_size[0];
}

void beam_spi_schedule_slot(gNB_MAC_INST *gNB, frame_t frame, slot_t slot)
{
    NR_beam_info_t *bi = &gNB->beam_info;
    if (bi->beam_mode == NO_BEAM_MODE)
        return;

    if (bi->beam_allocation_size[1] <= 0)
        return;

    int slots_per_frame = gNB->frame_structure.numb_slots_frame;
    int slot_index = get_beam_slot_index(bi, frame, slot, slots_per_frame);

    int step_size = NR_NUMBER_OF_SYMBOLS_PER_SLOT / bi->beam_allocation_size[1];

    beam_spi_slot_batch_t *batch = (beam_spi_slot_batch_t *)calloc(1, sizeof(*batch));
    if (!batch) {
        LOG_E(NR_MAC, "[BeamSPI] Failed to allocate batch\n");
        return;
    }

    int16_t prev_beam = -1;

    for (int sg = 0; sg < bi->beam_allocation_size[1]; sg++) {
        int16_t current_beam = -1;
        for (int bp = 0; bp < bi->beams_per_period; bp++) {
            int16_t b = bi->beam_allocation[bp][slot_index][sg];
            if (b >= 0) {
                current_beam = b;
                break;
            }
        }

        if (current_beam < 0)
            continue;

        /* Emit entry at: first valid beam OR beam transition */
        if (prev_beam < 0 || current_beam != prev_beam) {
            int start_sym = sg * step_size;
            uint8_t dir = get_beam_direction(&gNB->frame_structure, slot, start_sym);

            if (batch->n_entries < BEAM_SPI_MAX_ENTRIES_PER_SLOT) {
                beam_spi_entry_t *e = &batch->entries[batch->n_entries++];
                e->frame     = frame;
                e->slot      = slot;
                e->symbol    = start_sym;
                e->beam_id   = current_beam;
                e->direction = dir;
            }
        }
        prev_beam = current_beam;
    }

    if (batch->n_entries > 0) {
        if (!beam_spi_enqueue(batch)) {
            LOG_W(NR_MAC, "[BeamSPI] Queue full, dropping batch f=%d s=%d (%d entries)\n",
                  frame, slot, batch->n_entries);
            free(batch);
        } else {
            LOG_D(NR_MAC, "[BeamSPI] Enqueued batch f=%d s=%d (%d entries)\n",
                  frame, slot, batch->n_entries);
        }
    } else {
        free(batch);
    }
}

#else /* !ENABLE_TMYTEK_UD_BBOX */

void beam_spi_schedule_slot(gNB_MAC_INST *gNB, frame_t frame, slot_t slot)
{
    (void)gNB;
    (void)frame;
    (void)slot;
    /* No-op when TMYTEK BBox is not enabled */
}

#endif /* ENABLE_TMYTEK_UD_BBOX */
