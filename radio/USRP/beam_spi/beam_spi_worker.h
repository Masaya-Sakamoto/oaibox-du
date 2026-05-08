/*
 * beam_spi_worker.h — C linkage API for the beam SPI worker thread
 *
 * The worker thread consumes beam_spi_slot_batch_t items from the queue,
 * validates timing constraints, and executes timed SPI commands via UHD.
 */

#ifndef BEAM_SPI_WORKER_H
#define BEAM_SPI_WORKER_H

#include "radio/COMMON/beam_spi_task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the beam SPI worker thread.
 *
 * Initializes the blocking queue, spawns the worker pthread, and registers
 * the enqueue callback with the MAC layer.
 *
 * @param usrp_device  Opaque pointer to uhd::usrp::multi_usrp::sptr
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
 * Unregisters the enqueue callback, shuts down the queue, joins the thread.
 */
void beam_spi_thread_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BEAM_SPI_WORKER_H */
