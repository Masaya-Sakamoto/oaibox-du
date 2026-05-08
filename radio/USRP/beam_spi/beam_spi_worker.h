/*
 * beam_spi_worker.h — C linkage API for the beam SPI worker thread
 *
 * The worker thread consumes beam_spi_slot_batch_t items from the queue,
 * validates timing constraints, and executes timed SPI commands via UHD.
 */

#ifndef BEAM_SPI_WORKER_H
#define BEAM_SPI_WORKER_H

#include "beam_spi_task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the beam SPI worker thread.
 *
 * Initializes the blocking queue and spawns the worker pthread.
 * Must be called after USRP and SPI have been initialized (usrp_spi_setup).
 *
 * @param usrp_device  Opaque pointer to uhd::usrp::multi_usrp::sptr (cast internally)
 * @param sample_rate  Sample rate in Hz (e.g. 122.88e6 for FR2 μ=3)
 * @param slots_per_frame  Number of slots per frame
 * @param symbols_per_slot Number of symbols per slot (typically 14)
 * @return 0 on success, -1 on failure
 */
int beam_spi_thread_start(void *usrp_device, double sample_rate,
                          int slots_per_frame, int symbols_per_slot);

/**
 * @brief Stop the beam SPI worker thread.
 *
 * Shuts down the queue (unblocks pop), joins the thread, and frees resources.
 */
void beam_spi_thread_stop(void);

/**
 * @brief Enqueue a batch of beam switch entries (non-blocking).
 *
 * Called from the MAC scheduler (RT context). If the queue is full, the batch
 * is dropped and a warning is logged.
 *
 * @param batch  Heap-allocated batch. Ownership transfers to the queue on success.
 * @return true on success, false if queue is full or shut down (caller must free).
 */
bool beam_spi_enqueue(beam_spi_slot_batch_t *batch);

#ifdef __cplusplus
}
#endif

#endif /* BEAM_SPI_WORKER_H */
