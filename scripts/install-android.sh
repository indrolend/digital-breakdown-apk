#!/usr/bin/env bash
set -euo pipefail

APP_ID="com.indrolend.digitalbreakdown"
APK_PATH="android/app/build/outputs/apk/debug/app-debug.apk"

cd "$(dirname "$0")/.."

if [ ! -f "$APK_PATH" ]; then
  echo "Missing APK:"
  echo "$APK_PATH"
  echo "Run ./scripts/build-android.sh first."
  exit 1
fi

echo "== ADB devices =="
adb devices

echo "== Try install =="
set +e
INSTALL_OUTPUT="$(adb install -r "$APK_PATH" 2>&1)"
INSTALL_CODE=$?
set -e

echo "$INSTALL_OUTPUT"

if [ "$INSTALL_CODE" -eq 0 ]; then
  echo "Install succeeded."
else
  if echo "$INSTALL_OUTPUT" | grep -q "INSTALL_FAILED_UPDATE_INCOMPATIBLE"; then
    echo "Signature mismatch detected."
    echo "Uninstalling old package: $APP_ID"
    adb uninstall "$APP_ID" || true

    echo "Reinstalling fresh APK..."
    adb install -r "$APK_PATH"
  else
    echo "Install failed for another reason."
    exit "$INSTALL_CODE"
  fi
fi

echo "== Clear app data =="
adb shell pm clear "$APP_ID" || true

echo "== Launch app =="
adb shell monkey -p "$APP_ID" 1

echo "Done."
