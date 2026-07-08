#!/usr/bin/env bash
set -u

APPROVED_WRITE_COMMANDS=0
OUT_DIR="logs/lg-q710-interrogation-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT_DIR"

log() {
  echo ""
  echo "== $1 =="
}

capture() {
  local name="$1"
  shift
  log "$name"
  "$@" 2>&1 | tee "$OUT_DIR/$name.txt"
}

capture_shell() {
  local name="$1"
  local cmd="$2"
  log "$name"
  adb shell "$cmd" 2>&1 | tee "$OUT_DIR/$name.txt"
}

echo "LG Q710 hardware interrogation"
echo "Output: $OUT_DIR"
echo ""
echo "This script is READ-ONLY."
echo "It will NOT flash, erase, format, unlock, root, or write partitions."
echo ""

# -------------------------
# Host tool state
# -------------------------

capture "host-tools" bash -lc '
date
echo "--- adb ---"
which adb || true
adb version || true
echo "--- fastboot ---"
which fastboot || true
fastboot --version || true
echo "--- host ---"
uname -a
sw_vers 2>/dev/null || true
'

# -------------------------
# ADB interrogation
# -------------------------

capture "adb-devices" adb devices -l

capture_shell "android-identity" '
echo "--- product ---"
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
getprop ro.vendor.build.fingerprint
getprop ro.bootimage.build.fingerprint
getprop ro.bootloader

echo "--- lock / verified boot ---"
getprop ro.oem_unlock_supported
getprop sys.oem_unlock_allowed
getprop ro.boot.flash.locked
getprop ro.boot.verifiedbootstate
getprop ro.boot.vbmeta.device_state
getprop ro.boot.veritymode
getprop ro.boot.warranty_bit
getprop ro.boot.secureboot
getprop ro.secure
getprop ro.debuggable

echo "--- slots / partitions ---"
getprop ro.boot.slot_suffix
getprop ro.boot.dynamic_partitions
getprop ro.boot.super_partition
getprop ro.boot.logical_partitions

echo "--- cpu ---"
getprop ro.product.cpu.abi
getprop ro.product.cpu.abilist
'

capture_shell "kernel" 'uname -a'
capture_shell "cmdline" 'cat /proc/cmdline'
capture_shell "partitions" 'cat /proc/partitions'
capture_shell "mounts" 'mount'

capture_shell "fstab-search" '
find / -maxdepth 5 -iname "fstab*" 2>/dev/null | sort
'

capture_shell "block-map" '
echo "--- /dev/block/by-name ---"
if [ -d /dev/block/by-name ]; then
  ls -al /dev/block/by-name
else
  echo "not present"
fi

echo "--- /dev/block symlinks ---"
find /dev/block -maxdepth 6 -type l 2>/dev/null | sort

echo "--- platform dirs ---"
find /dev/block/platform -maxdepth 8 -type d 2>/dev/null | sort
'

capture_shell "boot-related-packages" '
pm list packages -f 2>/dev/null | grep -Ei "lge|lg|laf|fota|update|download|boot|recovery|carrier|tmobile|metropcs|system" || true
'

capture_shell "su-check" '
command -v su || echo "no su in PATH"
'

# -------------------------
# Fastboot interrogation
# -------------------------

log "reboot-to-fastboot"
adb reboot bootloader 2>&1 | tee "$OUT_DIR/reboot-to-fastboot.txt"
sleep 8

capture "fastboot-devices" fastboot devices

log "fastboot-selected-vars"
{
  fastboot getvar unlocked
  fastboot getvar secure
  fastboot flashing get_unlock_ability
  fastboot getvar product
  fastboot getvar variant
  fastboot getvar anti
  fastboot getvar current-slot
  fastboot getvar slot-count
  fastboot getvar has-slot:boot
  fastboot getvar has-slot:recovery
  fastboot getvar has-slot:laf
  fastboot getvar partition-type:boot
  fastboot getvar partition-type:boot_a
  fastboot getvar partition-type:laf
  fastboot getvar partition-size:boot
  fastboot getvar partition-size:boot_a
  fastboot getvar partition-size:laf
} 2>&1 | tee "$OUT_DIR/fastboot-selected-vars.txt"

capture "fastboot-getvar-all" fastboot getvar all
capture "fastboot-oem-device-id" fastboot oem device-id

# -------------------------
# Reboot back
# -------------------------

capture "fastboot-reboot" fastboot reboot

# -------------------------
# Generate summary
# -------------------------

SUMMARY="$OUT_DIR/summary.txt"

{
  echo "LG Q710 hardware interrogation summary"
  echo "Generated: $(date)"
  echo "Folder: $OUT_DIR"
  echo ""

  echo "== Android identity =="
  grep -E "LM-Q710|cv7a|msm8953|lge/cv7a|8\.1\.0|2020-05-01|armeabi|_a" "$OUT_DIR/android-identity.txt" || true
  echo ""

  echo "== Fastboot state =="
  grep -Ei "unlocked:|secure:|get_unlock_ability:|product:|variant:|anti:|current-slot:|slot-count:|has-slot|partition-type|partition-size" "$OUT_DIR/fastboot-selected-vars.txt" "$OUT_DIR/fastboot-getvar-all.txt" || true
  echo ""

  echo "== LG Device-ID =="
  grep -E "^[0-9A-Fa-f]{16,}$" "$OUT_DIR/fastboot-oem-device-id.txt" || true
  echo ""

  echo "== Safety conclusion =="
  if grep -qi "unlocked: no" "$OUT_DIR/fastboot-selected-vars.txt" "$OUT_DIR/fastboot-getvar-all.txt"; then
    echo "- Bootloader appears locked."
  fi

  if grep -qi "get_unlock_ability: 1" "$OUT_DIR/fastboot-selected-vars.txt"; then
    echo "- OEM unlock ability flag reports 1, but this does not mean the bootloader is unlocked."
  fi

  if grep -qi "device-id" "$OUT_DIR/fastboot-oem-device-id.txt"; then
    echo "- LG legacy unlock.bin flow is present."
  fi

  if grep -qi "no su in PATH" "$OUT_DIR/su-check.txt"; then
    echo "- No su binary found through ADB shell."
  fi

  echo "- No write operations were performed."
  echo "- Do not flash unlock.bin, boot, laf, aboot, or firehose files without exact LM-Q710(FGN) proof."
} | tee "$SUMMARY"

tar -czf "$OUT_DIR.tar.gz" "$OUT_DIR"

echo ""
echo "Done."
echo "Log folder: $OUT_DIR"
echo "Archive: $OUT_DIR.tar.gz"
echo "Summary:"
echo "$SUMMARY"
