#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="logs/root-probe-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT_DIR"

echo "Writing probe logs to: $OUT_DIR"

echo "== ADB devices =="
adb devices -l | tee "$OUT_DIR/adb-devices.txt"

echo "== Basic Android properties =="
adb shell '
echo "--- identity ---"
getprop ro.product.model
getprop ro.product.name
getprop ro.product.device
getprop ro.product.manufacturer
getprop ro.hardware
getprop ro.board.platform

echo "--- build ---"
getprop ro.build.version.release
getprop ro.build.version.sdk
getprop ro.build.version.security_patch
getprop ro.build.fingerprint
getprop ro.bootloader
getprop ro.bootimage.build.fingerprint

echo "--- boot / lock state ---"
getprop ro.oem_unlock_supported
getprop sys.oem_unlock_allowed
getprop ro.boot.flash.locked
getprop ro.boot.verifiedbootstate
getprop ro.boot.vbmeta.device_state
getprop ro.boot.veritymode
getprop ro.boot.warranty_bit

echo "--- partitions / slots ---"
getprop ro.boot.slot_suffix
getprop ro.boot.dynamic_partitions
getprop ro.boot.super_partition
getprop ro.boot.logical_partitions

echo "--- cpu ---"
getprop ro.product.cpu.abi
getprop ro.product.cpu.abilist
' | tee "$OUT_DIR/getprop-summary.txt"

echo "== Kernel =="
adb shell uname -a | tee "$OUT_DIR/uname.txt"

echo "== Mounts =="
adb shell mount | tee "$OUT_DIR/mount.txt" || true

echo "== Block devices =="
adb shell '
if [ -d /dev/block/by-name ]; then
  ls -al /dev/block/by-name
else
  echo "/dev/block/by-name not present"
  echo "Searching likely block symlink directories..."
  find /dev/block -maxdepth 4 -type l 2>/dev/null | sort | head -300
fi
' 2>&1 | tee "$OUT_DIR/block-by-name.txt" || true

echo "== Packages related to LG / update / boot =="
adb shell pm list packages 2>&1 | grep -Ei 'lg|update|boot|carrier|metropcs|tmobile|system' | tee "$OUT_DIR/packages-filtered.txt" || true

echo "== Check su presence, non-invasive =="
adb shell 'command -v su || echo "no su in PATH"' | tee "$OUT_DIR/su-check.txt" || true

echo "== Done ADB probe =="
echo "Next optional step: bootloader / fastboot probe."
echo "This script does NOT unlock or flash anything."
echo ""
echo "To try bootloader probe manually:"
echo "  adb reboot bootloader"
echo "  fastboot devices"
echo "  fastboot getvar all"
echo "  fastboot oem device-info"
echo "  fastboot reboot"
