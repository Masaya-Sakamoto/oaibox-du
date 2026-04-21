/*
 * Unit tests for symbol-level beam allocation and reset functions.
 *
 * This file is self-contained so it can be compiled without the full OAI
 * build graph. It intentionally duplicates the small subset of definitions
 * needed to validate the new allocation semantics.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NR_SYMBOLS_PER_SLOT 14
#define IS_BIT_SET(a, b) (((a) >> (b)) & 1)
#define LOG_D(x, ...) do {} while (0)

typedef enum {
  NO_BEAM_MODE,
  PRECONFIGURED_BEAM_IDX,
  LOPHY_BEAM_IDX,
} nr_beam_mode_t;

typedef struct {
  int idx;
  uint16_t new_beam;
} NR_beam_alloc_t;

typedef struct {
  int16_t ***beam_allocation;
  int beam_slot_duration;
  int beam_symbol_duration;
  int beams_per_period;
  int beam_allocation_size[2];
  nr_beam_mode_t beam_mode;
} NR_beam_info_t;

static inline int get_beam_allocation_slot_index(const NR_beam_info_t *beam_info, int frame, int slot, int slots_per_frame)
{
  return ((frame * slots_per_frame + slot) / beam_info->beam_slot_duration) % beam_info->beam_allocation_size[0];
}

static NR_beam_alloc_t beam_allocation_procedure(NR_beam_info_t *beam_info,
                                                 int frame,
                                                 int slot,
                                                 int start_symbol,
                                                 int nb_symbols,
                                                 int16_t beam_index,
                                                 int slots_per_frame)
{
  if (beam_info->beam_mode == NO_BEAM_MODE)
    return (NR_beam_alloc_t){.new_beam = 0, .idx = 0};

  const int index = get_beam_allocation_slot_index(beam_info, frame, slot, slots_per_frame);
  int alloc_start = 0;
  int alloc_end = 0;
  if (beam_info->beam_allocation_size[1] > 1) {
    int step_size = NR_SYMBOLS_PER_SLOT / beam_info->beam_allocation_size[1];
    alloc_start = start_symbol / step_size;
    alloc_end = (start_symbol + nb_symbols - 1) / step_size;
  }

  for (int i = 0; i < beam_info->beams_per_period; i++) {
    bool beam_found = true;
    uint16_t new_beam = 0;
    for (int j = alloc_start; j <= alloc_end; j++) {
      int16_t beam = beam_info->beam_allocation[i][index][j];
      if (beam != -1 && beam != beam_index) {
        beam_found = false;
        break;
      } else if (beam == -1) {
        new_beam |= (1U << j);
      }
    }
    if (beam_found) {
      NR_beam_alloc_t beam_struct = {.new_beam = new_beam, .idx = i};
      for (int j = alloc_start; j <= alloc_end; j++)
        beam_info->beam_allocation[i][index][j] = beam_index;
      LOG_D(NR_MAC,
            "%d.%d Using beam structure with index %d for beam %d (%s)\n",
            frame,
            slot,
            beam_struct.idx,
            beam_index,
            beam_struct.new_beam ? "new beam" : "old beam");
      return beam_struct;
    }
  }
  return (NR_beam_alloc_t){.new_beam = 0, .idx = -1};
}

static void reset_beam_status(NR_beam_info_t *beam_info,
                              int frame,
                              int slot,
                              int16_t beam_index,
                              int slots_per_frame,
                              uint16_t beam_alloc)
{
  if (beam_alloc == 0)
    return;

  const int index = get_beam_allocation_slot_index(beam_info, frame, slot, slots_per_frame);
  for (int i = 0; i < beam_info->beams_per_period; i++) {
    for (int j = 0; j < NR_SYMBOLS_PER_SLOT; j++) {
      if (IS_BIT_SET(beam_alloc, j) && beam_info->beam_allocation[i][index][j] == beam_index)
        beam_info->beam_allocation[i][index][j] = -1;
    }
  }
}

static void init_beam_info(NR_beam_info_t *bi, int beam_duration, int beams_per_period, int slots_per_frame)
{
  bi->beam_mode = PRECONFIGURED_BEAM_IDX;
  bi->beam_slot_duration = beam_duration > 0 ? beam_duration : 1;
  bi->beam_symbol_duration = beam_duration < 0 ? -beam_duration : 0;
  bi->beams_per_period = beams_per_period;

  int size = slots_per_frame * 2;
  bi->beam_allocation_size[0] = size / bi->beam_slot_duration;

  int symb_dur = bi->beam_symbol_duration ? bi->beam_symbol_duration : NR_SYMBOLS_PER_SLOT;
  bi->beam_allocation_size[1] = NR_SYMBOLS_PER_SLOT / symb_dur;

  bi->beam_allocation = malloc(beams_per_period * sizeof(int16_t **));
  for (int i = 0; i < beams_per_period; i++) {
    bi->beam_allocation[i] = malloc(bi->beam_allocation_size[0] * sizeof(int16_t *));
    for (int j = 0; j < bi->beam_allocation_size[0]; j++) {
      bi->beam_allocation[i][j] = malloc(bi->beam_allocation_size[1] * sizeof(int16_t));
      for (int k = 0; k < bi->beam_allocation_size[1]; k++)
        bi->beam_allocation[i][j][k] = -1;
    }
  }
}

static void free_beam_info(NR_beam_info_t *bi)
{
  for (int i = 0; i < bi->beams_per_period; i++) {
    for (int j = 0; j < bi->beam_allocation_size[0]; j++)
      free(bi->beam_allocation[i][j]);
    free(bi->beam_allocation[i]);
  }
  free(bi->beam_allocation);
}

static int tests_run;
static int tests_passed;
static int tests_failed;

#define TEST_START(name) \
  do { \
    tests_run++; \
    const char *_test_name = name; \
    printf("  [RUN ] %s\n", _test_name);

#define TEST_END() \
    tests_passed++; \
    printf("  [PASS] %s\n", _test_name); \
  } while (0)

#define ASSERT_EQ(a, b) \
  do { \
    if ((a) != (b)) { \
      printf("  [FAIL] %s (line %d): expected %d, got %d\n", _test_name, __LINE__, (int)(b), (int)(a)); \
      tests_failed++; \
      return; \
    } \
  } while (0)

#define ASSERT_TRUE(cond) \
  do { \
    if (!(cond)) { \
      printf("  [FAIL] %s (line %d): condition not met\n", _test_name, __LINE__); \
      tests_failed++; \
      return; \
    } \
  } while (0)

#define ASSERT_GE(a, b) \
  do { \
    if ((a) < (b)) { \
      printf("  [FAIL] %s (line %d): expected >= %d, got %d\n", _test_name, __LINE__, (int)(b), (int)(a)); \
      tests_failed++; \
      return; \
    } \
  } while (0)

static void test_slot_mode_basic(void)
{
  TEST_START("TC-1: test_slot_mode_basic");
  NR_beam_info_t bi;
  init_beam_info(&bi, 1, 1, 10);

  NR_beam_alloc_t res = beam_allocation_procedure(&bi, 0, 0, 0, 14, 5, 10);
  ASSERT_GE(res.idx, 0);
  ASSERT_TRUE(res.new_beam != 0);

  NR_beam_alloc_t res2 = beam_allocation_procedure(&bi, 0, 0, 0, 14, 5, 10);
  ASSERT_GE(res2.idx, 0);
  ASSERT_EQ(res2.new_beam, 0);

  NR_beam_alloc_t res3 = beam_allocation_procedure(&bi, 0, 0, 0, 14, 10, 10);
  ASSERT_EQ(res3.idx, -1);

  free_beam_info(&bi);
  TEST_END();
}

static void test_symbol_mode_basic(void)
{
  TEST_START("TC-2: test_symbol_mode_basic");
  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 1, 10);

  ASSERT_EQ(bi.beam_allocation_size[1], 2);

  NR_beam_alloc_t res = beam_allocation_procedure(&bi, 0, 0, 0, 7, 5, 10);
  ASSERT_GE(res.idx, 0);
  ASSERT_TRUE(res.new_beam != 0);

  free_beam_info(&bi);
  TEST_END();
}

static void test_symbol_mode_no_conflict(void)
{
  TEST_START("TC-3: test_symbol_mode_no_conflict");
  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 2, 10);

  NR_beam_alloc_t res1 = beam_allocation_procedure(&bi, 0, 0, 0, 7, 5, 10);
  ASSERT_GE(res1.idx, 0);

  NR_beam_alloc_t res2 = beam_allocation_procedure(&bi, 0, 0, 7, 7, 10, 10);
  ASSERT_GE(res2.idx, 0);

  free_beam_info(&bi);
  TEST_END();
}

static void test_symbol_mode_conflict(void)
{
  TEST_START("TC-4: test_symbol_mode_conflict");
  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 1, 10);

  NR_beam_alloc_t res1 = beam_allocation_procedure(&bi, 0, 0, 0, 7, 5, 10);
  ASSERT_GE(res1.idx, 0);

  NR_beam_alloc_t res2 = beam_allocation_procedure(&bi, 0, 0, 0, 7, 10, 10);
  ASSERT_EQ(res2.idx, -1);

  free_beam_info(&bi);
  TEST_END();
}

static void test_reset_symbol_bitmap(void)
{
  TEST_START("TC-5: test_reset_symbol_bitmap");
  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 1, 10);

  NR_beam_alloc_t res1 = beam_allocation_procedure(&bi, 0, 0, 0, 7, 5, 10);
  ASSERT_GE(res1.idx, 0);
  uint16_t saved_new_beam = res1.new_beam;

  NR_beam_alloc_t res2 = beam_allocation_procedure(&bi, 0, 0, 7, 7, 5, 10);
  ASSERT_GE(res2.idx, 0);

  reset_beam_status(&bi, 0, 0, 5, 10, saved_new_beam);

  NR_beam_alloc_t res3 = beam_allocation_procedure(&bi, 0, 0, 0, 7, 99, 10);
  ASSERT_GE(res3.idx, 0);

  NR_beam_alloc_t res4 = beam_allocation_procedure(&bi, 0, 0, 7, 7, 99, 10);
  ASSERT_EQ(res4.idx, -1);

  free_beam_info(&bi);
  TEST_END();
}

static void test_dci_pdsch_separate_beams(void)
{
  TEST_START("TC-6: test_dci_pdsch_separate_beams");
  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 2, 10);

  NR_beam_alloc_t dci = beam_allocation_procedure(&bi, 0, 0, 0, 3, 1, 10);
  ASSERT_GE(dci.idx, 0);

  NR_beam_alloc_t pdsch = beam_allocation_procedure(&bi, 0, 0, 3, 11, 2, 10);
  ASSERT_GE(pdsch.idx, 0);

  free_beam_info(&bi);
  TEST_END();
}

static void test_no_beam_mode(void)
{
  TEST_START("TC-7: test_no_beam_mode");
  NR_beam_info_t bi;
  memset(&bi, 0, sizeof(bi));
  bi.beam_mode = NO_BEAM_MODE;

  NR_beam_alloc_t res = beam_allocation_procedure(&bi, 0, 0, 0, 14, 99, 10);
  ASSERT_EQ(res.idx, 0);
  ASSERT_EQ(res.new_beam, 0);

  TEST_END();
}

/* ========================================================================= */
/*  FR2 64-Beam Test Cases (band257, 120kHz SCS, 80 slots/frame)             */
/* ========================================================================= */

#define FR2_SLOTS_PER_FRAME 80

static void test_fr2_64beam_sequential_slots(void)
{
  TEST_START("TC-8: fr2_64beam_sequential_slot_allocation");
  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 1, FR2_SLOTS_PER_FRAME);

  /* Allocate 64 different beams, each in a unique slot within frame 0.
   * With 80 slots available, all 64 should succeed without conflict. */
  for (int beam = 0; beam < 64; beam++) {
    int slot = beam % FR2_SLOTS_PER_FRAME;
    NR_beam_alloc_t res = beam_allocation_procedure(&bi, 0, slot, 0, 7, beam, FR2_SLOTS_PER_FRAME);
    ASSERT_GE(res.idx, 0);
  }

  free_beam_info(&bi);
  TEST_END();
}

static void test_fr2_table_sizing(void)
{
  TEST_START("TC-9: fr2_beam_table_size_160_entries");
  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 1, FR2_SLOTS_PER_FRAME);

  /* 80 slots/frame × 2 frames / beam_slot_duration(1) = 160 */
  ASSERT_EQ(bi.beam_allocation_size[0], 160);
  /* 14 symbols / 7 = 2 symbol groups */
  ASSERT_EQ(bi.beam_allocation_size[1], 2);

  free_beam_info(&bi);
  TEST_END();
}

static void test_fr2_64beam_round_robin(void)
{
  TEST_START("TC-10: fr2_64beam_round_robin_two_frames");
  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 1, FR2_SLOTS_PER_FRAME);

  /* Simulate SSB-like round-robin: beam i in slot i (mod 80).
   * Allocate across frames 0 and 1 to fill the entire table. */
  int alloc_count = 0;
  for (int frame = 0; frame < 2; frame++) {
    for (int slot = 0; slot < FR2_SLOTS_PER_FRAME; slot++) {
      int beam = (frame * FR2_SLOTS_PER_FRAME + slot) % 64;
      /* Symbol group 0 (symbols 0-6) */
      NR_beam_alloc_t res = beam_allocation_procedure(&bi, frame, slot, 0, 7, beam, FR2_SLOTS_PER_FRAME);
      ASSERT_GE(res.idx, 0);
      alloc_count++;
    }
  }
  ASSERT_EQ(alloc_count, 160);

  free_beam_info(&bi);
  TEST_END();
}

static void test_fr2_64beam_reset_all(void)
{
  TEST_START("TC-11: fr2_64beam_full_reset");
  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 1, FR2_SLOTS_PER_FRAME);

  /* Allocate 64 beams */
  uint16_t bitmaps[64];
  for (int beam = 0; beam < 64; beam++) {
    NR_beam_alloc_t res = beam_allocation_procedure(&bi, 0, beam, 0, 7, beam, FR2_SLOTS_PER_FRAME);
    ASSERT_GE(res.idx, 0);
    bitmaps[beam] = res.new_beam;
  }

  /* Reset all 64 beams */
  for (int beam = 0; beam < 64; beam++) {
    reset_beam_status(&bi, 0, beam, beam, FR2_SLOTS_PER_FRAME, bitmaps[beam]);
  }

  /* Re-allocate should succeed (table is clear) */
  for (int beam = 0; beam < 64; beam++) {
    NR_beam_alloc_t res = beam_allocation_procedure(&bi, 0, beam, 0, 7, beam + 100, FR2_SLOTS_PER_FRAME);
    ASSERT_GE(res.idx, 0);
    ASSERT_TRUE(res.new_beam != 0);
  }

  free_beam_info(&bi);
  TEST_END();
}

int main(void)
{
  printf("=== Symbol-Level Beam Allocation Unit Tests ===\n\n");

  test_slot_mode_basic();
  test_symbol_mode_basic();
  test_symbol_mode_no_conflict();
  test_symbol_mode_conflict();
  test_reset_symbol_bitmap();
  test_dci_pdsch_separate_beams();
  test_no_beam_mode();

  /* FR2 64-beam tests */
  test_fr2_64beam_sequential_slots();
  test_fr2_table_sizing();
  test_fr2_64beam_round_robin();
  test_fr2_64beam_reset_all();

  printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
  if (tests_failed > 0)
    printf(", %d FAILED", tests_failed);
  printf(" ===\n");

  return tests_failed > 0 ? 1 : 0;
}
