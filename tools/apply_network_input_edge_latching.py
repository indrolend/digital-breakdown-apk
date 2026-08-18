#!/usr/bin/env python3
"""Apply the narrow remote-input edge-latching repair.

Default mode is a dry-run contract check. Pass --apply to rewrite only the four
one-shot boolean edge assignments in Game::setNetworkPeerInput(). The transform
is exact and count-checked so it refuses to operate if the source has drifted.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
GAME_CPP = ROOT / "native-android/app/src/main/cpp/game/Game.cpp"

OLD = (
    "peer.input.jumpPressed=(buttons&(1u<<5))!=0&&(previous&(1u<<5))==0;"
    "peer.input.meleePressed=(buttons&(1u<<7))!=0&&(previous&(1u<<7))==0;"
    "peer.input.shootPressed=(buttons&(1u<<8))!=0&&(previous&(1u<<8))==0;"
    "peer.input.cameraTogglePressed=(buttons&(1u<<9))!=0&&(previous&(1u<<9))==0;"
)

NEW = (
    "peer.input.jumpPressed=peer.input.jumpPressed||((buttons&(1u<<5))!=0&&(previous&(1u<<5))==0);"
    "peer.input.meleePressed=peer.input.meleePressed||((buttons&(1u<<7))!=0&&(previous&(1u<<7))==0);"
    "peer.input.shootPressed=peer.input.shootPressed||((buttons&(1u<<8))!=0&&(previous&(1u<<8))==0);"
    "peer.input.cameraTogglePressed=peer.input.cameraTogglePressed||((buttons&(1u<<9))!=0&&(previous&(1u<<9))==0);"
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true", help="write the edge-latching repair")
    args = parser.parse_args()

    try:
        text = GAME_CPP.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"NETWORK_INPUT_EDGE_LATCH_FAIL {exc}", file=sys.stderr)
        return 1

    count = text.count(OLD)
    if count != 1:
        print(
            f"NETWORK_INPUT_EDGE_LATCH_FAIL expected exactly one source match, found {count}",
            file=sys.stderr,
        )
        return 1

    proposed = text.replace(OLD, NEW, 1)
    print("NETWORK_INPUT_EDGE_LATCH_READY native-android/app/src/main/cpp/game/Game.cpp")

    if not args.apply:
        print("NETWORK_INPUT_EDGE_LATCH_CHECK=PASS")
        return 0

    try:
        GAME_CPP.write_text(proposed, encoding="utf-8")
    except OSError as exc:
        print(f"NETWORK_INPUT_EDGE_LATCH_FAIL {exc}", file=sys.stderr)
        return 1

    print("NETWORK_INPUT_EDGE_LATCH_APPLIED=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
