/*
 * Unit tests for antenna control interface (beam_antenna_control.h).
 *
 * Self-contained: exercises the callback-based antenna control interface
 * that will be implemented in WP-B. The test verifies:
 *   1) Default implementation logs without crashing.
 *   2) Custom callback receives correct event parameters.
 *   3) Destroy function handles cleanup safely.
 *
 * Build (standalone):
 *   gcc -o test_antenna_control test_antenna_control.c -Wall -Wextra && ./test_antenna_control
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/*  Duplicate the antenna control interface from WP-B (beam_antenna_control.h) */
/* ========================================================================= */

typedef int32_t frame_t;
typedef int32_t slot_t;

typedef struct {
  uint16_t rnti;
  int16_t  old_beam_index;
  int16_t  new_beam_index;
  int      frame;
  int      slot;
} beam_switch_event_t;

typedef void (*beam_switch_notify_fn)(const beam_switch_event_t *event);

typedef struct {
  beam_switch_notify_fn on_beam_switch;
  void *priv;
} antenna_control_if_t;

/* ---- Default implementation (duplicated from beam_antenna_control.c) ---- */

static void default_beam_switch_notify(const beam_switch_event_t *event)
{
  printf("  [DefaultCtrl] Beam switch: RNTI=0x%04x beam %d -> %d at (%d.%d)\n",
         event->rnti,
         event->old_beam_index,
         event->new_beam_index,
         event->frame,
         event->slot);
}

static antenna_control_if_t *create_default_antenna_ctrl(void)
{
  antenna_control_if_t *ctrl = calloc(1, sizeof(*ctrl));
  ctrl->on_beam_switch = default_beam_switch_notify;
  ctrl->priv = NULL;
  return ctrl;
}

static void destroy_antenna_ctrl(antenna_control_if_t *ctrl)
{
  free(ctrl);
}

/* ========================================================================= */
/*  Test infrastructure                                                      */
/* ========================================================================= */

static int tests_run;
static int tests_passed;
static int tests_failed;

#define TEST_START(name)                \
  do {                                  \
    tests_run++;                        \
    const char *_test_name = name;      \
    printf("  [RUN ] %s\n", _test_name);

#define TEST_END()                      \
    tests_passed++;                     \
    printf("  [PASS] %s\n", _test_name);\
  } while (0)

#define ASSERT_EQ(a, b)                                                                              \
  do {                                                                                               \
    if ((a) != (b)) {                                                                                \
      printf("  [FAIL] %s (line %d): expected %d, got %d\n", _test_name, __LINE__, (int)(b), (int)(a)); \
      tests_failed++;                                                                                \
      return;                                                                                        \
    }                                                                                                \
  } while (0)

#define ASSERT_TRUE(cond)                                                       \
  do {                                                                          \
    if (!(cond)) {                                                              \
      printf("  [FAIL] %s (line %d): condition not met\n", _test_name, __LINE__);\
      tests_failed++;                                                           \
      return;                                                                   \
    }                                                                           \
  } while (0)

/* ---- Custom mock callback ---- */

typedef struct {
  int call_count;
  beam_switch_event_t last_event;
  /* History for multi-event verification */
  beam_switch_event_t history[16];
} mock_ctrl_state_t;

static void mock_notify(const beam_switch_event_t *event)
{
  /* retrieve priv from a global — in real code, use container_of */
  /* but for test simplicity, we use a global */
  extern mock_ctrl_state_t g_mock_state;
  if (g_mock_state.call_count < 16)
    g_mock_state.history[g_mock_state.call_count] = *event;
  g_mock_state.last_event = *event;
  g_mock_state.call_count++;
}

mock_ctrl_state_t g_mock_state;

/* ========================================================================= */
/*  Test cases                                                               */
/* ========================================================================= */

/* TC-D1: Default implementation doesn't crash */
static void test_default_impl_no_crash(void)
{
  TEST_START("default_impl_no_crash");

  antenna_control_if_t *ctrl = create_default_antenna_ctrl();
  ASSERT_TRUE(ctrl != NULL);
  ASSERT_TRUE(ctrl->on_beam_switch != NULL);

  beam_switch_event_t event = {
    .rnti = 0xBEEF,
    .old_beam_index = 0,
    .new_beam_index = 42,
    .frame = 100,
    .slot = 7,
  };
  /* Should log and not crash */
  ctrl->on_beam_switch(&event);

  destroy_antenna_ctrl(ctrl);

  TEST_END();
}

/* TC-D2: Custom callback receives correct parameters */
static void test_custom_callback_params(void)
{
  TEST_START("custom_callback_params");

  memset(&g_mock_state, 0, sizeof(g_mock_state));

  antenna_control_if_t ctrl = {
    .on_beam_switch = mock_notify,
    .priv = &g_mock_state,
  };

  beam_switch_event_t event = {
    .rnti = 0x1234,
    .old_beam_index = 3,
    .new_beam_index = 57,
    .frame = 42,
    .slot = 13,
  };
  ctrl.on_beam_switch(&event);

  ASSERT_EQ(g_mock_state.call_count, 1);
  ASSERT_EQ(g_mock_state.last_event.rnti, 0x1234);
  ASSERT_EQ(g_mock_state.last_event.old_beam_index, 3);
  ASSERT_EQ(g_mock_state.last_event.new_beam_index, 57);
  ASSERT_EQ(g_mock_state.last_event.frame, 42);
  ASSERT_EQ(g_mock_state.last_event.slot, 13);

  TEST_END();
}

/* TC-D3: Multiple callbacks — event history captured */
static void test_multiple_callbacks_history(void)
{
  TEST_START("multiple_callbacks_history");

  memset(&g_mock_state, 0, sizeof(g_mock_state));

  antenna_control_if_t ctrl = {
    .on_beam_switch = mock_notify,
    .priv = &g_mock_state,
  };

  int beams[] = {0, 8, 16, 32, 48, 63};
  for (int i = 0; i < 5; i++) {
    beam_switch_event_t event = {
      .rnti = 0xAAAA,
      .old_beam_index = beams[i],
      .new_beam_index = beams[i + 1],
      .frame = i,
      .slot = 0,
    };
    ctrl.on_beam_switch(&event);
  }

  ASSERT_EQ(g_mock_state.call_count, 5);
  /* Verify first and last events */
  ASSERT_EQ(g_mock_state.history[0].old_beam_index, 0);
  ASSERT_EQ(g_mock_state.history[0].new_beam_index, 8);
  ASSERT_EQ(g_mock_state.history[4].old_beam_index, 48);
  ASSERT_EQ(g_mock_state.history[4].new_beam_index, 63);

  TEST_END();
}

/* TC-D4: Destroy with NULL is safe */
static void test_destroy_null_safe(void)
{
  TEST_START("destroy_null_safe");

  /* Ensure destroy doesn't crash with NULL */
  destroy_antenna_ctrl(NULL);

  TEST_END();
}

/* TC-D5: Create and destroy cycle */
static void test_create_destroy_cycle(void)
{
  TEST_START("create_destroy_cycle");

  for (int i = 0; i < 100; i++) {
    antenna_control_if_t *ctrl = create_default_antenna_ctrl();
    ASSERT_TRUE(ctrl != NULL);
    destroy_antenna_ctrl(ctrl);
  }

  TEST_END();
}

/* ========================================================================= */
/*  Main                                                                     */
/* ========================================================================= */

int main(void)
{
  printf("=== Antenna Control Interface Tests ===\n\n");

  test_default_impl_no_crash();
  test_custom_callback_params();
  test_multiple_callbacks_history();
  test_destroy_null_safe();
  test_create_destroy_cycle();

  printf("\n--- Results: %d run, %d passed, %d failed ---\n",
         tests_run, tests_passed, tests_failed);

  return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
