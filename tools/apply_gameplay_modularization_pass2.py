#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys

GAME = Path("native-android/app/src/main/cpp/game/Game.cpp")


def once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, found {count}")
    return text.replace(old, new, 1)


def transform(source: str) -> str:
    text = source
    text = once(
        text,
        '#include "gameplay/SoulMotion.hpp"\n#include "gameplay/TargetRoles.hpp"\n',
        '#include "gameplay/MeleeConfig.hpp"\n#include "gameplay/SoulMotion.hpp"\n#include "gameplay/TargetRoles.hpp"\n#include "gameplay/VacuumGeometry.hpp"\n#include "world/RoomCoordinates.hpp"\n',
        "module includes",
    )

    melee_block = '''struct MeleeCombo { int variant; float range, damage, hitRadius, visual, dash, dashSpeed, cooldown, recoilDistance, recoilSpeed, lunge, cost; };
constexpr MeleeCombo MELEE_COMBOS[] = {
    {0,2.35f,0.82f,0.78f,0.20f,0.13f,12.5f,0.22f,0.08f,1.25f,0.15f,2.8f},
    {1,2.85f,1.08f,0.90f,0.25f,0.18f,14.0f,0.27f,0.12f,1.75f,0.22f,3.6f},
    {2,3.18f,1.48f,1.02f,0.31f,0.23f,15.2f,0.38f,0.15f,2.10f,0.29f,5.0f},
    {3,3.00f,1.22f,0.96f,0.29f,0.20f,13.8f,0.34f,0.12f,1.80f,0.25f,4.2f}
};
constexpr float MELEE_VARIANT_SIDE[] = {1,-1,1,-1};
constexpr float MELEE_VARIANT_ROLL[] = {-0.72f,0.72f,-0.42f,0.42f};
constexpr float MELEE_VARIANT_YAW[] = {0.62f,-0.62f,0.42f,-0.42f};
constexpr float MELEE_VARIANT_PITCH[] = {-0.32f,-0.32f,0.42f,0.42f};
constexpr float MELEE_VARIANT_LIFT[] = {0.012f,0.012f,-0.006f,-0.006f};
'''
    text = once(text, melee_block, "", "remove melee config block")

    text = text.replace("const MeleeCombo& combo = MELEE_COMBOS[comboIndex];", "const gameplay::MeleeCombo& combo = gameplay::MELEE_COMBOS[comboIndex];")
    for name in (
        "MELEE_VARIANT_SIDE",
        "MELEE_VARIANT_ROLL",
        "MELEE_VARIANT_YAW",
        "MELEE_VARIANT_PITCH",
        "MELEE_VARIANT_LIFT",
    ):
        text = text.replace(name, f"gameplay::{name}")

    marker = "    const Vec3 pullPoint = state_.phoneTransform.vacuumPullPoint;\n"
    addition = marker + "    const world::RoomCoordinates roomCoordinates{ROOM_DEPTH};\n    const gameplay::VacuumGeometryConfig vacuumGeometry{SOUL_ATTRACTION_RANGE, SOUL_ATTRACTION_CONE_RADIUS, SOUL_CAPTURE_CYLINDER_RADIUS, SOUL_CAPTURE_CYLINDER_HEIGHT};\n"
    text = once(text, marker, addition, "vacuum helper configuration")

    nearest = '''    auto nearestWorldPos = [&](const TargetState& target) {
        Vec3 p = target.pos;
        const int centerTile = getRoomTileIndex(pullPoint.z);
        float bestZ = wrapZ(target.pos.z) + getRoomTileOriginZ(centerTile);
        float bestDist = std::abs(bestZ - pullPoint.z);
        for (int offset : {-1, 1}) {
            const float candidate = wrapZ(target.pos.z) + getRoomTileOriginZ(centerTile + offset);
            const float candidateDist = std::abs(candidate - pullPoint.z);
            if (candidateDist < bestDist) { bestZ = candidate; bestDist = candidateDist; }
        }
        p.z = bestZ;
        return p;
    };
'''
    text = once(
        text,
        nearest,
        "    auto nearestWorldPos = [&](const TargetState& target) { return gameplay::nearestPeriodicPosition(target.pos, pullPoint.z, roomCoordinates); };\n",
        "nearest periodic position",
    )

    canonical = '''    auto writeCanonical = [&](TargetState& target, const Vec3& world) {
        target.pos = world;
        target.pos.z = wrapZ(world.z) + getRoomTileOriginZ(state_.topology.currentTileIndex);
    };
'''
    text = once(
        text,
        canonical,
        "    auto writeCanonical = [&](TargetState& target, const Vec3& world) { gameplay::writeCanonicalPosition(target.pos, world, state_.topology.currentTileIndex, roomCoordinates); };\n",
        "canonical position writer",
    )

    cylinder = '''    auto insideCylinder = [&](const Vec3& p) {
        const Vec3 d = p - state_.phoneTransform.position;
        return d.x*d.x + d.z*d.z <= SOUL_CAPTURE_CYLINDER_RADIUS*SOUL_CAPTURE_CYLINDER_RADIUS &&
            std::abs(d.y) <= SOUL_CAPTURE_CYLINDER_HEIGHT * 0.5f;
    };
'''
    text = once(
        text,
        cylinder,
        "    auto insideCylinder = [&](const Vec3& p) { return gameplay::insideCaptureCylinder(p, state_.phoneTransform.position, vacuumGeometry); };\n",
        "capture cylinder geometry",
    )

    offer = '''    auto inOffer = [&](const Vec3& p) {
        const Vec3 toSoul = p - state_.camera.pos;
        const float forwardDistance = dot3(toSoul, state_.camera.forward);
        if (forwardDistance <= 0.0f || forwardDistance > SOUL_ATTRACTION_RANGE) return false;
        const Vec3 radial = toSoul - state_.camera.forward * forwardDistance;
        const float coneRadius = SOUL_ATTRACTION_CONE_RADIUS * (0.24f + forwardDistance / SOUL_ATTRACTION_RANGE);
        return lengthSq(radial) <= coneRadius * coneRadius;
    };
'''
    text = once(
        text,
        offer,
        "    auto inOffer = [&](const Vec3& p) { return gameplay::insideVacuumOffer(p, state_.camera.pos, state_.camera.forward, vacuumGeometry); };\n",
        "vacuum offer geometry",
    )
    return text


def main() -> int:
    source = GAME.read_text(encoding="utf-8-sig")
    result = transform(source)
    GAME.write_text(result, encoding="utf-8", newline="\n")
    print(f"updated {GAME}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
