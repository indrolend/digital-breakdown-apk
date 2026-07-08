#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-help}"

case "$MODE" in
  build)
    ./scripts/build-android.sh
    ;;

  install)
    ./scripts/install-android.sh
    ;;

  run)
    ./scripts/build-android.sh
    ./scripts/install-android.sh
    ;;

  status)
    ./scripts/status-android.sh
    ;;

  test)
    ./scripts/run-cycle.sh test-cycle
    ;;

  posttest)
    ./scripts/run-cycle.sh posttest
    ;;

  logs)
    ./scripts/live-android-debug.sh
    ;;

  mirror)
    ./scripts/mirror-android.sh
    ;;

  demo)
    ./scripts/build-android.sh
    ./scripts/install-android.sh
    ./scripts/mirror-android.sh --no-launch
    ;;

  demo-window)
    ./scripts/demo-window-mac.sh
    ;;

  clean)
    ./scripts/clean.sh
    ;;

  help|-h|--help)
    cat <<'TXT'
Android workflow:

  ./scripts/android.sh build        Build web bundle + APK
  ./scripts/android.sh install      Install APK, clear app data, launch
  ./scripts/android.sh run          Build + install
  ./scripts/android.sh status       Compact device/app state
  ./scripts/android.sh test         Build + install + status + telemetry/crash scan
  ./scripts/android.sh posttest     Status + telemetry/crash scan only
  ./scripts/android.sh logs         Live filtered logcat
  ./scripts/android.sh mirror       Open scrcpy mirror/input
  ./scripts/android.sh demo         Build + install + mirror
  ./scripts/android.sh demo-window  Open live logs in Terminal + demo mirror
  ./scripts/android.sh clean        Clean generated files
TXT
    ;;

  *)
    echo "Unknown Android mode: $MODE"
    echo "Run: ./scripts/android.sh help"
    exit 2
    ;;
esac
