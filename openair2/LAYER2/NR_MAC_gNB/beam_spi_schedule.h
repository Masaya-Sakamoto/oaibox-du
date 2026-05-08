/*
 * beam_spi_schedule.h — Producer API for beam SPI scheduling
 *
 * Called from gNB_dlsch_ulsch_scheduler() after all sub-schedulers have run.
 * Scans the beam_allocation table to extract beam switch entries for the
 * current slot and enqueues them for the SPI worker thread.
 *
 * When ENABLE_TMYTEK_UD_BBOX is not defined, this is a no-op.
 */

#ifndef BEAM_SPI_SCHEDULE_H
#define BEAM_SPI_SCHEDULE_H

#include "NR_MAC_gNB/nr_mac_gNB.h"

/**
 * @brief Extract beam schedule from beam_allocation table and enqueue SPI batch.
 *
 * Scans the beam_allocation table for the given slot, identifies symbols where
 * the beam changes (first symbol + each transition), determines direction
 * (TX or RX) from the TDD slot structure, and enqueues a batch for the SPI
 * worker thread via non-blocking push.
 *
 * No-op when ENABLE_TMYTEK_UD_BBOX is not defined.
 *
 * @param gNB   MAC instance (provides beam_info, frame_structure)
 * @param frame Current frame number
 * @param slot  Current slot number
 */
void beam_spi_schedule_slot(gNB_MAC_INST *gNB, frame_t frame, slot_t slot);

#endif /* BEAM_SPI_SCHEDULE_H */
