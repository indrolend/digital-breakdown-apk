#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build/macos-release"

export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
cd "$ROOT"

if [ ! -f "$BUILD/CMakeCache.txt" ]; then
  cmake -S native-desktop -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
fi
cmake --build "$BUILD" --config Release --parallel
open "$BUILD/bin/DigitalBreakdown.app"
