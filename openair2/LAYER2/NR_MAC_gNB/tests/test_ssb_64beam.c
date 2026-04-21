/*
 * Unit tests for 64-beam SSB scheduling in FR2 (band 257, 120kHz SCS).
 *
 * Self-contained: duplicates the minimal set of types and functions from the
 * OAI MAC layer.
 *
 * Tests verify:
 *   1) longBitmap with all 64 bits set → 64 SSBs are valid
 *   2) fill_beam_index_list() identity mapping: SSB[i] → beam[i]
 *   3) beam_allocation_procedure() handles 64 distinct beam IDs
 *   4) FR2 slot counts: 80 slots/frame @ 120kHz SCS
 *   5) Symbol-level allocation with beam_duration=-7 and 64 beams
 *
 * Build (standalone):
 *   gcc -o test_ssb_64beam test_ssb_64beam.c -Wall -Wextra && ./test_ssb_64beam
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/*  Minimal type duplicates                                                  */
/* ========================================================================= */

#define NR_SYMBOLS_PER_SLOT 14
#define MAX_NUM_OF_SSB 64
#define IS_BIT_SET(a, b) (((a) >> (b)) & 1)
#define LOG_D(x, ...) do {} while (0)

/* FR2 120kHz SCS → 80 slots/frame */
#define FR2_120KHZ_SLOTS_PER_FRAME 80

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

/* ========================================================================= */
/*  Functions under test (copied from test_beam_symbol_alloc.c / primitives) */
/* ========================================================================= */

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
      return beam_struct;
    }
  }
  return (NR_beam_alloc_t){.new_beam = 0, .idx = -1};
}

/* Simplified fill_beam_index_list for identity mapping */
static void fill_beam_index_list_identity(int *beam_index_list, uint64_t ssb_bitmap, int len)
{
  int index = 0;
  for (int i = 0; i < len; i++) {
    if (IS_BIT_SET(ssb_bitmap, (63 - i))) {
      beam_index_list[i] = index;
      index++;
    } else {
      beam_index_list[i] = -1;
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

/* ========================================================================= */
/*  Test infrastructure                                                      */
/* ========================================================================= */

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

#define ASSERT_GE(a, b) \
  do { \
    if ((a) < (b)) { \
      printf("  [FAIL] %s (line %d): expected >= %d, got %d\n", _test_name, __LINE__, (int)(b), (int)(a)); \
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

/* ========================================================================= */
/*  Test cases                                                               */
/* ========================================================================= */

/* TC-C1: longBitmap 0xFFFF_FFFF_FFFF_FFFF has all 64 bits → 64 valid SSBs */
static void test_64_ssb_bitmap(void)
{
  TEST_START("64_ssb_all_valid");

  uint64_t bitmap = 0xFFFFFFFFFFFFFFFFULL;
  int count = 0;
  for (int i = 0; i < 64; i++) {
    if (IS_BIT_SET(bitmap, (63 - i)))
      count++;
  }
  ASSERT_EQ(count, 64);

  TEST_END();
}

/* TC-C2: fill_beam_index_list identity mapping: SSB[i] → beam[i] */
static void test_identity_beam_mapping(void)
{
  TEST_START("identity_beam_mapping_64ssb");

  int beam_index_list[MAX_NUM_OF_SSB];
  uint64_t bitmap = 0xFFFFFFFFFFFFFFFFULL;

  fill_beam_index_list_identity(beam_index_list, bitmap, 64);

  for (int i = 0; i < 64; i++) {
    ASSERT_EQ(beam_index_list[i], i);
  }

  TEST_END();
}

/* TC-C3: Partial bitmap — only even SSBs active */
static void test_partial_bitmap_mapping(void)
{
  TEST_START("partial_bitmap_even_ssbs");

  int beam_index_list[MAX_NUM_OF_SSB];
  uint64_t bitmap = 0xAAAAAAAAAAAAAAAAULL; /* bits 63,61,59,...,1 set */

  fill_beam_index_list_identity(beam_index_list, bitmap, 64);

  /* 32 beams should be assigned (indices 0..31), alternating with -1 */
  int active_count = 0;
  for (int i = 0; i < 64; i++) {
    if (beam_index_list[i] >= 0)
      active_count++;
  }
  ASSERT_EQ(active_count, 32);

  TEST_END();
}

/* TC-C4: FR2 slot count: 120kHz SCS → 80 slots/frame */
static void test_fr2_slot_count(void)
{
  TEST_START("fr2_120khz_80_slots_per_frame");

  /* 120kHz SCS: 2^(120/15) = 2^3 = 8 slots per 1ms subframe, 10 subframes = 80 */
  int scs_index = 3; /* mu=3 for 120kHz */
  int slots_per_subframe = 1 << scs_index;
  int slots_per_frame = slots_per_subframe * 10;

  ASSERT_EQ(slots_per_frame, FR2_120KHZ_SLOTS_PER_FRAME);

  TEST_END();
}

/* TC-C5: beam_allocation_procedure with 64 beams in different slots */
static void test_64_beam_allocation_different_slots(void)
{
  TEST_START("64_beam_alloc_different_slots");

  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 1, FR2_120KHZ_SLOTS_PER_FRAME);

  /* Allocate 64 different beams, each in a different slot (frame 0) */
  for (int beam = 0; beam < 64; beam++) {
    int slot = beam % FR2_120KHZ_SLOTS_PER_FRAME;
    NR_beam_alloc_t result = beam_allocation_procedure(&bi, 0, slot, 0, 7, beam, FR2_120KHZ_SLOTS_PER_FRAME);
    ASSERT_GE(result.idx, 0);
  }

  free_beam_info(&bi);
  TEST_END();
}

/* TC-C6: Symbol-level allocation with beam_duration=-7, 2 symbol groups.
 * With beams_per_period=1, the allocator checks conflicts per symbol group.
 * Two beams in DIFFERENT symbol groups of the same slot can coexist in the
 * same beam period because each group is checked independently. */
static void test_symbol_level_two_groups(void)
{
  TEST_START("symbol_level_beam_duration_neg7_fr2");

  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 1, FR2_120KHZ_SLOTS_PER_FRAME);

  /* beam_allocation_size[1] = 14/7 = 2 symbol groups */
  ASSERT_EQ(bi.beam_allocation_size[1], 2);

  /* Same slot, symbol group 0 (symbols 0-6): beam 0 */
  NR_beam_alloc_t r0 = beam_allocation_procedure(&bi, 0, 0, 0, 4, 0, FR2_120KHZ_SLOTS_PER_FRAME);
  ASSERT_GE(r0.idx, 0);

  /* Same slot, symbol group 1 (symbols 7-13): different beam 1
   * This succeeds because group 1 is independently checked (slot[1] == -1). */
  NR_beam_alloc_t r1 = beam_allocation_procedure(&bi, 0, 0, 7, 4, 1, FR2_120KHZ_SLOTS_PER_FRAME);
  ASSERT_GE(r1.idx, 0);

  /* Same slot, same symbol group 0: beam 2 — MUST fail (conflict with beam 0) */
  NR_beam_alloc_t r2 = beam_allocation_procedure(&bi, 0, 0, 0, 4, 2, FR2_120KHZ_SLOTS_PER_FRAME);
  ASSERT_EQ(r2.idx, -1);

  free_beam_info(&bi);
  TEST_END();
}

/* TC-C7: With beams_per_period=2, two beams can coexist in same slot */
static void test_symbol_level_two_beams_per_period(void)
{
  TEST_START("symbol_level_two_beams_per_period_fr2");

  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 2, FR2_120KHZ_SLOTS_PER_FRAME);

  /* Symbol group 0: beam 0 */
  NR_beam_alloc_t r0 = beam_allocation_procedure(&bi, 0, 0, 0, 7, 0, FR2_120KHZ_SLOTS_PER_FRAME);
  ASSERT_EQ(r0.idx, 0);

  /* Symbol group 1: beam 1 — should succeed in beam period idx=1 */
  NR_beam_alloc_t r1 = beam_allocation_procedure(&bi, 0, 0, 7, 7, 1, FR2_120KHZ_SLOTS_PER_FRAME);
  ASSERT_GE(r1.idx, 0);

  free_beam_info(&bi);
  TEST_END();
}

/* TC-C8: Table size for FR2: 80 slots × 2 frames = 160 entries */
static void test_beam_table_size_fr2(void)
{
  TEST_START("beam_table_size_fr2_160_entries");

  NR_beam_info_t bi;
  init_beam_info(&bi, -7, 1, FR2_120KHZ_SLOTS_PER_FRAME);

  /* beam_allocation_size[0] = (80 * 2) / 1 = 160 */
  ASSERT_EQ(bi.beam_allocation_size[0], 160);

  free_beam_info(&bi);
  TEST_END();
}

/* ========================================================================= */
/*  Main                                                                     */
/* ========================================================================= */

int main(void)
{
  printf("=== 64-Beam SSB Scheduling Tests (FR2 band257) ===\n\n");

  test_64_ssb_bitmap();
  test_identity_beam_mapping();
  test_partial_bitmap_mapping();
  test_fr2_slot_count();
  test_64_beam_allocation_different_slots();
  test_symbol_level_two_groups();
  test_symbol_level_two_beams_per_period();
  test_beam_table_size_fr2();

  printf("\n--- Results: %d run, %d passed, %d failed ---\n",
         tests_run, tests_passed, tests_failed);

  return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
