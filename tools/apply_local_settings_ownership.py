#!/usr/bin/env python3
"""Guard the pending LocalSettingsState ownership migration.

The original migration proposed whole-LocalSettingsState replacement through a
named Game API. Characterization has since proven that this aggregate mixes
persistent preferences, menu/session transients, and `mobileFraming`, which
materially changes authoritative remote-player combat direction.

Default mode verifies that the known ownership seams have not drifted and
reports the migration as intentionally blocked. `--apply` refuses to write
until `mobileFraming` receives an explicit gameplay/network ownership policy.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

GAME_HPP = ROOT / "native-android/app/src/main/cpp/game/Game.hpp"
DESKTOP_MAIN = ROOT / "native-desktop/main.cpp"
ANDROID_BRIDGE = ROOT / "native-android/app/src/main/cpp/native_bridge.cpp"

EXPECTED = {
    GAME_HPP: [
        (
            "    void setPersistentProgression(std::int64_t tokens, int shotLevel, int lungeLevel, int attackLevel);\n",
            "Game owner API insertion point",
        ),
        (
            "    GameState& networkMutableState() { return state_; }\n",
            "broad mutable escape hatch still present",
        ),
    ],
    DESKTOP_MAIN: [
        (
            "    if(version>=2)game.networkMutableState().localSettings=settings;\n",
            "desktop persistence still uses broad mutable state",
        ),
    ],
    ANDROID_BRIDGE: [
        (
            "auto& settings=gGame.networkMutableState().localSettings;",
            "Android settings still use broad mutable state",
        ),
        (
            "settings.mobileFraming=true;",
            "Android still forces the gameplay-coupled mobileFraming policy",
        ),
    ],
}


def require_exact(text: str, needle: str, label: str) -> None:
    count = text.count(needle)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one source match, found {count}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--apply",
        action="store_true",
        help="reserved for a future evidence-backed settings ownership migration",
    )
    args = parser.parse_args()

    try:
        for path, expectations in EXPECTED.items():
            text = path.read_text(encoding="utf-8")
            for needle, label in expectations:
                require_exact(text, needle, label)
                print(f"LOCAL_SETTINGS_OWNERSHIP_OBSERVED {path.relative_to(ROOT)} {label}")
    except (OSError, RuntimeError) as exc:
        print(f"LOCAL_SETTINGS_OWNERSHIP_FAIL {exc}", file=sys.stderr)
        return 1

    print(
        "LOCAL_SETTINGS_OWNERSHIP_BLOCKED "
        "whole LocalSettingsState replacement is not a valid settled boundary; "
        "mobileFraming materially affects authoritative remote combat"
    )

    if args.apply:
        print(
            "LOCAL_SETTINGS_OWNERSHIP_APPLY_REFUSED "
            "define mobileFraming gameplay/network ownership before migrating production callers",
            file=sys.stderr,
        )
        return 2

    print("LOCAL_SETTINGS_OWNERSHIP_CHECK=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
