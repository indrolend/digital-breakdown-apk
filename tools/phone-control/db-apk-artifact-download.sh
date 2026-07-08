#!/data/data/com.termux/files/usr/bin/bash
# db-apk-artifact-download.sh
# Termux-side: download GitHub Actions APK artifact to phone stable path.

set -euo pipefail

REPO="indrolend/digital-breakdown-apk"
RUN_ID=""
ARTIFACT_NAME=""
OUT_PATH="/sdcard/Download/db-control/apks/current.apk"

usage() {
    cat <<'USAGE'
Usage: db-apk-artifact-download [OPTIONS]

Options:
  --repo      GitHub repository (default: indrolend/digital-breakdown-apk)
  --run       GitHub Actions run ID (required)
  --artifact  Artifact name (required)
  --out       Output APK path (default: /sdcard/Download/db-control/apks/current.apk)
  --help      Show help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --repo)      REPO="$2";          shift 2 ;;
        --run)       RUN_ID="$2";        shift 2 ;;
        --artifact)  ARTIFACT_NAME="$2"; shift 2 ;;
        --out)       OUT_PATH="$2";      shift 2 ;;
        --help|-h)   usage; exit 0 ;;
        *) echo "[error] Unknown argument: $1"; usage; exit 1 ;;
    esac
done

if [[ -z "$RUN_ID" || -z "$ARTIFACT_NAME" ]]; then
    echo "[error] --run and --artifact are required."
    usage
    exit 1
fi

if [[ ! -d "/sdcard/Download" ]]; then
    echo "[fail] /sdcard/Download not accessible. Run: termux-setup-storage"
    exit 1
fi

OUT_DIR="$(dirname "$OUT_PATH")"
mkdir -p "$OUT_DIR"

if [[ -f "$OUT_PATH" ]]; then
    echo "[info] APK already exists at $OUT_PATH"
    read -r -p "Overwrite current.apk? [y/N] " overwrite
    if [[ ! "$overwrite" =~ ^[Yy]$ ]]; then
        echo "[ok] Keeping existing APK."
        exit 0
    fi
fi

if ! command -v gh >/dev/null 2>&1; then
    echo "[fail] gh not installed. Run: pkg install gh"
    exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
    echo "[fail] gh not authenticated. Run: gh auth login"
    exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "[download] Fetching artifact '$ARTIFACT_NAME' from run $RUN_ID ..."
gh run download "$RUN_ID" \
    --repo "$REPO" \
    --name "$ARTIFACT_NAME" \
    --dir "$TMP_DIR"

APK_FILE="$(find "$TMP_DIR" -name '*.apk' | head -1)"
if [[ -z "$APK_FILE" ]]; then
    echo "[fail] No APK found in downloaded artifact."
    ls -la "$TMP_DIR"
    exit 1
fi

cp "$APK_FILE" "$OUT_PATH"
echo "[ok] APK saved to: $OUT_PATH"
ls -lh "$OUT_PATH"
