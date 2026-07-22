#pragma once

#include <cmath>

#include "../Math.hpp"
#include "../world/RoomCoordinates.hpp"

namespace gameplay {

struct VacuumGeometryConfig {
    float attractionRange = 15.5f;
    float attractionConeRadius = 2.35f;
    float captureCylinderRadius = 1.75f;
    float captureCylinderHeight = 2.25f;
};

inline bool insideCaptureCylinder(
    const Vec3& point,
    const Vec3& phonePosition,
    const VacuumGeometryConfig& config = {}) noexcept {
    const Vec3 delta = point - phonePosition;
    return delta.x * delta.x + delta.z * delta.z <=
               config.captureCylinderRadius * config.captureCylinderRadius &&
           std::abs(delta.y) <= config.captureCylinderHeight * 0.5f;
}

inline bool insideVacuumOffer(
    const Vec3& point,
    const Vec3& cameraPosition,
    const Vec3& cameraForward,
    const VacuumGeometryConfig& config = {}) noexcept {
    const Vec3 toSoul = point - cameraPosition;
    const float forwardDistance = dot3(toSoul, cameraForward);
    if (forwardDistance <= 0.0f || forwardDistance > config.attractionRange) return false;
    const Vec3 radial = toSoul - cameraForward * forwardDistance;
    const float coneRadius = config.attractionConeRadius *
        (0.24f + forwardDistance / config.attractionRange);
    return lengthSq(radial) <= coneRadius * coneRadius;
}

inline Vec3 nearestPeriodicPosition(
    const Vec3& canonicalPosition,
    float referenceZ,
    const world::RoomCoordinates& coordinates) noexcept {
    Vec3 result = canonicalPosition;
    result.z = coordinates.nearestPeriodicZ(canonicalPosition.z, referenceZ);
    return result;
}

inline void writeCanonicalPosition(
    Vec3& canonicalPosition,
    const Vec3& worldPosition,
    int targetTile,
    const world::RoomCoordinates& coordinates) noexcept {
    canonicalPosition = worldPosition;
    canonicalPosition.z = coordinates.canonicalZ(worldPosition.z, targetTile);
}

} // namespace gameplay
