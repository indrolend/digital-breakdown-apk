#!/usr/bin/env python3
"""Validate the small set of version fields the project currently uses."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "version.json"
SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")
STAGES = {"development", "alpha", "beta", "rc", "stable"}


def fail(message: str) -> None:
    print(f"version validation failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    required = {
        "schemaVersion",
        "gameVersion",
        "releaseStage",
        "protocolVersion",
        "androidVersionCodeBase",
    }
    missing = sorted(required - data.keys())
    if missing:
        fail(f"missing fields: {', '.join(missing)}")

    unknown = sorted(set(data) - required)
    if unknown:
        fail(f"unknown fields: {', '.join(unknown)}")

    if not SEMVER.fullmatch(str(data["gameVersion"])):
        fail("gameVersion must be MAJOR.MINOR.PATCH without a suffix")
    if data["releaseStage"] not in STAGES:
        fail(f"releaseStage must be one of {sorted(STAGES)}")

    for field in ("schemaVersion", "protocolVersion", "androidVersionCodeBase"):
        value = data[field]
        if not isinstance(value, int) or value < 1:
            fail(f"{field} must be a positive integer")

    if data["androidVersionCodeBase"] % 1000 != 0:
        fail("androidVersionCodeBase must reserve the final three digits for builds")

    print(
        "version manifest OK: "
        f"{data['gameVersion']}-{data['releaseStage']} "
        f"protocol={data['protocolVersion']}"
    )


if __name__ == "__main__":
    main()
