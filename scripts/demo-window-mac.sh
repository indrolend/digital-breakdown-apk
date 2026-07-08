#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "== Close existing scrcpy, if running =="
pkill -x scrcpy 2>/dev/null || true

echo "== Open live debug in Terminal =="
osascript <<OSA
tell application "Terminal"
  do script "cd '$ROOT' && npm run android:live-debug"
end tell
OSA

sleep 1

echo "== Build, install, and open mirror =="
npm run android:demo-cycle
