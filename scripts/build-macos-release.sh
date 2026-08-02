#!/usr/bin/env bash
set -euo pipefail

export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="${DB_MAC_DIST_DIR:-$ROOT/dist/macos}"
ARCHITECTURES="${DB_MAC_ARCHITECTURES:-x86_64;arm64}"
COMMIT="$(git -C "$ROOT" rev-parse HEAD)"
SHORT_COMMIT="$(git -C "$ROOT" rev-parse --short=7 HEAD)"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "This release builder must run on macOS." >&2
  exit 2
fi

command -v git >/dev/null
command -v cmake >/dev/null
command -v ditto >/dev/null
command -v shasum >/dev/null
xcode-select -p >/dev/null

if [ -n "$(git -C "$ROOT" status --porcelain --untracked-files=no)" ]; then
  echo "Tracked source changes are present; refusing an ambiguous release build." >&2
  git -C "$ROOT" status --short >&2
  exit 1
fi

node "$ROOT/tools/release/protocol-consistency.mjs"

DB_MAC_ARCHITECTURES="$ARCHITECTURES" \
  bash "$ROOT/scripts/verify-macos-baseline.sh"

APP="$ROOT/build/macos-release/bin/DigitalBreakdown.app"
EXE="$APP/Contents/MacOS/DigitalBreakdown"
ZIP="$DIST_DIR/DigitalBreakdown-macOS-Universal-${SHORT_COMMIT}.zip"

test -d "$APP"
test -x "$EXE"
mkdir -p "$DIST_DIR"

file "$EXE" | tee "$DIST_DIR/macos-binary-${SHORT_COMMIT}.txt"
file "$EXE" | grep -q 'x86_64'
file "$EXE" | grep -q 'arm64'

rm -f "$ZIP" "$ZIP.sha256"
ditto -c -k --sequesterRsrc --keepParent "$APP" "$ZIP"
(
  cd "$DIST_DIR"
  shasum -a 256 "$(basename "$ZIP")" > "$(basename "$ZIP").sha256"
)

cat > "$DIST_DIR/build-info-${SHORT_COMMIT}.json" <<EOF
{
  "name": "Digital Breakdown",
  "commit": "$COMMIT",
  "shortCommit": "$SHORT_COMMIT",
  "platform": "macos-universal-x86_64-arm64",
  "configuration": "Release",
  "deploymentTarget": "11.0",
  "codeSigned": false,
  "notarized": false
}
EOF

echo "MAC_RELEASE_BUILD_OK commit=$COMMIT"
echo "Package:  $ZIP"
echo "Checksum: $ZIP.sha256"
