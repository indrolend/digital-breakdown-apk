#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASELINE="${DB_MAC_BASELINE:-b4e3ecb}"
BUILD_DIR="${DB_MAC_BUILD_DIR:-$ROOT/build/macos-release}"
ARCHITECTURES="${DB_MAC_ARCHITECTURES:-$(uname -m)}"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "This verifier must run on macOS." >&2
  exit 2
fi

git -C "$ROOT" merge-base --is-ancestor "$BASELINE" HEAD || {
  echo "Current source does not descend from verified Mac baseline $BASELINE." >&2
  exit 1
}

echo "Verified baseline: $BASELINE"
echo "Candidate source:  $(git -C "$ROOT" rev-parse --short HEAD)"
echo "Architectures:     $ARCHITECTURES"

rm -rf "$BUILD_DIR"
cmake -S "$ROOT/native-desktop" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$ARCHITECTURES" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build "$BUILD_DIR" --config Release --parallel
ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure

APP="$BUILD_DIR/bin/DigitalBreakdown.app"
EXE="$APP/Contents/MacOS/DigitalBreakdown"
test -x "$EXE"
"$EXE" --smoke-test

IDENTITY="$("$EXE" --build-identity-json)"
printf '%s\n' "$IDENTITY"
printf '%s' "$IDENTITY" | grep -q '"platform":"macos"'
printf '%s' "$IDENTITY" | grep -q '"configuration":"Release"'

for asset in audio fonts models tv-gifs; do
  test -d "$APP/Contents/Resources/$asset" || {
    echo "Missing packaged asset directory: $asset" >&2
    exit 1
  }
done

BINARY_INFO="$(file "$EXE")"
printf '%s\n' "$BINARY_INFO"
IFS=';' read -r -a requested_arches <<< "$ARCHITECTURES"
for architecture in "${requested_arches[@]}"; do
  printf '%s' "$BINARY_INFO" | grep -q "$architecture"
done

echo "MAC_BASELINE_VERIFY_OK baseline=$BASELINE candidate=$(git -C "$ROOT" rev-parse --short HEAD)"
