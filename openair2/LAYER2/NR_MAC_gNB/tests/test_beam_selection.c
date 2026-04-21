/*
 * Unit tests for beam_selection_procedures() and beam_switching_procedure().
 *
 * Self-contained: duplicates the minimal set of types and functions from the
 * OAI MAC layer so the test can be compiled without the full build graph.
 *
 * Tests confirm that:
 *   1) A change in the best-RSRP SSB index triggers a beam switch.
 *   2) An identical best-RSRP SSB index does NOT trigger a beam switch.
 *   3) Sequential RSRP changes cause sequential beam switches (0→32→63).
 *   4) When do_TCI is true, TCI-state indication path is exercised.
 *   5) Antenna-control callback is invoked with correct parameters.
 *   6) NULL antenna-control callback does not crash.
 *
 * Build (standalone):
 *   gcc -o test_beam_selection test_beam_selection.c -Wall -Wextra && ./test_beam_selection
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/*  Minimal type duplicates — keep in sync with nr_mac_gNB.h                 */
/* ========================================================================= */

#define MAX_NR_OF_REPORTED_RS 4
#define MAX_NUM_OF_SSB 64
#define NR_SYMBOLS_PER_SLOT 14
#define LOG_I(x, ...) do {} while (0)

typedef uint16_t rnti_t;

typedef enum {
  NO_BEAM_MODE,
  PRECONFIGURED_BEAM_IDX,
  LOPHY_BEAM_IDX,
} nr_beam_mode_t;

/* Minimal RSRP report */
typedef struct RSRP_report {
  uint8_t nr_reports;
  uint8_t resource_id[MAX_NR_OF_REPORTED_RS];
  int     RSRP[MAX_NR_OF_REPORTED_RS];
  int     SINRx10[MAX_NR_OF_REPORTED_RS];
} RSRP_report_t;

/* Minimal CSI report */
struct CSI_Report {
  RSRP_report_t ssb_rsrp_report;
};

/* TCI state indication */
typedef struct tciStateInd {
  bool     is_scheduled;
  uint32_t coresetId;
  uint32_t tciStateId;
} tciStateInd_t;

/* Minimal UE MAC CE control */
typedef struct {
  tciStateInd_t tci_state_ind;
} NR_UE_mac_ce_ctrl_t;

/* Minimal ControlResourceSet stub */
typedef struct {
  long controlResourceSetId;
} NR_ControlResourceSet_t;

/* Minimal sched_ctrl */
typedef struct {
  struct CSI_Report  CSI_report;
  NR_UE_mac_ce_ctrl_t UE_mac_ce_ctrl;
  NR_ControlResourceSet_t *coreset;
} NR_UE_sched_ctrl_t;

/* UE info */
typedef struct {
  rnti_t             rnti;
  int16_t            UE_beam_index;
  NR_UE_sched_ctrl_t UE_sched_ctrl;
} NR_UE_info_t;

/* Beam switch event (from beam_antenna_control.h — WP-B) */
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

/* Minimal beam info */
typedef struct {
  nr_beam_mode_t beam_mode;
} NR_beam_info_t;

/* Minimal radio config */
typedef struct {
  bool do_TCI;
} nr_radio_config_t;

/* Minimal MAC inst */
typedef struct {
  NR_beam_info_t     beam_info;
  nr_radio_config_t  radio_config;
  int                beam_index_list[MAX_NUM_OF_SSB];
  antenna_control_if_t *antenna_ctrl;   /* WP-B: antenna control interface */
} gNB_MAC_INST;

/* ========================================================================= */
/*  Functions under test (duplicated from gNB_scheduler_primitives.c)        */
/* ========================================================================= */

static int get_beam_from_ssbidx(gNB_MAC_INST *mac, int ssb_idx)
{
  int beam_idx = mac->beam_index_list[ssb_idx];
  return beam_idx;
}

/*
 * beam_switching_procedure — switches UE beam and invokes antenna control
 * callback.  The callback invocation is the code that WP-C will add;
 * we include it here to write tests FIRST (t-wada TDD: RED phase).
 */
static void beam_switching_procedure(gNB_MAC_INST *mac, NR_UE_info_t *UE, int new_beam_index, int frame, int slot)
{
  int old_beam = UE->UE_beam_index;
  LOG_I(NR_MAC, "[UE %x] Switching to beam with ID %d (from %d)\n", UE->rnti, new_beam_index, old_beam);
  UE->UE_beam_index = new_beam_index;

  /* --- WP-C addition: antenna control callback --- */
  if (mac->antenna_ctrl && mac->antenna_ctrl->on_beam_switch) {
    beam_switch_event_t event = {
      .rnti = UE->rnti,
      .old_beam_index = (int16_t)old_beam,
      .new_beam_index = (int16_t)new_beam_index,
      .frame = frame,
      .slot = slot,
    };
    mac->antenna_ctrl->on_beam_switch(&event);
  }
}

/*
 * beam_selection_procedures — simplified for test.
 * Extended signature to accept frame/slot for callback forwarding.
 */
static void beam_selection_procedures(gNB_MAC_INST *mac, NR_UE_info_t *UE, int frame, int slot)
{
  if (mac->beam_info.beam_mode == NO_BEAM_MODE)
    return;

  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  RSRP_report_t *rsrp_report = &sched_ctrl->CSI_report.ssb_rsrp_report;
  int new_bf_index = get_beam_from_ssbidx(mac, rsrp_report->resource_id[0]);

  if (!mac->radio_config.do_TCI) {
    if (UE->UE_beam_index != new_bf_index)
      beam_switching_procedure(mac, UE, new_bf_index, frame, slot);
    return;
  }

  tciStateInd_t *tci = &sched_ctrl->UE_mac_ce_ctrl.tci_state_ind;
  if (UE->UE_beam_index == new_bf_index) {
    if (tci->is_scheduled) {
      tci->is_scheduled = false;
    }
    return;
  }

  tci->is_scheduled = true;
  tci->coresetId = sched_ctrl->coreset->controlResourceSetId;
  tci->tciStateId = new_bf_index;
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

/* ---- Mock antenna control callback ---- */

static int mock_callback_count;
static beam_switch_event_t mock_last_event;

static void mock_beam_switch_notify(const beam_switch_event_t *event)
{
  mock_callback_count++;
  mock_last_event = *event;
}

static antenna_control_if_t mock_antenna_ctrl = {
  .on_beam_switch = mock_beam_switch_notify,
  .priv = NULL,
};

/* ---- Helpers ---- */

static void init_mac(gNB_MAC_INST *mac, bool do_tci)
{
  memset(mac, 0, sizeof(*mac));
  mac->beam_info.beam_mode = PRECONFIGURED_BEAM_IDX;
  mac->radio_config.do_TCI = do_tci;
  /* identity mapping: SSB i -> beam i */
  for (int i = 0; i < MAX_NUM_OF_SSB; i++)
    mac->beam_index_list[i] = i;
  mac->antenna_ctrl = &mock_antenna_ctrl;
  mock_callback_count = 0;
  memset(&mock_last_event, 0, sizeof(mock_last_event));
}

static NR_ControlResourceSet_t coreset_stub = {.controlResourceSetId = 0};

static void init_ue(NR_UE_info_t *UE, rnti_t rnti, int16_t initial_beam)
{
  memset(UE, 0, sizeof(*UE));
  UE->rnti = rnti;
  UE->UE_beam_index = initial_beam;
  UE->UE_sched_ctrl.coreset = &coreset_stub;
}

static void set_rsrp_report(NR_UE_info_t *UE, uint8_t best_ssb_idx, int rsrp_dbm)
{
  RSRP_report_t *r = &UE->UE_sched_ctrl.CSI_report.ssb_rsrp_report;
  r->nr_reports = 1;
  r->resource_id[0] = best_ssb_idx;
  r->RSRP[0] = rsrp_dbm;
}

/* ========================================================================= */
/*  Test cases                                                               */
/* ========================================================================= */

/* TC-B1: RSRP change triggers beam switch */
static void test_beam_switch_on_rsrp_change(void)
{
  TEST_START("beam_switch_on_rsrp_change");

  gNB_MAC_INST mac;
  NR_UE_info_t UE;
  init_mac(&mac, false);
  init_ue(&UE, 0x1234, 3);

  /* Best SSB = 5, current beam = 3 → should switch to 5 */
  set_rsrp_report(&UE, 5, -80);
  beam_selection_procedures(&mac, &UE, 0, 0);

  ASSERT_EQ(UE.UE_beam_index, 5);
  ASSERT_EQ(mock_callback_count, 1);
  ASSERT_EQ(mock_last_event.rnti, 0x1234);
  ASSERT_EQ(mock_last_event.old_beam_index, 3);
  ASSERT_EQ(mock_last_event.new_beam_index, 5);

  TEST_END();
}

/* TC-B2: Same beam → no switch */
static void test_no_switch_when_same_beam(void)
{
  TEST_START("no_switch_when_same_beam");

  gNB_MAC_INST mac;
  NR_UE_info_t UE;
  init_mac(&mac, false);
  init_ue(&UE, 0x5678, 3);

  set_rsrp_report(&UE, 3, -70);
  beam_selection_procedures(&mac, &UE, 0, 0);

  ASSERT_EQ(UE.UE_beam_index, 3);
  ASSERT_EQ(mock_callback_count, 0);

  TEST_END();
}

/* TC-B3: Sequential beam switching 0→32→63 */
static void test_sequential_beam_switching(void)
{
  TEST_START("sequential_beam_switching_0_32_63");

  gNB_MAC_INST mac;
  NR_UE_info_t UE;
  init_mac(&mac, false);
  init_ue(&UE, 0xABCD, 0);

  /* Switch 1: 0 → 32 */
  set_rsrp_report(&UE, 32, -75);
  beam_selection_procedures(&mac, &UE, 1, 0);
  ASSERT_EQ(UE.UE_beam_index, 32);
  ASSERT_EQ(mock_callback_count, 1);
  ASSERT_EQ(mock_last_event.old_beam_index, 0);
  ASSERT_EQ(mock_last_event.new_beam_index, 32);

  /* Switch 2: 32 → 63 */
  set_rsrp_report(&UE, 63, -60);
  beam_selection_procedures(&mac, &UE, 2, 5);
  ASSERT_EQ(UE.UE_beam_index, 63);
  ASSERT_EQ(mock_callback_count, 2);
  ASSERT_EQ(mock_last_event.old_beam_index, 32);
  ASSERT_EQ(mock_last_event.new_beam_index, 63);
  ASSERT_EQ(mock_last_event.frame, 2);
  ASSERT_EQ(mock_last_event.slot, 5);

  /* No switch: still 63 */
  set_rsrp_report(&UE, 63, -55);
  beam_selection_procedures(&mac, &UE, 3, 0);
  ASSERT_EQ(UE.UE_beam_index, 63);
  ASSERT_EQ(mock_callback_count, 2);

  TEST_END();
}

/* TC-B4: TCI path — do_TCI=true → tci_state_ind is set instead of direct switch */
static void test_tci_path(void)
{
  TEST_START("tci_state_indication_path");

  gNB_MAC_INST mac;
  NR_UE_info_t UE;
  init_mac(&mac, true);
  init_ue(&UE, 0x4321, 3);

  /* Best SSB = 10, current = 3 → tci_state_ind should be scheduled */
  set_rsrp_report(&UE, 10, -80);
  beam_selection_procedures(&mac, &UE, 0, 0);

  /* With do_TCI=true, beam_switching_procedure is NOT called directly */
  ASSERT_EQ(UE.UE_beam_index, 3);  /* unchanged */
  ASSERT_TRUE(UE.UE_sched_ctrl.UE_mac_ce_ctrl.tci_state_ind.is_scheduled);
  ASSERT_EQ((int)UE.UE_sched_ctrl.UE_mac_ce_ctrl.tci_state_ind.tciStateId, 10);

  /* If best beam goes back to current → tci_state_ind is cancelled */
  set_rsrp_report(&UE, 3, -70);
  beam_selection_procedures(&mac, &UE, 1, 0);
  ASSERT_TRUE(!UE.UE_sched_ctrl.UE_mac_ce_ctrl.tci_state_ind.is_scheduled);

  TEST_END();
}

/* TC-B5: NO_BEAM_MODE → no action */
static void test_no_beam_mode(void)
{
  TEST_START("no_beam_mode_skips_procedures");

  gNB_MAC_INST mac;
  NR_UE_info_t UE;
  init_mac(&mac, false);
  mac.beam_info.beam_mode = NO_BEAM_MODE;
  init_ue(&UE, 0x1111, 0);

  set_rsrp_report(&UE, 5, -80);
  beam_selection_procedures(&mac, &UE, 0, 0);

  ASSERT_EQ(UE.UE_beam_index, 0);
  ASSERT_EQ(mock_callback_count, 0);

  TEST_END();
}

/* TC-B6: NULL antenna_ctrl callback does not crash */
static void test_null_callback_safe(void)
{
  TEST_START("null_callback_safe");

  gNB_MAC_INST mac;
  NR_UE_info_t UE;
  init_mac(&mac, false);
  mac.antenna_ctrl = NULL;  /* no antenna control */
  init_ue(&UE, 0x2222, 0);

  set_rsrp_report(&UE, 10, -80);
  beam_selection_procedures(&mac, &UE, 0, 0);

  /* Should switch without crash */
  ASSERT_EQ(UE.UE_beam_index, 10);

  TEST_END();
}

/* TC-B7: antenna_ctrl set but on_beam_switch is NULL */
static void test_null_on_beam_switch_safe(void)
{
  TEST_START("null_on_beam_switch_callback_safe");

  antenna_control_if_t empty_ctrl = {.on_beam_switch = NULL, .priv = NULL};
  gNB_MAC_INST mac;
  NR_UE_info_t UE;
  init_mac(&mac, false);
  mac.antenna_ctrl = &empty_ctrl;
  init_ue(&UE, 0x3333, 5);

  set_rsrp_report(&UE, 20, -80);
  beam_selection_procedures(&mac, &UE, 0, 0);

  ASSERT_EQ(UE.UE_beam_index, 20);

  TEST_END();
}

/* ========================================================================= */
/*  Main                                                                     */
/* ========================================================================= */

int main(void)
{
  printf("=== Beam Selection & Switching Procedure Tests ===\n\n");

  test_beam_switch_on_rsrp_change();
  test_no_switch_when_same_beam();
  test_sequential_beam_switching();
  test_tci_path();
  test_no_beam_mode();
  test_null_callback_safe();
  test_null_on_beam_switch_safe();

  printf("\n--- Results: %d run, %d passed, %d failed ---\n",
         tests_run, tests_passed, tests_failed);

  return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
