#!/usr/bin/env python3
"""Apply the first guarded TargetRoles/SoulMotion adoption to Game.cpp.

This script exists because the GitHub contents API replaces whole files.  It
performs narrow, count-checked substitutions against the inspected source and
refuses to write when any anchor is stale or ambiguous.
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


def transform(source: str) -> str:
    text = source

    text = replace_once(
        text,
        '#include "Game.hpp"\n',
        '#include "Game.hpp"\n#include "gameplay/SoulMotion.hpp"\n#include "gameplay/TargetRoles.hpp"\n',
        "gameplay includes",
    )

    text = replace_once(
        text,
        'for (int i=0;i<TARGET_COUNT;++i) { TargetState& t=state_.targets[i]; if (!t.alive || (visual.hitMask&(1u<<i))!=0) continue;',
        'for (int i=0;i<TARGET_COUNT;++i) { TargetState& t=state_.targets[i]; if (!gameplay::isCombatTarget(t) || (visual.hitMask&(1u<<i))!=0) continue;',
        "melee combat eligibility",
    )

    text = replace_once(
        text,
        '        if(!target.alive||target.captureQueued||target.captureCommitted)continue;\n        if(target.slurpable)continue;',
        '        if(!gameplay::isCombatTarget(target))continue;',
        "aim-assist combat eligibility",
    )

    text = replace_once(
        text,
        '    if(!t.alive || t.captureQueued || t.captureCommitted) return false;',
        '    if(!gameplay::isCombatTarget(t)) return false;',
        "damage-shell defensive eligibility",
    )

    text = replace_once(
        text,
        '    for(const auto& target:state_.targets) if(target.alive && !target.slurpable && target.soulState==SoulState::Free) ++active;',
        '    for(const auto& target:state_.targets) if(gameplay::isActiveHuman(target)) ++active;',
        "room active-human count",
    )

    text = replace_once(
        text,
        '         for(int i=0;i<TARGET_COUNT;++i){const TargetState& target=state_.targets[i]; if(!target.alive && !target.captureQueued && !target.captureCommitted && target.soulState==SoulState::Free){slot=i;break;}}',
        '         for(int i=0;i<TARGET_COUNT;++i){const TargetState& target=state_.targets[i]; if(!target.alive && !target.captureQueued && !target.captureCommitted && target.soulState==SoulState::Free){slot=i;break;}}',
        "respawn slot guard",
    )

    text = replace_once(
        text,
        '        if (!t.alive) continue;\n        t.hitFlash = std::max(0.0f, t.hitFlash - TARGET_HITFLASH_DECAY_PER_FRAME);',
        '        if (!t.alive) continue;\n        gameplay::updateLooseSoulMotion(t, dt);\n        t.hitFlash = std::max(0.0f, t.hitFlash - TARGET_HITFLASH_DECAY_PER_FRAME);',
        "target simulation soul motion",
    )

    text = replace_once(
        text,
        '            if (!t.alive || !t.slurpable || t.captureQueued || t.captureCommitted ||\n                (t.soulState != SoulState::Free && t.soulState != SoulState::Attracted)) continue;',
        '            if (!gameplay::isFreeVacuumOffer(t)) continue;',
        "vacuum offer eligibility",
    )

    text = replace_once(
        text,
        '        if (!t.alive || !t.slurpable) continue;',
        '        if (!gameplay::isLooseSoul(t)) continue;',
        "vacuum loose-soul eligibility",
    )

    recoil_block = '''        if (t.soulState == SoulState::Recoiling) {\n            t.recoilTime -= dt;\n            t.vel.y -= 5.5f * dt;\n            t.pos += t.vel * dt;\n            const float damping = std::max(0.0f, 1.0f - 3.5f * dt);\n            t.vel.x *= damping; t.vel.z *= damping;\n            if (t.pos.y < GROUND_Y) { t.pos.y = GROUND_Y; t.vel.y = 0.0f; }\n            if (t.recoilTime <= 0.0f) { t.soulState = SoulState::Free; t.networkOwnerPlayerId=-1; }\n            continue;\n        }\n'''
    text = replace_once(text, recoil_block, '', "remove duplicate recoil simulation")

    text = replace_once(
        text,
        '            if (t.soulState == SoulState::Free) { t.ingestProgress = std::max(0.0f, t.ingestProgress - dt * SOUL_CAPTURE_DECAY); t.latchedToScreen = false; t.vel = {}; }',
        '            if (t.soulState == SoulState::Free) { t.ingestProgress = std::max(0.0f, t.ingestProgress - dt * SOUL_CAPTURE_DECAY); t.latchedToScreen = false; }',
        "preserve free-soul velocity",
    )

    return text


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="validate anchors without writing")
    parser.add_argument("--write", action="store_true", help="replace Game.cpp in place")
    args = parser.parse_args()

    if args.check == args.write:
        parser.error("choose exactly one of --check or --write")

    source = GAME_CPP.read_text(encoding="utf-8-sig")
    result = transform(source)

    if args.write:
        GAME_CPP.write_text(result, encoding="utf-8", newline="\n")
        print(f"updated {GAME_CPP}")
    else:
        print("all gameplay adoption anchors matched exactly once")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
