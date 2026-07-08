#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

APP_ID="com.indrolend.digitalbreakdown"
MODE="${1:-status}"

wait_for_adb() {
  echo "== Wait for ADB device =="
  adb wait-for-device

  # Wait until Android system is actually booted.
  for i in $(seq 1 60); do
    booted="$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r' || true)"
    if [ "$booted" = "1" ]; then
      echo "Android booted."
      return 0
    fi
    sleep 2
  done

  echo "Timed out waiting for sys.boot_completed=1"
  return 1
}

launch_app() {
  echo "== Launch app =="
  adb shell monkey -p "$APP_ID" 1 >/dev/null
}

status_compact() {
  echo "== Compact Android status =="
  ./scripts/status-android.sh
}

posttest() {
  echo "== Post-test status =="
  ./scripts/status-android.sh

  echo ""
  echo "== Recent telemetry =="
  adb logcat -d -t 1200 2>/dev/null \
    | grep '\[DBTEL\]' \
    | tail -40 \
    || echo "No DBTEL telemetry found."

  echo ""
  echo "== Recent crash scan =="
  adb logcat -d -t 1000 2>/dev/null \
    | grep -Ei 'FATAL EXCEPTION|AndroidRuntime.*FATAL|ANR in|am_crash|Process.*has died|Fatal signal|Force finishing|has stopped' \
    | grep -Evi 'cr_CrashFileManager|Crash Reports does not exist' \
    || echo "No crash lines found."
}

case "$MODE" in
  status)
    status_compact
    ;;

  diagnose)
    status_compact
    echo ""
    echo "== ADB LG probe =="
    ./scripts/probe-lg.sh adb
    ;;

  android-cycle)
    ./scripts/build-android.sh
    ./scripts/install-android.sh
    sleep 2
    status_compact
    ;;

  test-cycle)
    ./scripts/build-android.sh
    ./scripts/install-android.sh
    sleep 5
    posttest
    ;;

  posttest)
    posttest
    ;;

  fastboot-cycle)
    echo "Fastboot cycle is read-only."
    echo "It will reboot to bootloader, probe variables, reboot back to Android, then run compact status."
    echo "It will NOT unlock, flash, erase, format, or set active slot."
    echo ""

    adb devices
    adb reboot bootloader

    echo "== Wait for fastboot =="
    for i in $(seq 1 30); do
      if fastboot devices | grep -q .; then
        break
      fi
      sleep 1
    done

    if ! fastboot devices | grep -q .; then
      echo "No fastboot device detected."
      exit 1
    fi

    ./scripts/probe-lg.sh fastboot

    echo "== Reboot Android =="
    fastboot reboot

    wait_for_adb
    sleep 3
    status_compact
    ;;

  download-note)
    cat <<'TXT'
Download Mode cannot be fully chained from Android safely on this LG setup.

Manual safe flow:
  1. Power phone off.
  2. Hold Volume Up.
  3. Plug USB into Mac.
  4. Wait for Download/Firmware screen.
  5. Run:
       npm run lg:download
  6. Reboot phone normally.

This repo does not run LGUP, LAF writes, firehose, flash, erase, or QFIL.
TXT
    ;;

  *)
    cat <<TXT
Unknown cycle mode: $MODE

Available:
  ./scripts/run-cycle.sh status
  ./scripts/run-cycle.sh diagnose
  ./scripts/run-cycle.sh android-cycle
  ./scripts/run-cycle.sh test-cycle
  ./scripts/run-cycle.sh posttest
  ./scripts/run-cycle.sh fastboot-cycle
  ./scripts/run-cycle.sh download-note
TXT
    exit 2
    ;;
esac
