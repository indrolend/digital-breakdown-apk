#!/data/data/com.termux/files/usr/bin/bash
# db-apk-artifact-download.sh
# Termux-side: Download a GitHub Actions artifact APK to phone shared storage.
#
# Usage:
#   db-apk-artifact-download \
#     --repo indrolend/digital-breakdown-apk \
#     --run 28961104004 \
#     --artifact digital-breakdown-native-debug-apk \
#     --out /sdcard/Download/db-apks/pr2-native-debug.apk
#
# Fallback: if gh auth is unavailable, prints manual instructions.

set -euo pipefail

REPO="indrolend/digital-breakdown-apk"
RUN_ID=""
ARTIFACT_NAME=""
OUT_PATH="/sdcard/Download/db-apks/app-debug.apk"

usage() {
    cat <<'EOF'
Usage: db-apk-artifact-download [OPTIONS]

Options:
  --repo      GitHub repository (default: indrolend/digital-breakdown-apk)
  --run       GitHub Actions run ID (required)
  --artifact  Artifact name (required)
  --out       Output APK path (default: /sdcard/Download/db-apks/app-debug.apk)
  --help      Show this help

Example:
  db-apk-artifact-download \
    --repo indrolend/digital-breakdown-apk \
    --run 28961104004 \
    --artifact digital-breakdown-native-debug-apk \
    --out /sdcard/Download/db-apks/pr2-native-debug.apk
EOF
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

# Validate required args
if [[ -z "$RUN_ID" ]]; then
    echo "[error] --run is required."
    usage
    exit 1
fi
if [[ -z "$ARTIFACT_NAME" ]]; then
    echo "[error] --artifact is required."
    usage
    exit 1
fi

OUT_DIR="$(dirname "$OUT_PATH")"

echo "=== APK Artifact Download ==="
echo "  Repo:     $REPO"
echo "  Run ID:   $RUN_ID"
echo "  Artifact: $ARTIFACT_NAME"
echo "  Output:   $OUT_PATH"
echo ""

# ---------------------------------------------------------------------------
# Check if APK already exists
# ---------------------------------------------------------------------------

if [[ -f "$OUT_PATH" ]]; then
    echo "[info] APK already exists at $OUT_PATH"
    ls -lh "$OUT_PATH"
    read -r -p "Re-download and overwrite? [y/N] " overwrite
    if [[ ! "$overwrite" =~ ^[Yy]$ ]]; then
        echo "[ok] Using existing APK."
        exit 0
    fi
fi

# ---------------------------------------------------------------------------
# Check prerequisites
# ---------------------------------------------------------------------------

if ! command -v gh >/dev/null 2>&1; then
    echo "[fail] GitHub CLI (gh) not installed."
    echo "  Install with: pkg install gh"
    echo ""
    echo "  Manual fallback:"
    echo "    1. Download the APK from GitHub Actions on a browser:"
    echo "       https://github.com/$REPO/actions/runs/$RUN_ID"
    echo "    2. Transfer to phone via USB or cloud."
    exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
    echo "[fail] GitHub CLI not authenticated."
    echo "  Authenticate with: gh auth login"
    echo ""
    echo "  Manual fallback:"
    echo "    Download the APK from GitHub Actions browser:"
    echo "    https://github.com/$REPO/actions/runs/$RUN_ID"
    exit 1
fi

# ---------------------------------------------------------------------------
# Check shared storage
# ---------------------------------------------------------------------------

if [[ ! -d "/sdcard/Download" ]]; then
    echo "[fail] /sdcard/Download not accessible."
    echo "  Grant storage permission: termux-setup-storage"
    exit 1
fi

mkdir -p "$OUT_DIR"

# ---------------------------------------------------------------------------
# Download artifact
# ---------------------------------------------------------------------------

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "[download] Fetching artifact from GitHub Actions ..."
if ! gh run download "$RUN_ID" \
        --repo "$REPO" \
        --name "$ARTIFACT_NAME" \
        --dir "$TMP_DIR"; then
    echo "[fail] Artifact download failed."
    echo "  Check that run ID $RUN_ID exists and artifact '$ARTIFACT_NAME' is available:"
    echo "    gh run list --repo $REPO"
    echo "    gh run view $RUN_ID --repo $REPO"
    exit 1
fi

# Find the APK
APK_FILE="$(find "$TMP_DIR" -name '*.apk' | head -1)"
if [[ -z "$APK_FILE" ]]; then
    echo "[fail] No APK found in downloaded artifact."
    echo "  Contents:"
    ls -la "$TMP_DIR"
    exit 1
fi

echo "[ok] Found APK: $(basename "$APK_FILE")"
cp "$APK_FILE" "$OUT_PATH"

echo ""
echo "[ok] APK saved to: $OUT_PATH"
ls -lh "$OUT_PATH"
echo ""
echo "Next steps (from Windows):"
echo "  . .\\tools\\phone-control\\phone-session.ps1"
echo "  DbApkInstall -DevicePath '$OUT_PATH' -Package com.indrolend.digitalbreakdown.native"
echo ""
echo "Or full demo:"
echo "  DbApkDemo -RunId $RUN_ID -ArtifactName $ARTIFACT_NAME -Package com.indrolend.digitalbreakdown.native"
