#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

MODE="${1:-adb}"
TS="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="logs/lg-probe-$MODE-$TS"
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
  set -e

  keep_if_nonempty "$file"
  return 0
}

write_summary() {
  local summary="$OUT_DIR/summary.txt"

  {
    echo "LG probe summary"
    echo "Mode: $MODE"
    echo "Generated: $(date)"
    echo "Folder: $OUT_DIR"
    echo ""

    for f in \
      "$OUT_DIR/getprop-summary.txt" \
      "$OUT_DIR/fastboot-basic.txt" \
      "$OUT_DIR/fastboot-unlock.txt" \
      "$OUT_DIR/usb-filtered.txt"
    do
      if [ -f "$f" ]; then
        echo "== $(basename "$f") =="
        cat "$f"
        echo ""
      fi
    done
  } > "$summary"

  echo ""
  echo "== Summary =="
  cat "$summary"
}

probe_adb() {
  echo "LG ADB probe"
  echo "Output: $OUT_DIR"
  echo "Read-only. Does not unlock, flash, erase, or modify partitions."

  capture "adb-devices" adb devices -l

  capture_shell "getprop-summary" '
echo "--- identity ---"
echo "model=$(getprop ro.product.model)"
echo "name=$(getprop ro.product.name)"
echo "device=$(getprop ro.product.device)"
echo "manufacturer=$(getprop ro.product.manufacturer)"
echo "hardware=$(getprop ro.hardware)"
echo "platform=$(getprop ro.board.platform)"

echo "--- build ---"
echo "android=$(getprop ro.build.version.release)"
echo "sdk=$(getprop ro.build.version.sdk)"
echo "patch=$(getprop ro.build.version.security_patch)"
echo "fingerprint=$(getprop ro.build.fingerprint)"
echo "bootloader=$(getprop ro.bootloader)"
echo "bootimage_fingerprint=$(getprop ro.bootimage.build.fingerprint)"

echo "--- boot / lock state ---"
echo "oem_unlock_supported=$(getprop ro.oem_unlock_supported)"
echo "oem_unlock_allowed=$(getprop sys.oem_unlock_allowed)"
echo "flash_locked=$(getprop ro.boot.flash.locked)"
echo "verifiedbootstate=$(getprop ro.boot.verifiedbootstate)"
echo "vbmeta_device_state=$(getprop ro.boot.vbmeta.device_state)"
echo "veritymode=$(getprop ro.boot.veritymode)"
echo "warranty_bit=$(getprop ro.boot.warranty_bit)"

echo "--- partitions / slots ---"
echo "slot_suffix=$(getprop ro.boot.slot_suffix)"
echo "dynamic_partitions=$(getprop ro.boot.dynamic_partitions)"
echo "super_partition=$(getprop ro.boot.super_partition)"
echo "logical_partitions=$(getprop ro.boot.logical_partitions)"

echo "--- cpu ---"
echo "abi=$(getprop ro.product.cpu.abi)"
echo "abilist=$(getprop ro.product.cpu.abilist)"
'

  capture_shell "kernel" 'uname -a'

  capture_shell "mounts" 'mount'

  capture_shell "block-devices" '
if [ -d /dev/block/by-name ]; then
  echo "/dev/block/by-name present"
  ls -al /dev/block/by-name
else
  echo "/dev/block/by-name not present"
  echo "Searching likely block symlink directories..."
  find /dev/block -maxdepth 4 -type l 2>/dev/null | sort | head -300
fi
'

  capture_shell "packages-filtered" \
    "pm list packages 2>/dev/null | grep -Ei 'lge|lg|update|boot|carrier|metropcs|tmobile|system' || true"

  capture_shell "su-check" 'command -v su || echo "no su in PATH"'

  write_summary
}

probe_fastboot() {
  echo "LG fastboot probe"
  echo "Output: $OUT_DIR"
  echo "Read-only. Does not unlock, flash, erase, or modify partitions."

  capture "fastboot-devices" fastboot devices

  {
    echo "--- basic ---"
    fastboot getvar product 2>&1 || true
    fastboot getvar variant 2>&1 || true
    fastboot getvar unlocked 2>&1 || true
    fastboot getvar secure 2>&1 || true
    fastboot getvar current-slot 2>&1 || true
    fastboot flashing get_unlock_ability 2>&1 || true
  } | tee "$OUT_DIR/fastboot-basic.txt"
  keep_if_nonempty "$OUT_DIR/fastboot-basic.txt"

  {
    echo "--- unlock-related, read-only ---"
    fastboot oem device-id 2>&1 || true
    fastboot oem device-info 2>&1 || true
  } | tee "$OUT_DIR/fastboot-unlock.txt"
  keep_if_nonempty "$OUT_DIR/fastboot-unlock.txt"

  capture "fastboot-all" fastboot getvar all

  write_summary
}

probe_download() {
  echo "LG Download Mode / LAF USB probe"
  echo "Output: $OUT_DIR"
  echo "Read-only. Does not use LGUP, LAF writes, firehose, flash, or erase."
  echo ""
  echo "Expected phone state: powered off, hold Volume Up, plug USB, wait for Download/Firmware screen."

  capture "system-profiler-usb" system_profiler SPUSBDataType

  if [ -f "$OUT_DIR/system-profiler-usb.txt" ]; then
    grep -i -A 25 -B 5 "LG\|LGE\|Qualcomm\|Android\|QHSUSB\|9008\|Download\|Firmware" \
      "$OUT_DIR/system-profiler-usb.txt" \
      | tee "$OUT_DIR/usb-filtered.txt" || true
    keep_if_nonempty "$OUT_DIR/usb-filtered.txt"
  fi

  write_summary
}

usage() {
  cat <<'TXT'
Usage:
  ./scripts/probe-lg.sh adb
  ./scripts/probe-lg.sh fastboot
  ./scripts/probe-lg.sh download
  ./scripts/probe-lg.sh full

Modes:
  adb       Read Android properties, mounts, block layout, su presence.
  fastboot  Read fastboot variables and LG device-id. No unlock/flash.
  download  Read Mac USB visibility for LG Download Mode/LAF.
  full      Run adb probe, then tell you to manually enter fastboot/download.

Safety:
  This script does not run flash, erase, format, unlock, set_active, LGUP, QFIL, or firehose commands.
TXT
}

case "$MODE" in
  adb)
    probe_adb
    ;;
  fastboot)
    probe_fastboot
    ;;
  download)
    probe_download
    ;;
  full)
    probe_adb
    echo ""
    echo "ADB probe complete."
    echo "For fastboot mode, manually run:"
    echo "  adb reboot bootloader"
    echo "  ./scripts/probe-lg.sh fastboot"
    echo "  fastboot reboot"
    echo ""
    echo "For Download Mode, power off, hold Volume Up, plug USB, then run:"
    echo "  ./scripts/probe-lg.sh download"
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    echo "Unknown mode: $MODE"
    usage
    exit 2
    ;;
esac

find "$OUT_DIR" -type f -name "*.txt" -size 0 -delete 2>/dev/null || true

echo ""
echo "Done."
echo "$OUT_DIR"
