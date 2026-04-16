#!/bin/bash
# RFsim integration smoke tests for symbol-level beam control.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
BUILD="${PROJECT_ROOT}/cmake_targets/ran_build/build"
CONF_DIR="${PROJECT_ROOT}/ci-scripts/conf_files"

MODE="${1:-do-ra}"
BEAM_MODE="${2:-symbol}"
TIMEOUT=30

LOG_DIR="/tmp/beam_test_$(date +%Y%m%d_%H%M%S)"
mkdir -p "${LOG_DIR}"
LOG_GNB="${LOG_DIR}/gnb.log"
LOG_UE="${LOG_DIR}/ue.log"

RESULT=0

if [ "${BEAM_MODE}" = "slot" ]; then
  CONF="${CONF_DIR}/gnb.band78.106prb.rfsim.phytest-dora.conf"
  EXPECTED_GRANULARITY="slot-level"
  TEST_LABEL="slot-level (regression)"
else
  CONF="${CONF_DIR}/gnb.band78.106prb.rfsim.beam-symbol.conf"
  EXPECTED_GRANULARITY="symbol-level"
  TEST_LABEL="symbol-level"
fi

echo "============================================================"
echo " RFsim Beam Test"
echo "   Mode:        ${MODE}"
echo "   Beam:        ${TEST_LABEL}"
echo "   Config:      ${CONF}"
echo "   Timeout:     ${TIMEOUT}s"
echo "   Log dir:     ${LOG_DIR}"
echo "============================================================"

if [ ! -x "${BUILD}/nr-softmodem" ] || [ ! -x "${BUILD}/nr-uesoftmodem" ]; then
  echo "[ERROR] nr-softmodem or nr-uesoftmodem not found in ${BUILD}"
  echo "        Please build first: cd cmake_targets && ./build_oai --ninja --gNB --nrUE -w SIMU"
  exit 1
fi

cleanup() {
  echo ""
  echo "--- Cleaning up processes ---"
  if [ -n "${UE_PID:-}" ]; then kill "${UE_PID}" 2>/dev/null || true; fi
  if [ -n "${GNB_PID:-}" ]; then kill "${GNB_PID}" 2>/dev/null || true; fi
  if [ -n "${UE_PID:-}" ]; then wait "${UE_PID}" 2>/dev/null || true; fi
  if [ -n "${GNB_PID:-}" ]; then wait "${GNB_PID}" 2>/dev/null || true; fi
}
trap cleanup EXIT

echo ""
echo "--- Starting gNB ---"
"${BUILD}/nr-softmodem" \
  -O "${CONF}" \
  --rfsim --"${MODE}" --noS1 \
  --log_config.global_log_options level,nocolor,time \
  > "${LOG_GNB}" 2>&1 &
GNB_PID=$!
echo "  gNB PID: ${GNB_PID}"

sleep 5

if [ "${MODE}" = "do-ra" ]; then
  echo ""
  echo "--- Starting UE ---"
  "${BUILD}/nr-uesoftmodem" \
    --rfsim --do-ra --noS1 \
    --rfsimulator.serveraddr 127.0.0.1 \
    --log_config.global_log_options level,nocolor,time \
    > "${LOG_UE}" 2>&1 &
  UE_PID=$!
  echo "  UE PID: ${UE_PID}"
fi

echo ""
echo "--- Running for ${TIMEOUT}s ---"
sleep "${TIMEOUT}"

cleanup
trap - EXIT

echo ""
echo "============================================================"
echo " Test Results"
echo "============================================================"

echo ""
echo "--- [Check 1] Beam Configuration ---"
BEAM_CFG_LINE=$(grep "Beam configuration:" "${LOG_GNB}" 2>/dev/null || true)
if [ -n "${BEAM_CFG_LINE}" ]; then
  echo "  [INFO] ${BEAM_CFG_LINE}"
  if echo "${BEAM_CFG_LINE}" | grep -q "${EXPECTED_GRANULARITY}"; then
    echo "  [PASS] ${EXPECTED_GRANULARITY} beam mode confirmed"
  else
    echo "  [FAIL] Expected ${EXPECTED_GRANULARITY} but got different mode"
    RESULT=1
  fi
else
  if [ "${BEAM_MODE}" = "slot" ]; then
    echo "  [SKIP] No beam configuration log in slot-regression mode"
  else
    echo "  [FAIL] Beam configuration log not found"
    RESULT=1
  fi
fi

echo ""
echo "--- [Check 2] Fatal Errors ---"
FATAL_GNB=$(grep -Ec "Assertion|FATAL|Segmentation fault|Aborted" "${LOG_GNB}" 2>/dev/null || true)
FATAL_UE=0
if [ -f "${LOG_UE}" ]; then
  FATAL_UE=$(grep -Ec "Assertion|FATAL|Segmentation fault|Aborted" "${LOG_UE}" 2>/dev/null || true)
fi
if [ "${FATAL_GNB}" -eq 0 ] && [ "${FATAL_UE}" -eq 0 ]; then
  echo "  [PASS] No fatal errors in gNB or UE logs"
else
  echo "  [FAIL] Fatal errors detected (gNB: ${FATAL_GNB}, UE: ${FATAL_UE})"
  [ "${FATAL_GNB}" -gt 0 ] && grep -E "Assertion|FATAL|Segmentation fault|Aborted" "${LOG_GNB}" | head -5
  [ "${FATAL_UE}" -gt 0 ] && grep -E "Assertion|FATAL|Segmentation fault|Aborted" "${LOG_UE}" | head -5
  RESULT=1
fi

if [ "${MODE}" = "do-ra" ]; then
  echo ""
  echo "--- [Check 3] RA Procedure ---"
  if grep -Eq "CBRA procedure succeeded|CFRA procedure succeeded|Activating SRB" "${LOG_GNB}" 2>/dev/null; then
    echo "  [PASS] RA procedure completed successfully"
  else
    echo "  [FAIL] RA procedure did not complete"
    echo "  --- Last 100 lines of gNB log ---"
    tail -100 "${LOG_GNB}"
    echo "  ---------------------------------"
    echo "  --- Last 100 lines of UE log ---"
    tail -100 "${LOG_UE}"
    RESULT=1
  fi
fi

if [ "${MODE}" = "phy-test" ]; then
  echo ""
  echo "--- [Check 3] DL Scheduling ---"
  if grep -q "DLSCH" "${LOG_GNB}" 2>/dev/null; then
    echo "  [PASS] DL scheduling observed"
  else
    echo "  [WARN] No DLSCH activity observed"
  fi
fi

echo ""
echo "============================================================"
if [ "${RESULT}" -eq 0 ]; then
  echo " OVERALL: PASS"
else
  echo " OVERALL: FAIL"
fi
echo " Logs saved to: ${LOG_DIR}"
echo "============================================================"

exit "${RESULT}"
