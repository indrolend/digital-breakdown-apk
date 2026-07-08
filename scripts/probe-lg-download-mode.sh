#!/usr/bin/env bash
set -u

OUT_DIR="logs/lg-download-mode-probe-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT_DIR"

echo "LG Download Mode / LAF detection probe"
echo "Output: $OUT_DIR"
echo "This is read-only. It does not flash anything."
echo ""

system_profiler SPUSBDataType 2>&1 | tee "$OUT_DIR/system-profiler-usb.txt"

grep -i -A 25 -B 5 "LG\|LGE\|Qualcomm\|Android\|QHSUSB\|9008\|Download\|Firmware" \
  "$OUT_DIR/system-profiler-usb.txt" \
  | tee "$OUT_DIR/usb-filtered.txt" || true

echo "$OUT_DIR"
