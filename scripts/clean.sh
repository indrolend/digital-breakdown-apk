#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

MODE="${1:-standard}"

mkdir -p logs logs/_archives

echo "Clean mode: $MODE"

echo ""
echo "== Delete empty txt logs =="
find logs -type f -name "*.txt" -size 0 -print -delete 2>/dev/null || true

echo ""
echo "== Remove generated Capacitor files =="
rm -rf \
  android/app/src/main/assets \
  android/app/src/main/res/xml/config.xml \
  android/capacitor-cordova-android-plugins

echo ""
echo "== Remove generated web bundle =="
rm -f www/android-bundle.js

if [ "$MODE" = "logs" ] || [ "$MODE" = "all" ]; then
  echo ""
  echo "== Archive probe/status folders older than 7 days =="
  while IFS= read -r dir; do
    [ -d "$dir" ] || continue
    base="$(basename "$dir")"
    archive="logs/_archives/$base.tar.gz"

    if [ ! -f "$archive" ]; then
      echo "Archiving $dir -> $archive"
      tar -czf "$archive" "$dir"
    fi

    rm -rf "$dir"
  done < <(
    find logs -maxdepth 1 -type d \
      \( -name "root-probe-*" \
      -o -name "lg-q710-interrogation-*" \
      -o -name "lg-download-mode-probe-*" \
      -o -name "lg-probe-*" \
      -o -name "postboot-status-*" \) \
      -mtime +7 \
      -print
  )
fi

if [ "$MODE" = "all" ]; then
  echo ""
  echo "== Remove old archives over 14 days =="
  find logs/_archives -type f -name "*.tar.gz" -mtime +14 -print -delete 2>/dev/null || true
fi

echo ""
echo "== Remove empty folders =="
find logs -type d -empty -print -delete 2>/dev/null || true

echo ""
echo "== Disk usage =="
du -sh logs 2>/dev/null || true

echo ""
echo "Done."
