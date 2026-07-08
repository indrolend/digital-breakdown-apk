#!/usr/bin/env bash
set -euo pipefail

APP_ID="com.indrolend.digitalbreakdown"
NO_LAUNCH="${1:-}"

if ! command -v scrcpy >/dev/null 2>&1; then
  echo "scrcpy not found."
  echo "Install with:"
  echo "  brew install scrcpy"
  exit 127
fi

echo "== ADB devices =="
adb devices

if [ "$NO_LAUNCH" != "--no-launch" ]; then
  echo ""
  echo "== Launch app =="
  adb shell monkey -p "$APP_ID" 1 >/dev/null || true
fi

echo ""
echo "== Start scrcpy mirror =="
echo "Close the scrcpy window to end mirroring."

scrcpy \
  --stay-awake \
  --window-title "Digital Breakdown - LG Stylo 4" \
  --max-size 1024 \
  --video-bit-rate 4M
