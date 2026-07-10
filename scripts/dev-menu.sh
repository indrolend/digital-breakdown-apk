#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

pause_menu() {
  printf "\nPress Return to continue..."
  read -r _
}

run_action() {
  printf "\n== %s ==\n" "$1"
  shift
  "$@"
  local status=$?
  printf "\nExit status: %s\n" "$status"
  pause_menu
}

native_reinstall() {
  adb uninstall com.indrolend.digitalbreakdown.native >/dev/null 2>&1 || true
  bash ./scripts/native-android.sh install
  bash ./scripts/native-android.sh launch
}

native_demo() {
  bash ./scripts/native-android.sh build || return $?
  adb uninstall com.indrolend.digitalbreakdown.native >/dev/null 2>&1 || true
  bash ./scripts/native-android.sh install || return $?
  bash ./scripts/native-android.sh launch || return $?
  bash ./scripts/mirror-android.sh --no-launch
}

core_smoke() {
  mkdir -p native/build
  clang++ \
    -std=c++17 \
    -Wall \
    -Wextra \
    -pedantic \
    -I native/core \
    native/core/simulation.cpp \
    native/core/render_state.cpp \
    native/tests/sim_smoke_test.cpp \
    -o native/build/db_sim_smoke && \
  ./native/build/db_sim_smoke
}

while true; do
  clear
  cat <<'MENU'
Digital Breakdown Dev Menu

Native Android
  1) Native status
  2) Build native APK
  3) Reinstall + launch native APK
  4) Native test pipeline
  5) Native demo: build + reinstall + launch + scrcpy
  6) Native logs
  7) Native screenshot

WebView Android
  8) Build web bundle
  9) Sync Capacitor Android
 10) WebView status
 11) WebView demo pipeline

Testing and diagnostics
 12) Native C++ smoke test
 13) ADB devices
 14) LG diagnostic cycle
 15) Open scrcpy

Repository
 16) Git status
 17) Pull latest GitHub changes
 18) Clean generated files

  0) Exit
MENU

  printf "\nSelect: "
  read -r choice

  case "$choice" in
    1)  run_action "Native status" bash ./scripts/native-android.sh status ;;
    2)  run_action "Build native APK" bash ./scripts/native-android.sh build ;;
    3)  run_action "Reinstall and launch native APK" native_reinstall ;;
    4)  run_action "Native test pipeline" bash ./scripts/native-android.sh test ;;
    5)  run_action "Native demo" native_demo ;;
    6)  run_action "Native logs" bash ./scripts/native-android.sh logs ;;
    7)  run_action "Native screenshot" bash ./scripts/native-android.sh screenshot ;;
    8)  run_action "Build web bundle" npm run build:web ;;
    9)  run_action "Sync Capacitor Android" npx cap sync android ;;
    10) run_action "WebView status" bash ./scripts/android.sh status ;;
    11) run_action "WebView demo" bash ./scripts/android.sh demo ;;
    12) run_action "Native C++ smoke test" core_smoke ;;
    13) run_action "ADB devices" adb devices -l ;;
    14) run_action "LG diagnostic cycle" bash ./scripts/run-cycle.sh diagnose ;;
    15) run_action "scrcpy" bash ./scripts/mirror-android.sh ;;
    16) run_action "Git status" git status --short --branch ;;
    17) run_action "Pull latest GitHub changes" git pull --ff-only ;;
    18) run_action "Clean generated files" bash ./scripts/clean.sh ;;
    0) exit 0 ;;
    *) printf "Unknown selection: %s\n" "$choice"; sleep 1 ;;
  esac
done
