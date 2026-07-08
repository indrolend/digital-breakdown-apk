#!/usr/bin/env bash
set -euo pipefail

APP_ID="com.indrolend.digitalbreakdown"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

OUT_DIR="logs/postboot-status-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT_DIR"

keep_if_nonempty() {
  local file="$1"
  if [ ! -s "$file" ]; then
    rm -f "$file"
  fi
}

capture() {
  local name="$1"
  shift
  local file="$OUT_DIR/$name.txt"

  echo ""
  echo "== $name =="
  set +e
  "$@" 2>&1 | tee "$file"
  local code=${PIPESTATUS[0]}
  set -e

  keep_if_nonempty "$file"
  return 0
}

capture_shell() {
  local name="$1"
  local cmd="$2"
  local file="$OUT_DIR/$name.txt"

  echo ""
  echo "== $name =="
  set +e
  adb shell "$cmd" 2>&1 | tee "$file"
  local code=${PIPESTATUS[0]}
  set -e

  keep_if_nonempty "$file"
  return 0
}

echo "Post-boot status"
echo "Output: $OUT_DIR"

capture "adb-devices" adb devices -l

capture_shell "boot-state" '
echo "model=$(getprop ro.product.model)"
echo "product=$(getprop ro.product.name)"
echo "device=$(getprop ro.product.device)"
echo "platform=$(getprop ro.board.platform)"
echo "android=$(getprop ro.build.version.release)"
echo "sdk=$(getprop ro.build.version.sdk)"
echo "patch=$(getprop ro.build.version.security_patch)"
echo "fingerprint=$(getprop ro.build.fingerprint)"
echo "verifiedbootstate=$(getprop ro.boot.verifiedbootstate)"
echo "flash_locked=$(getprop ro.boot.flash.locked)"
echo "veritymode=$(getprop ro.boot.veritymode)"
echo "vbmeta_device_state=$(getprop ro.boot.vbmeta.device_state)"
echo "oem_unlock_allowed=$(getprop sys.oem_unlock_allowed)"
echo "slot_suffix=$(getprop ro.boot.slot_suffix)"
'

capture_shell "app-state" "
echo 'package path:'
pm path $APP_ID || true
echo ''
echo 'package version:'
dumpsys package $APP_ID 2>/dev/null | grep -Ei 'versionName|versionCode|firstInstallTime|lastUpdateTime|installerPackageName' || true
echo ''
echo 'process:'
pidof $APP_ID || true
"

capture_shell "foreground" '
dumpsys activity activities 2>/dev/null | grep -Ei "mResumedActivity|topResumedActivity|ResumedActivity" | head -20 || true
'

capture_shell "memory" '
cat /proc/meminfo | head -20
'

capture_shell "thermal" '
for f in /sys/class/thermal/thermal_zone*/temp; do
  [ -f "$f" ] || continue
  z="${f%/temp}"
  type="$(cat "$z/type" 2>/dev/null || echo unknown)"
  temp="$(cat "$f" 2>/dev/null || true)"
  echo "$type=$temp"
done
'

capture_shell "recent-app-logcat" "
logcat -d -t 300 2>/dev/null | grep -Ei '$APP_ID|Capacitor|chromium|crash|fatal|ANR|AndroidRuntime' || true
"

SUMMARY="$OUT_DIR/summary.txt"

{
  echo "Post-boot summary"
  echo "Generated: $(date)"
  echo "Folder: $OUT_DIR"
  echo ""

  if [ -f "$OUT_DIR/boot-state.txt" ]; then
    cat "$OUT_DIR/boot-state.txt"
  fi

  echo ""
  echo "== App installed =="
  if [ -f "$OUT_DIR/app-state.txt" ] && grep -q "$APP_ID" "$OUT_DIR/app-state.txt"; then
    echo "yes"
  else
    echo "unknown/no"
  fi

  echo ""
  echo "== Warnings =="
  if [ -f "$OUT_DIR/recent-app-logcat.txt" ]; then
    grep -Ei "crash|fatal|ANR|AndroidRuntime" "$OUT_DIR/recent-app-logcat.txt" || echo "no crash lines found"
  else
    echo "no recent app logcat matches"
  fi
} > "$SUMMARY"

find "$OUT_DIR" -type f -name "*.txt" -size 0 -delete 2>/dev/null || true

echo ""
echo "== Summary =="
cat "$SUMMARY"

echo ""
echo "Done."
