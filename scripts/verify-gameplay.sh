#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/gameplay-checks}"
LOG_DIR="$BUILD_DIR/verification-logs"
mkdir -p "$LOG_DIR"

run_logged() {
  local name="$1"
  shift
  local log="$LOG_DIR/$name.log"
  echo "==> $name"
  if ! "$@" >"$log" 2>&1; then
    echo "FAILED: $name"
    cat "$log"
    return 1
  fi
  echo "PASS: $name"
}

run_logged configure cmake -S native-desktop -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
run_logged build cmake --build "$BUILD_DIR" --config Release --target \
  DigitalBreakdown \
  GameplayRoleAndSoulMotionTest \
  GameplayGeometryAndConfigTest \
  TargetLifecycleTest \
  GameplayStateContractsTest \
  PhoneBodyContractTest \
  PhoneMenuLayoutTest \
  MenuNavigationTest \
  PhoneDisplayStateTest \
  EarlyBrowserVisualsTest \
  TraversalCalibrationTest \
  Pass7ParityTest \
  MultiplayerProtocolTest \
  MultiplayerDeterminismTest \
  HostRemotePeerSimulationIsolationTest \
  MultiplayerConnectionStateTest \
  GameplayFlowProbe \
  RoomProgressionProbe \
  DeterministicInputSoak \
  --parallel
run_logged ctest ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure
run_logged parity "$BUILD_DIR/Pass7ParityTest"
run_logged multiplayer "$BUILD_DIR/MultiplayerProtocolTest"
run_logged determinism "$BUILD_DIR/MultiplayerDeterminismTest"
run_logged peer-isolation "$BUILD_DIR/HostRemotePeerSimulationIsolationTest"
run_logged diff-check git diff --check
echo "NATIVE_VERIFICATION=PASS suite=gameplay"
