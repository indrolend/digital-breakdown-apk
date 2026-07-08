#!/usr/bin/env bash
set -euo pipefail

APP_ID="com.indrolend.digitalbreakdown"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mkdir -p logs
LOG="logs/live-logcat-$(date +%Y%m%d-%H%M%S).txt"

echo "Live Android debug"
echo "Log: $LOG"
echo ""

adb devices

echo ""
echo "Starting filtered logcat."
echo "Leave this terminal open."
echo "In another terminal, run scrcpy if needed:"
echo "  scrcpy --max-size 1024 --video-bit-rate 2M"
echo ""

adb logcat -c || true

adb logcat 2>/dev/null \
  | grep -Ei --line-buffered "$APP_ID|Capacitor|chromium|WebView|crash|fatal|ANR|AndroidRuntime|OpenGL|EGL|Adreno|libc" \
  | tee "$LOG"
