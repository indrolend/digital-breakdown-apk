#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mkdir -p logs

echo "== Build web bundle =="
npx esbuild ./www/android-entry.mjs \
  --bundle \
  --platform=browser \
  --format=iife \
  --target=chrome61 \
  --outfile=./www/android-bundle.js \
  --log-level=info

echo "== Capacitor sync =="
npx cap sync android

echo "== Gradle build =="
cd android
./gradlew assembleDebug

echo "Done."
echo "APK: $ROOT/android/app/build/outputs/apk/debug/app-debug.apk"
