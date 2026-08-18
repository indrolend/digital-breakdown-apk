#!/usr/bin/env python3
"""Fail when broad mutable GameState access spreads or silently regrows.

This is a ratchet, not an endorsement of the remaining call sites. Each known
production owner has an exact current call-count budget. A reduction must lower
that budget in the same change, so removed debt cannot later grow back under a
stale file-level allowance.
"""

from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
TOKEN = "networkMutableState()"

# Exact fixed-point surface after the current ownership migrations.
# If a cleanup removes a call, lower the corresponding budget in that same
# change. Never raise a budget merely to make this check pass.
ALLOWED_PRODUCTION_COUNTS = {
    Path("native-desktop/main.cpp"): 19,  # desktop UI/session + built-in stress fixtures
    Path("native-network/MultiplayerProtocol.cpp"): 1,  # authoritative snapshot transaction owner
    Path("native-android/app/src/main/cpp/game/Game.hpp"): 1,  # declaration itself
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
        if path not in ALLOWED_PRODUCTION_COUNTS
    }

    expanded: dict[Path, tuple[int, int]] = {}
    shrunk: dict[Path, tuple[int, int]] = {}

    print("OWNERSHIP_MUTABLE_STATE_SURFACE")
    for path in sorted(set(hits) | set(ALLOWED_PRODUCTION_COUNTS), key=str):
        lines = hits.get(path, [])
        actual = len(lines)
        budget = ALLOWED_PRODUCTION_COUNTS.get(path)
        if budget is None:
            status = "UNOWNED"
        elif actual > budget:
            status = "EXPANDED"
            expanded[path] = (actual, budget)
        elif actual < budget:
            status = "SHRUNK_UNRECORDED"
            shrunk[path] = (actual, budget)
        else:
            status = "FIXED"
        suffix = f" lines={','.join(map(str, lines))}" if lines else ""
        budget_text = f" budget={budget}" if budget is not None else ""
        print(f"{status} {path} count={actual}{budget_text}{suffix}")

    if unexpected:
        print("OWNERSHIP_BOUNDARY_FAIL: networkMutableState() spread to a new production file.")
        for path in sorted(unexpected, key=str):
            print(f"  {path}: lines={unexpected[path]}")

    if expanded:
        print("OWNERSHIP_BOUNDARY_FAIL: grandfathered mutable-state debt expanded.")
        for path in sorted(expanded, key=str):
            actual, budget = expanded[path]
            print(f"  {path}: count={actual} budget={budget}")

    if shrunk:
        print("OWNERSHIP_BOUNDARY_FAIL: mutable-state debt shrank; lower the recorded budget in this same change.")
        for path in sorted(shrunk, key=str):
            actual, budget = shrunk[path]
            print(f"  {path}: count={actual} old_budget={budget} new_budget={actual}")

    if unexpected or expanded or shrunk:
        return 1

    print("OWNERSHIP_BOUNDARY_PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
