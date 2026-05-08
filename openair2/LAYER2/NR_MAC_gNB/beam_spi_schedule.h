/*
 * beam_spi_schedule.h — Producer API for beam SPI scheduling
 *
 * Called from gNB_dlsch_ulsch_scheduler() after all sub-schedulers have run.
 * Scans the beam_allocation table to extract beam switch entries for the
 * current slot and enqueues them via a registered callback.
 *
 * The callback is registered at runtime by the USRP SPI worker thread.
 * When no callback is registered, beam_spi_schedule_slot() is a no-op.
 */

#ifndef BEAM_SPI_SCHEDULE_H
#define BEAM_SPI_SCHEDULE_H

#include "NR_MAC_gNB/nr_mac_gNB.h"
#include "radio/COMMON/beam_spi_task.h"

/**
 * @brief Register the enqueue callback for beam SPI batches.
 *
 * Called by the SPI worker thread at startup to connect the producer
 * (MAC scheduler) to the consumer (SPI worker queue).
 * Pass NULL to unregister (e.g. at shutdown).
 *
 * @param fn  Enqueue function pointer, or NULL to disable.
 */
void beam_spi_set_enqueue_fn(beam_spi_enqueue_fn_t fn);

/**
 * @brief Extract beam schedule from beam_allocation table and enqueue SPI batch.
 *
 * Scans the beam_allocation table for the given slot, identifies symbols where
 * the beam changes (first symbol + each transition), determines direction
 * (TX or RX) from the TDD slot structure, and enqueues a batch via the
 * registered callback.
 *
 * No-op when no enqueue callback is registered.
 *
 * @param gNB   MAC instance (provides beam_info, frame_structure)
 * @param frame Current frame number
 * @param slot  Current slot number
 */
void beam_spi_schedule_slot(gNB_MAC_INST *gNB, frame_t frame, slot_t slot);

#endif /* BEAM_SPI_SCHEDULE_H */
