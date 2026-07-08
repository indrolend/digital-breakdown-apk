#!/usr/bin/env bash
set -euo pipefail

REPO="${REPO:-indrolend/digital-breakdown-apk}"
WORKFLOW_NAME="${WORKFLOW_NAME:-native-android.yml}"
ARTIFACT_NAME="${ARTIFACT_NAME:-digital-breakdown-native-debug-apk}"
APP_ID="${APP_ID:-com.indrolend.digitalbreakdown.native}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STATE_DIR="$ROOT/logs/native-watch"
LAST_RUN_FILE="$STATE_DIR/last-run-id.txt"
mkdir -p "$STATE_DIR"

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || { echo "Missing command: $1"; exit 2; }
}

need_cmd gh
need_cmd adb

gh auth status >/dev/null

RUN_JSON="$(gh run list \
  --repo "$REPO" \
  --workflow "$WORKFLOW_NAME" \
  --branch main \
  --status success \
  --limit 1 \
  --json databaseId,headSha,displayTitle,createdAt,status,conclusion)"

RUN_ID="$(printf '%s' "$RUN_JSON" | python3 -c 'import json,sys; d=json.load(sys.stdin); print(d[0]["databaseId"] if d else "")')"
HEAD_SHA="$(printf '%s' "$RUN_JSON" | python3 -c 'import json,sys; d=json.load(sys.stdin); print(d[0]["headSha"] if d else "")')"

if [ -z "$RUN_ID" ]; then
  echo "No successful native workflow run found."
  exit 0
fi

LAST_RUN=""
if [ -f "$LAST_RUN_FILE" ]; then LAST_RUN="$(cat "$LAST_RUN_FILE")"; fi

if [ "$RUN_ID" = "$LAST_RUN" ]; then
  echo "No new artifact. Last processed run is already $RUN_ID."
  exit 0
fi

RUN_DIR="$STATE_DIR/run-$RUN_ID"
rm -rf "$RUN_DIR"
mkdir -p "$RUN_DIR/artifact"

echo "== Download artifact $ARTIFACT_NAME from run $RUN_ID =="
gh run download "$RUN_ID" --repo "$REPO" --name "$ARTIFACT_NAME" --dir "$RUN_DIR/artifact"
echo "$RUN_ID" > "$RUN_DIR/run-id.txt"
echo "$HEAD_SHA" > "$RUN_DIR/head-sha.txt"

APK="$(find "$RUN_DIR/artifact" -name "*.apk" -type f | head -1)"
if [ -z "$APK" ]; then
  echo "No APK found in artifact."
  find "$RUN_DIR/artifact" -type f -maxdepth 5
  exit 1
fi

echo "APK: $APK"
adb wait-for-device
adb install -r "$APK"
adb logcat -c || true
adb shell monkey -p "$APP_ID" 1
sleep 5

{
  echo "-- foreground --"
  adb shell dumpsys activity activities | grep -E "mResumedActivity|topResumedActivity" | tail -8 || true
  echo ""
  echo "-- process --"
  adb shell pidof "$APP_ID" || true
  echo ""
  echo "-- filtered logcat --"
  adb logcat -d -t 1000 | grep -E "DBNATIVE|AndroidRuntime|FATAL EXCEPTION|Fatal signal|ANR in|am_crash|has stopped" || true
} | tee "$RUN_DIR/status.txt"

adb exec-out screencap -p > "$RUN_DIR/screen-after-launch.png"
adb logcat -d -t 1500 > "$RUN_DIR/logcat.txt"

if grep -Ei "FATAL EXCEPTION|AndroidRuntime|ANR in|am_crash|Fatal signal|has stopped|Force finishing" "$RUN_DIR/logcat.txt" > "$RUN_DIR/crash.txt"; then
  echo "FAIL: crash lines found"
  cat "$RUN_DIR/crash.txt"
  exit 1
fi

: > "$RUN_DIR/crash.txt"
echo "$RUN_ID" > "$LAST_RUN_FILE"
echo "PASS: artifact downloaded, installed, launched, and probed."
echo "Artifacts: $RUN_DIR"
