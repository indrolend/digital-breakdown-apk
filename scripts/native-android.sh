#!/usr/bin/env bash
set -euo pipefail

APP_ID="${APP_ID:-com.indrolend.digitalbreakdown.native}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NATIVE_DIR="$ROOT/native-android"
APK="$NATIVE_DIR/app/build/outputs/apk/debug/app-debug.apk"
LOG_DIR="$ROOT/logs/native"
STAMP="$(date +%Y%m%d-%H%M%S)"
MODE="${1:-test}"
STATUS_FILE="$LOG_DIR/status-$STAMP.txt"
LOGCAT_FILE="$LOG_DIR/logcat-$STAMP.txt"
CRASH_FILE="$LOG_DIR/crash-$STAMP.txt"
SCREEN_FILE="$LOG_DIR/screen-$STAMP.png"
SCREEN_BEFORE_FILE="$LOG_DIR/screen-$STAMP-before-input.png"
RESULT_FILE="$LOG_DIR/result-$STAMP.json"
INPUT_TAP_DELAY="${INPUT_TAP_DELAY:-1}"
INPUT_SWIPE_DELAY="${INPUT_SWIPE_DELAY:-2}"
# Base64 for a 1x1 transparent PNG fallback artifact when a run fails before a real screenshot is captured.
PLACEHOLDER_SCREEN_PNG_BASE64='iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO6pYJ0AAAAASUVORK5CYII='

BUILD_RESULT="fail"
INSTALL_RESULT="fail"
LAUNCH_RESULT="fail"
FOREGROUND_RESULT="fail"
NATIVE_LOGS_RESULT="fail"
CRASH_SCAN_RESULT="fail"
SCREENSHOT_RESULT="fail"
TRACK_RESULTS=0

mkdir -p "$LOG_DIR"

write_result_json() {
  cat > "$RESULT_FILE" <<EOF
{
  "build": "$BUILD_RESULT",
  "install": "$INSTALL_RESULT",
  "launch": "$LAUNCH_RESULT",
  "foreground": "$FOREGROUND_RESULT",
  "nativeLogs": "$NATIVE_LOGS_RESULT",
  "crashScan": "$CRASH_SCAN_RESULT",
  "screenshot": "$SCREENSHOT_RESULT"
}
EOF
  echo "Saved: $RESULT_FILE"
}

cleanup() {
  local exit_code=$?
  trap - EXIT
  if [ "$TRACK_RESULTS" -eq 1 ]; then
    write_result_json
  fi
  exit "$exit_code"
}

trap cleanup EXIT

initialize_artifacts() {
  printf 'pending\n' > "$STATUS_FILE"
  printf 'pending\n' > "$LOGCAT_FILE"
  printf 'pending\n' > "$CRASH_FILE"

  if command -v base64 >/dev/null 2>&1; then
    # 1x1 transparent PNG placeholder so failed runs still leave a screen-*.png artifact behind.
    if decode_base64 "$PLACEHOLDER_SCREEN_PNG_BASE64" > "$SCREEN_FILE"; then
      cp "$SCREEN_FILE" "$SCREEN_BEFORE_FILE"
    else
      echo "Placeholder PNG decode failed" | tee -a "$STATUS_FILE"
      : > "$SCREEN_FILE"
      : > "$SCREEN_BEFORE_FILE"
    fi
  else
    echo "base64 command unavailable; creating empty placeholder screenshots" | tee -a "$STATUS_FILE"
    : > "$SCREEN_FILE"
    : > "$SCREEN_BEFORE_FILE"
  fi
}

hash_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
    return
  fi
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}'
    return
  fi
  echo ""
}

decode_base64() {
  if printf '%s' "$1" | base64 -d >/dev/null 2>&1; then
    printf '%s' "$1" | base64 -d
    return 0
  fi
  if printf '%s' "$1" | base64 -D >/dev/null 2>&1; then
    printf '%s' "$1" | base64 -D
    return 0
  fi
  return 1
}

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
  BUILD_RESULT="pass"
  echo "APK: $APK"
}

install_native() {
  echo "== Install native APK =="
  adb_wait
  adb install -r "$APK"
  INSTALL_RESULT="pass"
}

launch_native() {
  echo "== Clear logcat =="
  adb logcat -c || true

  echo "== Launch native APK =="
  adb shell monkey -p "$APP_ID" 1
  sleep 5
  if adb shell pidof "$APP_ID" >/dev/null 2>&1; then
    LAUNCH_RESULT="pass"
  fi
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
  } | tee "$STATUS_FILE"

  if grep -Fq "$APP_ID" "$STATUS_FILE"; then
    FOREGROUND_RESULT="pass"
  fi
  if grep -Fq "DBNATIVE" "$STATUS_FILE"; then
    NATIVE_LOGS_RESULT="pass"
  fi
}

screenshot_native() {
  local target="${1:-$SCREEN_FILE}"
  echo "== Screenshot =="
  adb exec-out screencap -p > "$target"
  if [ -s "$target" ]; then
    SCREENSHOT_RESULT="pass"
  fi
  echo "Saved: $target"
}

valid_display_size() {
  local width="$1"
  local height="$2"

  if ! printf '%s\n' "$width" | grep -Eq '^[0-9]+$'; then
    return 1
  fi
  if ! printf '%s\n' "$height" | grep -Eq '^[0-9]+$'; then
    return 1
  fi
  if [ "$width" -le 100 ] || [ "$height" -le 100 ]; then
    return 1
  fi
  if [ "$width" -ge 10000 ] || [ "$height" -ge 10000 ]; then
    return 1
  fi
  return 0
}

probe_input() {
  local size width height mid_x mid_y drag_x
  echo "== Probe input =="
  if ! adb shell pidof "$APP_ID" >/dev/null 2>&1; then
    echo "Probe skipped: $APP_ID is not running" | tee -a "$STATUS_FILE"
    return 1
  fi
  size="$(adb shell wm size 2>/dev/null | tr -d '\r' | grep -Eo '[0-9]+x[0-9]+' | head -1 || true)"
  width="${size%x*}"
  height="${size#*x}"
  if ! valid_display_size "$width" "$height"; then
    width=1280
    height=720
  fi
  mid_x=$((width / 2))
  mid_y=$((height / 2))
  drag_x=$((mid_x + (width / 6)))

  adb shell input tap "$mid_x" "$mid_y"
  sleep "$INPUT_TAP_DELAY"
  adb shell input swipe "$mid_x" "$mid_y" "$drag_x" "$mid_y" 250
  sleep "$INPUT_SWIPE_DELAY"
}

verify_probe_output() {
  local before_hash after_hash
  echo "== Probe evidence =="

  if [ -f "$SCREEN_BEFORE_FILE" ] && [ -f "$SCREEN_FILE" ]; then
    before_hash="$(hash_file "$SCREEN_BEFORE_FILE")"
    after_hash="$(hash_file "$SCREEN_FILE")"
    if [ -n "$before_hash" ] && [ -n "$after_hash" ] && [ "$before_hash" != "$after_hash" ]; then
      echo "probeEvidence=screenshot-changed" | tee -a "$STATUS_FILE"
      return 0
    fi
  fi

  if grep -Fq "DBNATIVE" "$STATUS_FILE"; then
    echo "probeEvidence=dbnative-logs-present" | tee -a "$STATUS_FILE"
    return 0
  fi

  echo "probeEvidence=none" | tee -a "$STATUS_FILE"
  return 1
}

crash_scan() {
  echo "== Crash scan =="
  adb logcat -d -t 1500 > "$LOGCAT_FILE"
  if grep -Ei "FATAL EXCEPTION|AndroidRuntime|ANR in|am_crash|Fatal signal|has stopped|Force finishing" "$LOGCAT_FILE" > "$CRASH_FILE"; then
    echo "FAIL: crash lines found"
    cat "$CRASH_FILE"
    exit 1
  fi
  : > "$CRASH_FILE"
  CRASH_SCAN_RESULT="pass"
  echo "PASS: no crash lines found"
}

case "$MODE" in
  build) build_native ;;
  install) install_native ;;
  launch) launch_native ;;
  status) status_native ;;
  screenshot) screenshot_native ;;
  test|cycle)
    TRACK_RESULTS=1
    initialize_artifacts
    build_native
    install_native
    launch_native
    screenshot_native "$SCREEN_BEFORE_FILE"
    probe_input
    status_native
    screenshot_native
    verify_probe_output
    crash_scan
    ;;
  *)
    echo "Usage: $0 {build|install|launch|status|screenshot|test|cycle}"
    exit 2
    ;;
esac
