#!/usr/bin/env python3
"""Validate the authoritative Digital Breakdown version manifest."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "version.json"
SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")
STAGES = {"development", "alpha", "beta", "rc", "stable"}
CHANNELS = {"development", "playtest", "preview", "stable"}


def fail(message: str) -> None:
    print(f"version validation failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    required = {
        "schemaVersion",
        "gameVersion",
        "releaseStage",
        "milestone",
        "protocolVersion",
        "gameplayVersion",
        "minimumCompatibleProtocol",
        "saveSchemaVersion",
        "assetSchemaVersion",
        "androidVersionCodeBase",
        "releaseChannel",
    }
    missing = sorted(required - data.keys())
    if missing:
        fail(f"missing fields: {', '.join(missing)}")

    if not SEMVER.fullmatch(str(data["gameVersion"])):
        fail("gameVersion must be MAJOR.MINOR.PATCH without a suffix")
    if data["releaseStage"] not in STAGES:
        fail(f"releaseStage must be one of {sorted(STAGES)}")
    if data["releaseChannel"] not in CHANNELS:
        fail(f"releaseChannel must be one of {sorted(CHANNELS)}")
    if data["releaseStage"] == "stable" and data["releaseChannel"] != "stable":
        fail("stable releases must use the stable channel")
    if data["releaseStage"] != "stable" and data["releaseChannel"] == "stable":
        fail("non-stable releases cannot use the stable channel")

    integer_fields = [
        "schemaVersion",
        "protocolVersion",
        "gameplayVersion",
        "minimumCompatibleProtocol",
        "saveSchemaVersion",
        "assetSchemaVersion",
        "androidVersionCodeBase",
    ]
    for field in integer_fields:
        value = data[field]
        if not isinstance(value, int) or value < 1:
            fail(f"{field} must be a positive integer")

    if data["minimumCompatibleProtocol"] > data["protocolVersion"]:
        fail("minimumCompatibleProtocol cannot exceed protocolVersion")
    if data["androidVersionCodeBase"] % 1000 != 0:
        fail("androidVersionCodeBase must reserve the final three digits for CI builds")
    if not str(data["milestone"]).strip():
        fail("milestone cannot be blank")

    print(
        "version manifest OK: "
        f"{data['gameVersion']}-{data['releaseStage']} "
        f"protocol={data['protocolVersion']} save={data['saveSchemaVersion']} "
        f"assets={data['assetSchemaVersion']}"
    )


if __name__ == "__main__":
    main()
