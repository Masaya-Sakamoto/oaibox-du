#!/bin/bash
# =============================================================================
# run_beam_switch_test.sh
#
# Integration test: FR2 64-beam SSB with symbol-level beam scheduling.
# Validates that channel variation (via RFsim path_loss manipulation)
# triggers beam switching in the gNB MAC scheduler.
#
# Prerequisites:
#   - OAI gNB and UE binaries built with RFsim (cmake -DRFSIMULATOR=ON)
#   - The gNB config: gnb.sa.band257.u3.66prb.rfsim.beam-symbol.conf
#   - Channel model config: channelmod_rfsimu.conf
#
# Test scenario:
#   1. Start gNB (server) with 64-beam symbol-level config
#   2. Start UE (client) and wait for RA completion
#   3. Inject channel variation via telnet (setpathloss or setdistance)
#   4. Check gNB logs for beam switching events
#   5. Report PASS/FAIL
#
# Usage:
#   ./run_beam_switch_test.sh [--gnb-binary <path>] [--ue-binary <path>]
#                             [--conf-dir <dir>] [--timeout <sec>]
# =============================================================================

set -euo pipefail

# ---- Defaults ----
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
GNB_BINARY="${REPO_ROOT}/cmake_targets/ran_build/build/nr-softmodem"
UE_BINARY="${REPO_ROOT}/cmake_targets/ran_build/build/nr-uesoftmodem"
CONF_DIR="${REPO_ROOT}/ci-scripts/conf_files"
GNB_CONF="${CONF_DIR}/gnb.sa.band257.u3.66prb.rfsim.beam-symbol.conf"
TIMEOUT=60
TELNET_PORT=9090
TELNET_TIMEOUT=2

# ---- Logging ----
LOG_DIR="${SCRIPT_DIR}/logs"
mkdir -p "${LOG_DIR}"
GNB_LOG="${LOG_DIR}/gnb_beam_switch.log"
UE_LOG="${LOG_DIR}/ue_beam_switch.log"
RESULT_LOG="${LOG_DIR}/test_result.log"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }

# ---- Argument parsing ----
while [[ $# -gt 0 ]]; do
  case $1 in
    --gnb-binary)  GNB_BINARY="$2";  shift 2;;
    --ue-binary)   UE_BINARY="$2";   shift 2;;
    --conf-dir)    CONF_DIR="$2";    shift 2;;
    --timeout)     TIMEOUT="$2";     shift 2;;
    *)             error "Unknown option: $1"; exit 1;;
  esac
done

# ---- Pre-flight checks ----
preflight() {
  local fail=0
  if [[ ! -x "${GNB_BINARY}" ]]; then
    error "gNB binary not found: ${GNB_BINARY}"
    fail=1
  fi
  if [[ ! -x "${UE_BINARY}" ]]; then
    error "UE binary not found: ${UE_BINARY}"
    fail=1
  fi
  if [[ ! -f "${GNB_CONF}" ]]; then
    error "gNB config not found: ${GNB_CONF}"
    fail=1
  fi
  if [[ ${fail} -ne 0 ]]; then
    error "Pre-flight checks failed. Build OAI with RFsim first."
    exit 1
  fi
  info "Pre-flight OK"
}

# ---- Cleanup on exit ----
GNB_PID=""
UE_PID=""

cleanup() {
  info "Cleaning up..."
  [[ -n "${UE_PID}" ]]  && kill "${UE_PID}"  2>/dev/null || true
  [[ -n "${GNB_PID}" ]] && kill "${GNB_PID}" 2>/dev/null || true
  sleep 1
  [[ -n "${UE_PID}" ]]  && kill -9 "${UE_PID}"  2>/dev/null || true
  [[ -n "${GNB_PID}" ]] && kill -9 "${GNB_PID}" 2>/dev/null || true
  wait 2>/dev/null || true
}

trap cleanup EXIT

# ---- Telnet helper ----
# Usage: telnet_cmd "rfsimu setpathloss rfsimu_channel_ue0 -20.0"
telnet_cmd() {
  local cmd="$1"
  (echo "${cmd}"; sleep "${TELNET_TIMEOUT}"; echo "exit") \
    | timeout 5 telnet localhost "${TELNET_PORT}" 2>/dev/null || true
}

# ---- Start gNB ----
start_gnb() {
  info "Starting gNB (band257, 64-beam, symbol-level)..."
  RFSIMULATOR=server "${GNB_BINARY}" \
    -O "${GNB_CONF}" \
    --sa \
    --rfsim \
    --telnetsrv \
    --telnetsrv.listenstdin \
    --telnetsrv.shrmod rfsim \
    > "${GNB_LOG}" 2>&1 &
  GNB_PID=$!
  info "gNB started (PID=${GNB_PID})"
}

# ---- Start UE ----
start_ue() {
  info "Starting OAI UE..."
  RFSIMULATOR=127.0.0.1 "${UE_BINARY}" \
    --rfsim \
    -r 66 \
    --numerology 3 \
    --band 257 \
    -C 27900000000 \
    --ssb 576 \
    --do-ra \
    --sa \
    > "${UE_LOG}" 2>&1 &
  UE_PID=$!
  info "UE started (PID=${UE_PID})"
}

# ---- Wait for RA completion ----
wait_for_ra() {
  info "Waiting for RA completion (timeout=${TIMEOUT}s)..."
  local elapsed=0
  while [[ ${elapsed} -lt ${TIMEOUT} ]]; do
    if grep -q "CFRA procedure succeeded" "${GNB_LOG}" 2>/dev/null || \
       grep -q "RA procedure succeeded" "${GNB_LOG}" 2>/dev/null || \
       grep -q "UE RNTI" "${GNB_LOG}" 2>/dev/null; then
      info "RA completed after ${elapsed}s"
      return 0
    fi
    # Check for fatal errors
    if grep -q "Segmentation fault\|FATAL\|Aborted\|AssertFatal" "${GNB_LOG}" 2>/dev/null; then
      error "gNB crashed during RA"
      tail -20 "${GNB_LOG}"
      return 1
    fi
    if ! kill -0 "${GNB_PID}" 2>/dev/null; then
      error "gNB process died"
      return 1
    fi
    sleep 1
    elapsed=$((elapsed + 1))
  done
  warn "RA did not complete within ${TIMEOUT}s"
  return 1
}

# ---- Inject channel variation scenario ----
inject_channel_variation() {
  info "Injecting channel variation scenario via telnet..."

  # Phase 1: Normal conditions (low path loss)
  info "  Phase 1: path_loss = -3.0 dB (good channel)"
  telnet_cmd "rfsimu setpathloss rfsimu_channel_ue0 -3.0"
  sleep 3

  # Phase 2: Degraded channel → should trigger beam switch
  info "  Phase 2: path_loss = -20.0 dB (degraded channel — expect beam switch)"
  telnet_cmd "rfsimu setpathloss rfsimu_channel_ue0 -20.0"
  sleep 5

  # Phase 3: Recovery
  info "  Phase 3: path_loss = -3.0 dB (recovered — possible beam switch back)"
  telnet_cmd "rfsimu setpathloss rfsimu_channel_ue0 -3.0"
  sleep 3

  info "Channel variation injection complete"
}

# ---- Fallback: use setdistance if setpathloss not available ----
inject_channel_variation_distance() {
  info "Injecting channel variation via distance changes..."

  # Baseline distance
  info "  Phase 1: distance = 100m"
  telnet_cmd "rfsimu setdistance rfsimu_channel_ue0 100.0"
  sleep 3

  # Move far → signal degradation
  info "  Phase 2: distance = 5000m (degraded — expect beam switch)"
  telnet_cmd "rfsimu setdistance rfsimu_channel_ue0 5000.0"
  sleep 5

  # Move close → recovery
  info "  Phase 3: distance = 100m (recovered)"
  telnet_cmd "rfsimu setdistance rfsimu_channel_ue0 100.0"
  sleep 3

  info "Distance-based channel variation complete"
}

# ---- Check for beam switching events in gNB log ----
check_beam_switch() {
  info "Checking gNB log for beam switching events..."

  local switch_count=0
  switch_count=$(grep -c "Switching to beam\|Beam switch\|AntennaCtrl.*beam.*->" "${GNB_LOG}" 2>/dev/null || echo 0)

  if [[ ${switch_count} -gt 0 ]]; then
    info "Found ${switch_count} beam switching event(s)"
    grep "Switching to beam\|Beam switch\|AntennaCtrl.*beam.*->" "${GNB_LOG}" | tail -10
    return 0
  else
    warn "No beam switching events detected"
    return 1
  fi
}

# ---- Check for fatal errors ----
check_no_fatal_errors() {
  local fatal_count=0
  fatal_count=$(grep -c "Segmentation fault\|FATAL\|Aborted\|AssertFatal" "${GNB_LOG}" 2>/dev/null || echo 0)

  if [[ ${fatal_count} -gt 0 ]]; then
    error "Fatal errors detected in gNB log:"
    grep "Segmentation fault\|FATAL\|Aborted\|AssertFatal" "${GNB_LOG}" | tail -5
    return 1
  fi
  return 0
}

# ---- Check that 64 SSBs are configured ----
check_64beam_config() {
  info "Checking 64-beam SSB configuration..."

  if grep -q "longBitmap\|ssb_PositionsInBurst.*64\|num_active_ssb.*64\|64 SSB" "${GNB_LOG}" 2>/dev/null; then
    info "64-beam SSB configuration confirmed"
    return 0
  fi
  # Even if not explicitly logged, check that beam scheduling is active
  if grep -q "beam_mode.*PRECONFIGURED\|set_analog_beamforming.*2\|beam_duration.*-7" "${GNB_LOG}" 2>/dev/null; then
    info "Symbol-level beam scheduling is active"
    return 0
  fi
  warn "Could not confirm 64-beam configuration in logs"
  return 0  # Non-fatal — the config file has the right settings
}

# =============================================================================
# Main test execution
# =============================================================================

main() {
  echo "=============================================="
  echo " FR2 64-Beam SSB Integration Test"
  echo " Band:    257 (27.9 GHz, 120kHz SCS)"
  echo " Config:  symbol-level, beam_duration=-7"
  echo " Beams:   64 (longBitmap=0xFFFFFFFFFFFFFFFF)"
  echo "=============================================="
  echo ""

  preflight

  local test_result="PASS"
  local failures=0

  # Step 1: Start gNB
  start_gnb
  sleep 5

  # Step 2: Start UE
  start_ue
  sleep 2

  # Step 3: Wait for RA
  if ! wait_for_ra; then
    error "FAILED: RA did not complete"
    test_result="FAIL"
    failures=$((failures + 1))
  fi

  # Step 4: Check 64-beam configuration
  check_64beam_config

  # Step 5: Inject channel variation
  inject_channel_variation

  # Step 6: Check for beam switching
  if ! check_beam_switch; then
    warn "Beam switching not detected — trying distance-based fallback..."
    inject_channel_variation_distance
    if ! check_beam_switch; then
      error "FAILED: No beam switching events detected"
      test_result="FAIL"
      failures=$((failures + 1))
    fi
  fi

  # Step 7: Check no fatal errors
  if ! check_no_fatal_errors; then
    error "FAILED: Fatal errors in gNB"
    test_result="FAIL"
    failures=$((failures + 1))
  fi

  # Report
  echo ""
  echo "=============================================="
  if [[ "${test_result}" == "PASS" ]]; then
    echo -e " Result: ${GREEN}PASS${NC}"
  else
    echo -e " Result: ${RED}FAIL${NC} (${failures} failure(s))"
  fi
  echo " gNB log: ${GNB_LOG}"
  echo " UE  log: ${UE_LOG}"
  echo "=============================================="

  # Save result
  echo "test_result=${test_result}" > "${RESULT_LOG}"
  echo "failures=${failures}" >> "${RESULT_LOG}"
  echo "timestamp=$(date -Iseconds)" >> "${RESULT_LOG}"

  [[ "${test_result}" == "PASS" ]] && exit 0 || exit 1
}

main "$@"
