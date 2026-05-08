/*
 * beam_spi_worker.cpp — SPI worker thread for TMYTEK BBox beam switching
 *
 * Consumes beam_spi_slot_batch_t from the blocking queue, validates that
 * consecutive beam switch commands have sufficient timing separation
 * (to avoid Phase register drop), and executes timed SPI via UHD.
 *
 * Pattern follows oaibox-ncr ncr_agc_worker (queue + dedicated pthread).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/features/spi_getter_iface.hpp>

extern "C" {
#include "beam_spi_worker.h"
#include "beam_spi_task.h"
#include "ptr_blocking_queue.h"
#include "common/utils/LOG/log.h"
}

/* ── TMYTEK BBox SPI register addresses (from usrp_fbs.cpp) ──────────── */

/* mode=0: TX, mode=1: RX.  [mode][0]=Gain reg, [mode][1]=Phase reg */
static const uint32_t tmy_reg_address[][2] = { {0x58, 0x78}, {0x54, 0x70} };
static const size_t   tmy_payload_bits = 13;

/* GPIO LDB pin mask */
static const size_t GPIO_DEFAULT_LDB_PIN = 6;
static inline uint32_t GPIO_BIT(size_t x) { return 1u << x; }

/* ── Worker context ──────────────────────────────────────────────────── */

typedef struct {
    /* UHD handles */
    uhd::usrp::multi_usrp::sptr usrp;
    uhd::spi_iface::sptr        spi_ref;
    uhd::spi_config_t           spi_config;
    std::string                 gpio_bank;

    /* Timing parameters */
    double   sample_rate;
    int      slots_per_frame;
    int      symbols_per_slot;
    uint64_t samples_per_slot;
    uint64_t samples_per_frame;
    /* SPI Duration in samples: ~3.3μs worth of samples.
     * Two set_command_time calls closer than this cause Phase register drop. */
    uint64_t spi_duration_samples;

    /* Queue and thread */
    ptr_blocking_queue_t *queue;
    pthread_t             thread;
    bool                  running;
} beam_spi_context_t;

static beam_spi_context_t g_ctx;

/* ── Timestamp calculation ───────────────────────────────────────────── */

/*
 * Compute the sample offset of a symbol within a slot.
 * Simplified: assumes normal CP and uniform symbol length.
 * For FR2 μ=3: symbol_size = FFT_size + CP ≈ 1097 samples @ 122.88 MHz.
 *
 * TODO: Use exact per-symbol CP lengths from the PHY for production.
 */
static uint64_t compute_spi_timestamp(const beam_spi_entry_t *e)
{
    /* Approximate samples per symbol (including CP) */
    uint64_t samples_per_symbol = g_ctx.samples_per_slot / g_ctx.symbols_per_slot;

    uint64_t ts = (uint64_t)e->frame * g_ctx.samples_per_frame
                + (uint64_t)e->slot  * g_ctx.samples_per_slot
                + (uint64_t)e->symbol * samples_per_symbol;
    return ts;
}

/* ── Duration collision check ────────────────────────────────────────── */

/*
 * Validate that consecutive beam switch entries have sufficient timing gap.
 *
 * If two set_command_time calls are issued within spi_duration_samples of
 * each other, the second overwrites the first before its Phase register
 * write completes, causing the Phase data to be dropped.
 *
 * Entries are expected to arrive in symbol-ascending order from the producer.
 */
static bool validate_batch_timing(const beam_spi_slot_batch_t *batch,
                                  uint64_t *timestamps)
{
    for (int i = 0; i < batch->n_entries; i++)
        timestamps[i] = compute_spi_timestamp(&batch->entries[i]);

    bool ok = true;
    for (int i = 1; i < batch->n_entries; i++) {
        int64_t gap = (int64_t)(timestamps[i] - timestamps[i-1]);
        if (gap < (int64_t)g_ctx.spi_duration_samples) {
            LOG_W(PHY, "[BeamSPI] Duration collision: "
                  "entry[%d] sym=%d beam=%d → entry[%d] sym=%d beam=%d  "
                  "gap=%ld samples (need >= %lu)\n",
                  i-1, batch->entries[i-1].symbol, batch->entries[i-1].beam_id,
                  i,   batch->entries[i].symbol,   batch->entries[i].beam_id,
                  (long)gap, (unsigned long)g_ctx.spi_duration_samples);
            ok = false;
        }
    }
    return ok;
}

/* ── SPI execution (1 entry = Gain + Phase pair for one direction) ──── */

static void execute_spi_entry(const beam_spi_entry_t *e, uint64_t ts)
{
    int mode = (e->direction == BEAM_SPI_TX) ? 0 : 1;  /* MODE_TX=0, MODE_RX=1 */
    uint32_t payload_id = ((e->beam_id - 1) & 0x3F);

    uhd::time_spec_t time_spec = uhd::time_spec_t::from_ticks((double)ts, g_ctx.sample_rate);

    g_ctx.usrp->set_command_time(time_spec);

    /* Gain register */
    uint32_t gain_payload = (tmy_reg_address[mode][0] << 6) | payload_id;
    g_ctx.spi_ref->write_spi(0, g_ctx.spi_config, gain_payload, tmy_payload_bits);
    g_ctx.usrp->set_gpio_attr(g_ctx.gpio_bank, "OUT", 0, GPIO_BIT(GPIO_DEFAULT_LDB_PIN));
    g_ctx.usrp->set_gpio_attr(g_ctx.gpio_bank, "OUT",
                               GPIO_BIT(GPIO_DEFAULT_LDB_PIN),
                               GPIO_BIT(GPIO_DEFAULT_LDB_PIN));

    /* Phase register */
    uint32_t phase_payload = (tmy_reg_address[mode][1] << 6) | payload_id;
    g_ctx.spi_ref->write_spi(0, g_ctx.spi_config, phase_payload, tmy_payload_bits);
    g_ctx.usrp->set_gpio_attr(g_ctx.gpio_bank, "OUT", 0, GPIO_BIT(GPIO_DEFAULT_LDB_PIN));
    g_ctx.usrp->set_gpio_attr(g_ctx.gpio_bank, "OUT",
                               GPIO_BIT(GPIO_DEFAULT_LDB_PIN),
                               GPIO_BIT(GPIO_DEFAULT_LDB_PIN));

    g_ctx.usrp->clear_command_time();
}

/* ── Worker main loop ────────────────────────────────────────────────── */

static void *beam_spi_worker(void *arg)
{
    (void)arg;
    beam_spi_slot_batch_t *batch = NULL;

    pthread_setname_np(pthread_self(), "beam_spi");
    LOG_I(PHY, "[BeamSPI] Worker thread started (sample_rate=%.2f MHz, "
          "spi_duration=%lu samples)\n",
          g_ctx.sample_rate / 1e6, (unsigned long)g_ctx.spi_duration_samples);

    while (pbq_pop(g_ctx.queue, (void **)&batch)) {
        uint64_t timestamps[BEAM_SPI_MAX_ENTRIES_PER_SLOT];

        /* 1. Duration collision check */
        bool timing_ok = validate_batch_timing(batch, timestamps);
        if (!timing_ok) {
            LOG_E(PHY, "[BeamSPI] f=%d s=%d has duration collisions — "
                  "some Phase registers may be dropped\n",
                  batch->entries[0].frame, batch->entries[0].slot);
        }

        /* 2. Execute SPI in symbol order */
        for (int i = 0; i < batch->n_entries; i++) {
            execute_spi_entry(&batch->entries[i], timestamps[i]);

            LOG_D(PHY, "[BeamSPI] f=%d s=%d sym=%d %s beam=%d ts=%lu\n",
                  batch->entries[i].frame, batch->entries[i].slot,
                  batch->entries[i].symbol,
                  batch->entries[i].direction == BEAM_SPI_TX ? "TX" : "RX",
                  batch->entries[i].beam_id,
                  (unsigned long)timestamps[i]);
        }

        free(batch);
    }

    LOG_I(PHY, "[BeamSPI] Worker thread exiting\n");
    return NULL;
}

/* ── Public API (C linkage) ──────────────────────────────────────────── */

extern "C" {

int beam_spi_thread_start(void *usrp_device, double sample_rate,
                          int slots_per_frame, int symbols_per_slot)
{
    if (g_ctx.running) {
        LOG_W(PHY, "[BeamSPI] Thread already running\n");
        return 0;
    }

    memset(&g_ctx, 0, sizeof(g_ctx));

    /* Cast opaque pointer back to UHD shared_ptr.
     * The caller (usrp_lib.cpp) passes &(s->usrp) where s->usrp is multi_usrp::sptr. */
    uhd::usrp::multi_usrp::sptr *usrp_ptr = (uhd::usrp::multi_usrp::sptr *)usrp_device;
    g_ctx.usrp = *usrp_ptr;

    if (!g_ctx.usrp) {
        LOG_E(PHY, "[BeamSPI] NULL USRP device\n");
        return -1;
    }

    /* Obtain SPI reference (must have been set up by usrp_spi_setup already) */
    extern uhd::spi_iface::sptr spi_ref;
    extern uhd::spi_config_t spi_config;
    extern std::string gpio_bank;

    if (!spi_ref) {
        LOG_E(PHY, "[BeamSPI] SPI not initialized — call usrp_spi_setup first\n");
        return -1;
    }

    g_ctx.spi_ref    = spi_ref;
    g_ctx.spi_config = spi_config;
    g_ctx.gpio_bank  = gpio_bank;

    /* Timing parameters */
    g_ctx.sample_rate       = sample_rate;
    g_ctx.slots_per_frame   = slots_per_frame;
    g_ctx.symbols_per_slot  = symbols_per_slot;
    g_ctx.samples_per_slot  = (uint64_t)(sample_rate * 1e-3 / (slots_per_frame / 10.0));
    g_ctx.samples_per_frame = (uint64_t)(sample_rate * 10e-3);  /* 10 ms frame */

    /* SPI duration: ~3.3 μs = set_command_time delay (2μs) + Gain+Phase pair (1.3μs) */
    g_ctx.spi_duration_samples = (uint64_t)(3.3e-6 * sample_rate) + 1;

    /* Create queue */
    g_ctx.queue = pbq_create(BEAM_SPI_QUEUE_CAPACITY);
    if (!g_ctx.queue) {
        LOG_E(PHY, "[BeamSPI] Failed to create queue\n");
        return -1;
    }

    /* Spawn worker thread */
    int ret = pthread_create(&g_ctx.thread, NULL, beam_spi_worker, NULL);
    if (ret != 0) {
        LOG_E(PHY, "[BeamSPI] Failed to create thread: %s\n", strerror(ret));
        pbq_destroy(g_ctx.queue);
        g_ctx.queue = NULL;
        return -1;
    }

    g_ctx.running = true;
    LOG_I(PHY, "[BeamSPI] Thread started (queue_capacity=%d)\n", BEAM_SPI_QUEUE_CAPACITY);
    return 0;
}

void beam_spi_thread_stop(void)
{
    if (!g_ctx.running)
        return;

    LOG_I(PHY, "[BeamSPI] Stopping worker thread...\n");

    pbq_shutdown(g_ctx.queue);
    pthread_join(g_ctx.thread, NULL);
    pbq_destroy(g_ctx.queue);

    g_ctx.queue = NULL;
    g_ctx.running = false;

    LOG_I(PHY, "[BeamSPI] Thread stopped\n");
}

bool beam_spi_enqueue(beam_spi_slot_batch_t *batch)
{
    if (!g_ctx.queue || !g_ctx.running)
        return false;
    return pbq_try_push(g_ctx.queue, batch);
}

} /* extern "C" */
