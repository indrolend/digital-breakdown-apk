#!/usr/bin/env python3
"""Fail when broad mutable GameState access spreads or silently regrows.

This is a ratchet, not an endorsement of the remaining call sites. Each known
production owner has an exact current call-count budget. Desktop call sites are
also classified by semantic owner so real UI/session mutation cannot silently
trade places with built-in fixture/stress mutation while preserving the same
coarse file count.
"""

from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
TOKEN = "networkMutableState()"
DESKTOP_MAIN = Path("native-desktop/main.cpp")

# Exact fixed-point surface after the current ownership migrations.
# If a cleanup removes a call, lower the corresponding budget in that same
# change. Never raise a budget merely to make this check pass.
ALLOWED_PRODUCTION_COUNTS = {
    DESKTOP_MAIN: 27,
    Path("native-network/MultiplayerProtocol.cpp"): 1,  # authoritative snapshot transaction owner
    Path("native-android/app/src/main/cpp/game/Game.hpp"): 1,  # declaration itself
}

# Real desktop menu/session mutation is intentionally concentrated in these
# named functions. Each currently owns one broad mutation transaction. This is
# separate from capture/demo/stress/parity fixture authority below.
DESKTOP_LIVE_UI_SESSION_COUNTS = {
    "setMenuPageDirect": 1,
    "openMenuRoot": 1,
    "pushMenuPage": 1,
    "popMenuPage": 1,
    "adjustMenuSetting": 1,
    "toggleMenuSetting": 1,
    "setMenuSelection": 1,
    "activateMenuSelection": 1,
    "controllerMenuBack": 1,
    "keyCallback": 1,
}
DESKTOP_FIXTURE_COUNT = 17

PRODUCTION_ROOTS = (
    Path("native-desktop"),
    Path("native-network"),
    Path("native-android/app/src/main/cpp"),
)

# main.cpp keeps top-level helper definitions at column zero. For each broad
# mutation line, the nearest preceding top-level function definition is its
# semantic owner. This deliberately avoids pretending to parse arbitrary C++.
TOP_LEVEL_FUNCTION = re.compile(
    r"^[A-Za-z_~][A-Za-z0-9_:<>,*& \t]*\b([A-Za-z_][A-Za-z0-9_]*)"
    r"\s*\([^;{}]*\)\s*(?:const\s*)?\{"
)


def source_files(root: Path):
    for suffix in ("*.cpp", "*.hpp", "*.cc", "*.cxx", "*.h"):
        yield from root.rglob(suffix)


def top_level_owners(lines: list[str]) -> dict[int, str | None]:
    owner: str | None = None
    owners: dict[int, str | None] = {}
    for number, line in enumerate(lines, 1):
        match = TOP_LEVEL_FUNCTION.match(line)
        if match:
            owner = match.group(1)
        owners[number] = owner
    return owners


def is_fixture_owner(owner: str | None) -> bool:
    if owner == "main":
        return True
    if not owner or not owner.startswith("run"):
        return False
    return any(marker in owner for marker in ("Stress", "Capture", "Parity", "Test", "Probe"))


def classify_desktop_calls(lines: list[str], hit_lines: list[int]):
    owners = top_level_owners(lines)
    live: dict[str, list[int]] = {name: [] for name in DESKTOP_LIVE_UI_SESSION_COUNTS}
    fixture: dict[str, list[int]] = {}
    unknown: dict[str, list[int]] = {}

    for number in hit_lines:
        owner = owners.get(number)
        if owner in live:
            live[owner].append(number)
        elif is_fixture_owner(owner):
            fixture.setdefault(owner or "<unknown>", []).append(number)
        else:
            unknown.setdefault(owner or "<unknown>", []).append(number)
    return live, fixture, unknown


def main() -> int:
    hits: dict[Path, list[int]] = {}
    source_lines: dict[Path, list[str]] = {}
    for relative_root in PRODUCTION_ROOTS:
        absolute_root = ROOT / relative_root
        if not absolute_root.exists():
            continue
        for path in source_files(absolute_root):
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            relative = path.relative_to(ROOT)
            source_lines[relative] = lines
            token_lines = [
                number
                for number, line in enumerate(lines, 1)
                if TOKEN in line
            ]
            if token_lines:
                hits[relative] = token_lines

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

    desktop_live_mismatch: dict[str, tuple[int, int]] = {}
    desktop_fixture_actual = 0
    desktop_unknown: dict[str, list[int]] = {}
    if DESKTOP_MAIN in source_lines:
        live, fixtures, desktop_unknown = classify_desktop_calls(
            source_lines[DESKTOP_MAIN], hits.get(DESKTOP_MAIN, [])
        )
        print("OWNERSHIP_DESKTOP_MUTATION_CLASSES")
        for owner, expected in DESKTOP_LIVE_UI_SESSION_COUNTS.items():
            owner_lines = live[owner]
            actual = len(owner_lines)
            if actual != expected:
                desktop_live_mismatch[owner] = (actual, expected)
            print(
                f"LIVE_UI_SESSION {owner} count={actual} budget={expected}"
                + (f" lines={','.join(map(str, owner_lines))}" if owner_lines else "")
            )
        for owner in sorted(fixtures):
            owner_lines = fixtures[owner]
            desktop_fixture_actual += len(owner_lines)
            print(
                f"FIXTURE {owner} count={len(owner_lines)}"
                + f" lines={','.join(map(str, owner_lines))}"
            )
        for owner in sorted(desktop_unknown):
            owner_lines = desktop_unknown[owner]
            print(
                f"UNCLASSIFIED {owner} count={len(owner_lines)}"
                + f" lines={','.join(map(str, owner_lines))}"
            )
        print(
            f"FIXTURE_TOTAL count={desktop_fixture_actual} budget={DESKTOP_FIXTURE_COUNT}"
        )

    desktop_fixture_mismatch = desktop_fixture_actual != DESKTOP_FIXTURE_COUNT

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

    if desktop_live_mismatch:
        print("OWNERSHIP_BOUNDARY_FAIL: live desktop UI/session mutation ownership changed.")
        for owner in sorted(desktop_live_mismatch):
            actual, budget = desktop_live_mismatch[owner]
            print(f"  {owner}: count={actual} budget={budget}")

    if desktop_fixture_mismatch:
        print("OWNERSHIP_BOUNDARY_FAIL: desktop fixture mutation budget changed.")
        print(f"  fixtures: count={desktop_fixture_actual} budget={DESKTOP_FIXTURE_COUNT}")

    if desktop_unknown:
        print("OWNERSHIP_BOUNDARY_FAIL: desktop broad mutation has no semantic owner class.")
        for owner in sorted(desktop_unknown):
            print(f"  {owner}: lines={desktop_unknown[owner]}")

    if unexpected or expanded or shrunk or desktop_live_mismatch or desktop_fixture_mismatch or desktop_unknown:
        return 1

    print("OWNERSHIP_BOUNDARY_PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
