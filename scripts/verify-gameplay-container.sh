#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${DB_GAMEPLAY_IMAGE:-digital-breakdown/gameplay-toolchain:ubuntu24.04}"
DOCKERFILE="$ROOT/tools/container/gameplay.Dockerfile"

command -v docker >/dev/null 2>&1 || {
  echo "Docker is required." >&2
  exit 2
}

docker build \
  --file "$DOCKERFILE" \
  --tag "$IMAGE" \
  "$ROOT/tools/container"

docker run --rm \
  --mount "type=bind,src=$ROOT,dst=/src,readonly" \
  --workdir /src \
  "$IMAGE" \
  bash -lc 'git config --global --add safe.directory /src && ./scripts/verify-gameplay.sh /tmp/gameplay-checks'
