#!/usr/bin/env python3
"""Adopt the canonical PhoneBody geometry contract in Game.cpp.

The transform is narrow, count-checked, and idempotent. It can be run manually
or from automation to replace duplicated numeric declarations with aliases to
gameplay::PHONE_BODY while preserving existing call sites and runtime values.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

GAME_CPP = Path("native-android/app/src/main/cpp/game/Game.cpp")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, found {count}")
    return text.replace(old, new, 1)


def is_fully_adopted(source: str) -> bool:
    required = (
        '#include "gameplay/PhoneBody.hpp"',
        'constexpr float PLAYER_CEILING_BODY_CLEARANCE = gameplay::PHONE_BODY.ceilingClearance;',
        'constexpr float PLAYER_COLLISION_RADIUS = gameplay::PHONE_BODY.collisionRadius;',
        'constexpr float PLAYER_WALL_MARGIN = gameplay::PHONE_BODY.wallMargin;',
        'constexpr float PLAYER_SUPPORT_RADIUS = gameplay::PHONE_BODY.supportRadius;',
        'constexpr float LEDGE_GRAB_VERTICAL_BELOW = gameplay::PHONE_BODY.ledgeGrabVerticalBelow;',
        'constexpr float LEDGE_GRAB_VERTICAL_ABOVE = gameplay::PHONE_BODY.ledgeGrabVerticalAbove;',
        'constexpr float LEDGE_GRAB_REACH = gameplay::PHONE_BODY.ledgeGrabReach;',
        'constexpr float LEDGE_PHONE_FACE_GAP = gameplay::PHONE_BODY.ledgeFaceGap;',
        'constexpr float LEDGE_CORNER_INSET = gameplay::PHONE_BODY.ledgeCornerInset;',
        'constexpr float CAMERA_COLLISION_RADIUS = gameplay::PHONE_BODY.cameraCollisionRadius;',
        'constexpr float CAMERA_COLLISION_BACKOFF = gameplay::PHONE_BODY.cameraCollisionBackoff;',
        'constexpr float AIR_MELEE_PHONE_RADIUS = gameplay::PHONE_BODY.airMeleeRadius;',
        'constexpr float AIR_MELEE_BODY_FORGIVENESS = gameplay::PHONE_BODY.airMeleeBodyForgiveness;',
    )
    legacy = (
        'constexpr float PLAYER_CEILING_BODY_CLEARANCE = 0.42f;',
        'constexpr float PLAYER_COLLISION_RADIUS = 0.34f;',
        'constexpr float PLAYER_SUPPORT_RADIUS = 0.06f;',
        'constexpr float CAMERA_COLLISION_RADIUS = 0.42f;',
        'constexpr float AIR_MELEE_PHONE_RADIUS = 0.10f;',
    )
    return all(marker in source for marker in required) and not any(marker in source for marker in legacy)


def transform(source: str) -> str:
    if is_fully_adopted(source):
        return source

    text = replace_once(
        source,
        '#include "Game.hpp"\n',
        '#include "Game.hpp"\n#include "gameplay/PhoneBody.hpp"\n',
        "PhoneBody include",
    )

    old_geometry = '''constexpr float PLAYER_CEILING_BODY_CLEARANCE = 0.42f;
constexpr float PLAYER_COLLISION_RADIUS = 0.34f;
constexpr float PLAYER_WALL_MARGIN = PLAYER_COLLISION_RADIUS + 0.06f;
// Side collision remains generous and game-feeling; floor support follows the
// phone's visible footprint so ledges do not grow an invisible shelf.
constexpr float PLAYER_SUPPORT_RADIUS = 0.06f;
constexpr float LEDGE_GRAB_VERTICAL_BELOW = 0.24f;
constexpr float LEDGE_GRAB_VERTICAL_ABOVE = 0.13f;
constexpr float LEDGE_GRAB_REACH = 0.48f;
constexpr float LEDGE_PHONE_FACE_GAP = 0.025f;
constexpr float LEDGE_CORNER_INSET = 0.10f;
'''
    new_geometry = '''constexpr float PLAYER_CEILING_BODY_CLEARANCE = gameplay::PHONE_BODY.ceilingClearance;
constexpr float PLAYER_COLLISION_RADIUS = gameplay::PHONE_BODY.collisionRadius;
constexpr float PLAYER_WALL_MARGIN = gameplay::PHONE_BODY.wallMargin;
// Side collision remains generous and game-feeling; floor support follows the
// phone's visible footprint so ledges do not grow an invisible shelf.
constexpr float PLAYER_SUPPORT_RADIUS = gameplay::PHONE_BODY.supportRadius;
constexpr float LEDGE_GRAB_VERTICAL_BELOW = gameplay::PHONE_BODY.ledgeGrabVerticalBelow;
constexpr float LEDGE_GRAB_VERTICAL_ABOVE = gameplay::PHONE_BODY.ledgeGrabVerticalAbove;
constexpr float LEDGE_GRAB_REACH = gameplay::PHONE_BODY.ledgeGrabReach;
constexpr float LEDGE_PHONE_FACE_GAP = gameplay::PHONE_BODY.ledgeFaceGap;
constexpr float LEDGE_CORNER_INSET = gameplay::PHONE_BODY.ledgeCornerInset;
'''
    text = replace_once(text, old_geometry, new_geometry, "player and ledge geometry")

    text = replace_once(
        text,
        'constexpr float CAMERA_COLLISION_RADIUS = 0.42f;\nconstexpr float CAMERA_COLLISION_BACKOFF = 0.16f;',
        'constexpr float CAMERA_COLLISION_RADIUS = gameplay::PHONE_BODY.cameraCollisionRadius;\nconstexpr float CAMERA_COLLISION_BACKOFF = gameplay::PHONE_BODY.cameraCollisionBackoff;',
        "camera geometry",
    )
    text = replace_once(
        text,
        'constexpr float AIR_MELEE_PHONE_RADIUS = 0.10f;\nconstexpr float AIR_MELEE_BODY_FORGIVENESS = 0.07f;',
        'constexpr float AIR_MELEE_PHONE_RADIUS = gameplay::PHONE_BODY.airMeleeRadius;\nconstexpr float AIR_MELEE_BODY_FORGIVENESS = gameplay::PHONE_BODY.airMeleeBodyForgiveness;',
        "air melee geometry",
    )
    return text


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    if args.check == args.write:
        parser.error("choose exactly one of --check or --write")

    source = GAME_CPP.read_text(encoding="utf-8-sig")
    result = transform(source)
    if result == source:
        print("phone body adoption already present")
        return 0
    if args.write:
        GAME_CPP.write_text(result, encoding="utf-8", newline="\n")
        print(f"updated {GAME_CPP}")
    else:
        print("all phone body adoption anchors matched exactly once")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
