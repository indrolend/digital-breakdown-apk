#!/usr/bin/env python3
"""Fail when broad mutable GameState access spreads to new production files.

This is intentionally a ratchet, not a claim that the existing call sites are good.
Current production users are grandfathered only so ownership cleanup can proceed
one category at a time without allowing new files to acquire generic mutation
authority in the meantime.
"""

from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
TOKEN = "networkMutableState()"

# Transitional/owned production uses after the current ownership migrations.
# Entries should only be removed when an even narrower named boundary replaces
# them; new files must not be added merely to make this check pass.
ALLOWED_PRODUCTION_FILES = {
    Path("native-desktop/main.cpp"),  # desktop UI/session + built-in stress fixtures
    Path("native-network/MultiplayerProtocol.cpp"),  # authoritative snapshot transaction owner
    Path("native-android/app/src/main/cpp/game/Game.hpp"),  # declaration itself
}

PRODUCTION_ROOTS = (
    Path("native-desktop"),
    Path("native-network"),
    Path("native-android/app/src/main/cpp"),
)


def source_files(root: Path):
    for suffix in ("*.cpp", "*.hpp", "*.cc", "*.cxx", "*.h"):
        yield from root.rglob(suffix)


def main() -> int:
    hits: dict[Path, list[int]] = {}
    for relative_root in PRODUCTION_ROOTS:
        absolute_root = ROOT / relative_root
        if not absolute_root.exists():
            continue
        for path in source_files(absolute_root):
            text = path.read_text(encoding="utf-8", errors="replace")
            lines = [
                number
                for number, line in enumerate(text.splitlines(), 1)
                if TOKEN in line
            ]
            if lines:
                hits[path.relative_to(ROOT)] = lines

    unexpected = {
        path: lines
        for path, lines in hits.items()
        if path not in ALLOWED_PRODUCTION_FILES
    }

    print("OWNERSHIP_MUTABLE_STATE_SURFACE")
    for path in sorted(hits, key=str):
        status = "TRANSITIONAL" if path in ALLOWED_PRODUCTION_FILES else "UNOWNED"
        print(f"{status} {path} lines={','.join(map(str, hits[path]))}")

    missing = sorted(ALLOWED_PRODUCTION_FILES - hits.keys(), key=str)
    for path in missing:
        print(f"MIGRATED {path}")

    if unexpected:
        print("OWNERSHIP_BOUNDARY_FAIL: networkMutableState() spread to new production files.")
        for path in sorted(unexpected, key=str):
            print(f"  {path}: {unexpected[path]}")
        return 1

    print("OWNERSHIP_BOUNDARY_PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
