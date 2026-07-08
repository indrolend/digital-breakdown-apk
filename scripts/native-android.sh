#!/usr/bin/env bash
set -euo pipefail

APP_ID="${APP_ID:-com.indrolend.digitalbreakdown.native}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NATIVE_DIR="$ROOT/native-android"
APK="$NATIVE_DIR/app/build/outputs/apk/debug/app-debug.apk"
LOG_DIR="$ROOT/logs/native"
STAMP="$(date +%Y%m%d-%H%M%S)"
MODE="${1:-test}"

mkdir -p "$LOG_DIR"

ensure_local_properties() {
  if [ -f "$NATIVE_DIR/local.properties" ]; then
    return
  fi
  if [ -n "${ANDROID_HOME:-}" ] && [ -d "$ANDROID_HOME" ]; then
    printf 'sdk.dir=%s\n' "$ANDROID_HOME" > "$NATIVE_DIR/local.properties"
    return
  fi
  if [ -d "$HOME/Library/Android/sdk" ]; then
    printf 'sdk.dir=%s\n' "$HOME/Library/Android/sdk" > "$NATIVE_DIR/local.properties"
    return
  fi
  echo "Missing native-android/local.properties and ANDROID_HOME is not set."
  exit 2
}

adb_wait() {
  adb wait-for-device
  adb devices
}

build_native() {
  echo "== Build native APK =="
  ensure_local_properties
  "$ROOT/android/gradlew" -p "$NATIVE_DIR" assembleDebug
  test -f "$APK"
  echo "APK: $APK"
}

install_native() {
  echo "== Install native APK =="
  adb_wait
  adb install -r "$APK"
}

launch_native() {
  echo "== Clear logcat =="
  adb logcat -c || true

  echo "== Launch native APK =="
  adb shell monkey -p "$APP_ID" 1
  sleep 5
}

status_native() {
  echo "== Native status =="
  {
    echo "-- foreground --"
    adb shell dumpsys activity activities | grep -E "mResumedActivity|topResumedActivity" | tail -8 || true
    echo ""
    echo "-- process --"
    adb shell pidof "$APP_ID" || true
    echo ""
    echo "-- DBNATIVE/logcat --"
    adb logcat -d -t 1000 | grep -E "DBNATIVE|AndroidRuntime|FATAL EXCEPTION|Fatal signal|ANR in|am_crash|has stopped" || true
  } | tee "$LOG_DIR/status-$STAMP.txt"
}

screenshot_native() {
  echo "== Screenshot =="
  adb exec-out screencap -p > "$LOG_DIR/screen-$STAMP.png"
  echo "Saved: $LOG_DIR/screen-$STAMP.png"
}

crash_scan() {
  echo "== Crash scan =="
  adb logcat -d -t 1500 > "$LOG_DIR/logcat-$STAMP.txt"
  if grep -Ei "FATAL EXCEPTION|AndroidRuntime|ANR in|am_crash|Fatal signal|has stopped|Force finishing" "$LOG_DIR/logcat-$STAMP.txt" > "$LOG_DIR/crash-$STAMP.txt"; then
    echo "FAIL: crash lines found"
    cat "$LOG_DIR/crash-$STAMP.txt"
    exit 1
  fi
  : > "$LOG_DIR/crash-$STAMP.txt"
  echo "PASS: no crash lines found"
}

case "$MODE" in
  build) build_native ;;
  install) install_native ;;
  launch) launch_native ;;
  status) status_native ;;
  screenshot) screenshot_native ;;
  test|cycle)
    build_native
    install_native
    launch_native
    status_native
    screenshot_native
    crash_scan
    ;;
  *)
    echo "Usage: $0 {build|install|launch|status|screenshot|test|cycle}"
    exit 2
    ;;
esac
