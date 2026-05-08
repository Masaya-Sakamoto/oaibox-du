/*
 * beam_spi_task.h — Data structures for symbol-level beam SPI scheduling
 *
 * Shared between MAC layer (producer) and USRP layer (consumer).
 * Placed in radio/COMMON so both layers can include it without cross-dependency.
 */

#ifndef BEAM_SPI_TASK_H
#define BEAM_SPI_TASK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Beam switch direction — determined by signal type (DL → TX, UL → RX) */
#define BEAM_SPI_TX 0
#define BEAM_SPI_RX 1

/* A single beam switch entry */
typedef struct {
    int frame;
    int slot;
    int symbol;             /* switch start symbol (0-13) */
    int16_t beam_id;        /* BBox beam ID (1-64) */
    uint8_t direction;      /* BEAM_SPI_TX or BEAM_SPI_RX */
} beam_spi_entry_t;

/* Maximum entries per slot — one antenna does TX or RX, never both simultaneously */
#define BEAM_SPI_MAX_ENTRIES_PER_SLOT 14

/* Per-slot batch of beam switch entries */
typedef struct {
    int n_entries;
    beam_spi_entry_t entries[BEAM_SPI_MAX_ENTRIES_PER_SLOT];
} beam_spi_slot_batch_t;

/* Queue capacity — number of slot batches that can be buffered */
#define BEAM_SPI_QUEUE_CAPACITY 64

/*
 * Callback type for enqueuing a batch into the SPI worker.
 * Returns true on success, false if queue full (caller must free batch).
 * The batch is heap-allocated; ownership transfers to the callee on success.
 */
typedef bool (*beam_spi_enqueue_fn_t)(beam_spi_slot_batch_t *batch);

#ifdef __cplusplus
}
#endif

#endif /* BEAM_SPI_TASK_H */
