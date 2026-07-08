#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "== Remove empty probe txt files =="
find logs -type f -name "*.txt" -size 0 -print -delete 2>/dev/null || true

echo ""
echo "== Remove generated Capacitor Android files =="
rm -rf \
  android/app/src/main/assets \
  android/app/src/main/res/xml/config.xml \
  android/capacitor-cordova-android-plugins

echo ""
echo "== Remove generated web bundle =="
rm -f www/android-bundle.js

echo ""
echo "== Remove old probe archives over 14 days =="
find logs -type f -name "*.tar.gz" -mtime +14 -print -delete 2>/dev/null || true

echo ""
echo "== Remove old empty log folders =="
find logs -type d -empty -print -delete 2>/dev/null || true

echo ""
echo "Done."
