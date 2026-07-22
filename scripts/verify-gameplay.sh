#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/gameplay-checks}"

cmake -S native-desktop -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release --target \
  GameplayRoleAndSoulMotionTest \
  Pass7ParityTest \
  MultiplayerProtocolTest \
  --parallel

ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure
"$BUILD_DIR/Pass7ParityTest"
"$BUILD_DIR/MultiplayerProtocolTest"

git diff --check
