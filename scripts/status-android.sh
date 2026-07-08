#!/usr/bin/env bash
set -euo pipefail

APP_ID="com.indrolend.digitalbreakdown"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

STATE_DIR="logs/state"
mkdir -p "$STATE_DIR"

CURRENT="$STATE_DIR/device-current.env"
PREVIOUS="$STATE_DIR/device-previous.env"
DIFF_FILE="$STATE_DIR/device-last-diff.txt"
HISTORY="$STATE_DIR/device-history.ndjson"

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT

adb shell "
echo model=\$(getprop ro.product.model)
echo product=\$(getprop ro.product.name)
echo device=\$(getprop ro.product.device)
echo platform=\$(getprop ro.board.platform)
echo android=\$(getprop ro.build.version.release)
echo sdk=\$(getprop ro.build.version.sdk)
echo patch=\$(getprop ro.build.version.security_patch)
echo fingerprint=\$(getprop ro.build.fingerprint)
echo verifiedbootstate=\$(getprop ro.boot.verifiedbootstate)
echo flash_locked=\$(getprop ro.boot.flash.locked)
echo veritymode=\$(getprop ro.boot.veritymode)
echo vbmeta_device_state=\$(getprop ro.boot.vbmeta.device_state)
echo oem_unlock_allowed=\$(getprop sys.oem_unlock_allowed)
echo slot_suffix=\$(getprop ro.boot.slot_suffix)
echo app_path=\$(pm path $APP_ID 2>/dev/null | head -1)
echo app_pid=\$(pidof $APP_ID 2>/dev/null)
echo foreground=\$(dumpsys activity activities 2>/dev/null | grep -Ei 'mResumedActivity|topResumedActivity|ResumedActivity' | head -1 | tr -s ' ')
" > "$TMP" 2>/dev/null || {
  echo "ADB status failed. Is the phone booted and authorized?"
  exit 1
}

MEMINFO="$(adb shell cat /proc/meminfo 2>/dev/null | tr -d '\r' || true)"
MEM_AVAILABLE="$(printf "%s\n" "$MEMINFO" | awk '/MemAvailable/ {print $2; exit}')"
SWAP_FREE="$(printf "%s\n" "$MEMINFO" | awk '/SwapFree/ {print $2; exit}')"

{
  echo "mem_available_kb=${MEM_AVAILABLE}"
  echo "swap_free_kb=${SWAP_FREE}"
} >> "$TMP"

# Normalize empty values and remove volatile whitespace.
sed -i.bak 's/[[:space:]]*$//' "$TMP"
rm -f "$TMP.bak"

if [ -f "$CURRENT" ]; then
  cp "$CURRENT" "$PREVIOUS"
else
  touch "$PREVIOUS"
fi

cp "$TMP" "$CURRENT"

{
  echo "timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  cat "$CURRENT"
} | python3 -c '
import sys, json
d={}
for line in sys.stdin:
    line=line.rstrip("\n")
    if "=" in line:
        k,v=line.split("=",1)
        d[k]=v
print(json.dumps(d, separators=(",",":")))
' >> "$HISTORY"

set +e
diff -u "$PREVIOUS" "$CURRENT" > "$DIFF_FILE"
DIFF_CODE=$?
set -e

verified="$(grep '^verifiedbootstate=' "$CURRENT" | cut -d= -f2-)"
locked="$(grep '^flash_locked=' "$CURRENT" | cut -d= -f2-)"
verity="$(grep '^veritymode=' "$CURRENT" | cut -d= -f2-)"
app_path="$(grep '^app_path=' "$CURRENT" | cut -d= -f2-)"
app_pid="$(grep '^app_pid=' "$CURRENT" | cut -d= -f2-)"
foreground="$(grep '^foreground=' "$CURRENT" | cut -d= -f2-)"
mem="$(grep '^mem_available_kb=' "$CURRENT" | cut -d= -f2-)"

echo "Android status"
echo "State file: $CURRENT"

if [ "$DIFF_CODE" -eq 0 ]; then
  echo "Change: none"
else
  echo "Change: detected"
  echo "Diff: $DIFF_FILE"
fi

echo "Verified: ${verified:-unknown} / locked=${locked:-unknown} / verity=${verity:-unknown}"

if [ -n "$app_path" ]; then
  echo "App installed: yes"
else
  echo "App installed: no"
fi

if [ -n "$app_pid" ]; then
  echo "App process: running pid=$app_pid"
else
  echo "App process: not running"
fi

echo "Foreground: ${foreground:-unknown}"
echo "MemAvailable: ${mem:-unknown} KB"

# Only print diff details when meaningful and not first-run noise.
if [ "$DIFF_CODE" -ne 0 ] && [ -s "$PREVIOUS" ]; then
  echo ""
  echo "Changed fields:"
  grep -E '^[+-][a-zA-Z0-9_]+=' "$DIFF_FILE" | grep -v '^---' | grep -v '^+++' || true
fi
