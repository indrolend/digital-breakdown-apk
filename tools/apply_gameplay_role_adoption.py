#!/usr/bin/env python3
"""Apply the first guarded TargetRoles/SoulMotion adoption to Game.cpp.

The GitHub contents API replaces whole files. This script performs narrow,
count-checked substitutions against the inspected source, accepts a fully
adopted source as a no-op, and refuses partial or ambiguous states.
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
        '#include "gameplay/SoulMotion.hpp"',
        '#include "gameplay/TargetRoles.hpp"',
        'gameplay::isCombatTarget(t)',
        'gameplay::isCombatTarget(target)',
        'gameplay::isActiveHuman(target)',
        'gameplay::updateLooseSoulMotion(t, dt)',
        'gameplay::isFreeVacuumOffer(t)',
        'gameplay::isLooseSoul(t)',
        'if (t.soulState == SoulState::Free) { t.ingestProgress = std::max(0.0f, t.ingestProgress - dt * SOUL_CAPTURE_DECAY); t.latchedToScreen = false; }',
    )
    legacy = (
        'if (!t.alive || (visual.hitMask&(1u<<i))!=0) continue;',
        'if(!target.alive||target.captureQueued||target.captureCommitted)continue;',
        'if(!t.alive || t.captureQueued || t.captureCommitted) return false;',
        'if(target.alive && !target.slurpable && target.soulState==SoulState::Free)',
        'if (!t.alive || !t.slurpable || t.captureQueued || t.captureCommitted ||',
        'if (!t.alive || !t.slurpable) continue;',
        'if (t.soulState == SoulState::Free) { t.ingestProgress = std::max(0.0f, t.ingestProgress - dt * SOUL_CAPTURE_DECAY); t.latchedToScreen = false; t.vel = {}; }',
    )
    return all(marker in source for marker in required) and not any(marker in source for marker in legacy)


def transform(source: str) -> str:
    if is_fully_adopted(source):
        return source

    text = source
    replacements = (
        ('#include "Game.hpp"\n', '#include "Game.hpp"\n#include "gameplay/SoulMotion.hpp"\n#include "gameplay/TargetRoles.hpp"\n', "gameplay includes"),
        ('for (int i=0;i<TARGET_COUNT;++i) { TargetState& t=state_.targets[i]; if (!t.alive || (visual.hitMask&(1u<<i))!=0) continue;', 'for (int i=0;i<TARGET_COUNT;++i) { TargetState& t=state_.targets[i]; if (!gameplay::isCombatTarget(t) || (visual.hitMask&(1u<<i))!=0) continue;', "melee combat eligibility"),
        ('        if(!target.alive||target.captureQueued||target.captureCommitted)continue;\n        if(target.slurpable)continue;', '        if(!gameplay::isCombatTarget(target))continue;', "aim-assist combat eligibility"),
        ('    if(!t.alive || t.captureQueued || t.captureCommitted) return false;', '    if(!gameplay::isCombatTarget(t)) return false;', "damage-shell defensive eligibility"),
        ('    for(const auto& target:state_.targets) if(target.alive && !target.slurpable && target.soulState==SoulState::Free) ++active;', '    for(const auto& target:state_.targets) if(gameplay::isActiveHuman(target)) ++active;', "room active-human count"),
        ('        if (!t.alive) continue;\n        t.hitFlash = std::max(0.0f, t.hitFlash - TARGET_HITFLASH_DECAY_PER_FRAME);', '        if (!t.alive) continue;\n        gameplay::updateLooseSoulMotion(t, dt);\n        t.hitFlash = std::max(0.0f, t.hitFlash - TARGET_HITFLASH_DECAY_PER_FRAME);', "target simulation soul motion"),
        ('            if (!t.alive || !t.slurpable || t.captureQueued || t.captureCommitted ||\n                (t.soulState != SoulState::Free && t.soulState != SoulState::Attracted)) continue;', '            if (!gameplay::isFreeVacuumOffer(t)) continue;', "vacuum offer eligibility"),
        ('        if (!t.alive || !t.slurpable) continue;', '        if (!gameplay::isLooseSoul(t)) continue;', "vacuum loose-soul eligibility"),
        ('            if (t.soulState == SoulState::Free) { t.ingestProgress = std::max(0.0f, t.ingestProgress - dt * SOUL_CAPTURE_DECAY); t.latchedToScreen = false; t.vel = {}; }', '            if (t.soulState == SoulState::Free) { t.ingestProgress = std::max(0.0f, t.ingestProgress - dt * SOUL_CAPTURE_DECAY); t.latchedToScreen = false; }', "preserve free-soul velocity"),
    )
    for old, new, label in replacements:
        text = replace_once(text, old, new, label)

    recoil_block = '''        if (t.soulState == SoulState::Recoiling) {\n            t.recoilTime -= dt;\n            t.vel.y -= 5.5f * dt;\n            t.pos += t.vel * dt;\n            const float damping = std::max(0.0f, 1.0f - 3.5f * dt);\n            t.vel.x *= damping; t.vel.z *= damping;\n            if (t.pos.y < GROUND_Y) { t.pos.y = GROUND_Y; t.vel.y = 0.0f; }\n            if (t.recoilTime <= 0.0f) { t.soulState = SoulState::Free; t.networkOwnerPlayerId=-1; }\n            continue;\n        }\n'''
    return replace_once(text, recoil_block, '', "remove duplicate recoil simulation")


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
        print("gameplay role adoption already present")
        return 0
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
