#pragma once

#include "Math.hpp"

#include <algorithm>
#include <cmath>

// A fixed-cost static solid for manufactured/generated forms that need yaw but
// do not justify arbitrary mesh collision. Coordinates use the same yaw
// convention as the environment primitive render path.
struct BoundedSolid {
    Vec3 center{};
    Vec3 size{1.0f, 1.0f, 1.0f};
    float yaw = 0.0f;
};

struct BoundedSolidSupportSample {
    bool inside = false;
    float height = 0.0f;
    Vec3 normal{0.0f, 1.0f, 0.0f};
};

struct BoundedSolidObstructionSample {
    bool hit = false;
    Vec3 correctedPosition{};
    Vec3 normal{};
};

inline Vec3 boundedSolidToLocal(const BoundedSolid& solid, const Vec3& world) {
    const float c = std::cos(solid.yaw), s = std::sin(solid.yaw);
    const Vec3 delta = world - solid.center;
    return {delta.x * c - delta.z * s, delta.y, delta.x * s + delta.z * c};
}

inline Vec3 boundedSolidToWorld(const BoundedSolid& solid, const Vec3& local) {
    const float c = std::cos(solid.yaw), s = std::sin(solid.yaw);
    return solid.center + Vec3{local.x * c + local.z * s, local.y,
                               -local.x * s + local.z * c};
}

inline Vec3 boundedSolidDirectionToWorld(const BoundedSolid& solid, const Vec3& local) {
    const float c = std::cos(solid.yaw), s = std::sin(solid.yaw);
    return {local.x * c + local.z * s, local.y, -local.x * s + local.z * c};
}

inline BoundedSolidSupportSample sampleBoundedSolidSupport(const BoundedSolid& solid,
                                                           float x, float z,
                                                           float supportRadius = 0.0f) {
    BoundedSolidSupportSample sample{};
    if (solid.size.x <= 0.0f || solid.size.y <= 0.0f || solid.size.z <= 0.0f ||
        supportRadius < 0.0f) return sample;
    const Vec3 local = boundedSolidToLocal(solid, {x, solid.center.y, z});
    if (std::abs(local.x) > solid.size.x * 0.5f + supportRadius ||
        std::abs(local.z) > solid.size.z * 0.5f + supportRadius) return sample;
    sample.inside = true;
    sample.height = solid.center.y + solid.size.y * 0.5f;
    return sample;
}

inline BoundedSolidObstructionSample obstructCircleWithBoundedSolid(
    const BoundedSolid& solid, const Vec3& position, float radius) {
    BoundedSolidObstructionSample sample{};
    sample.correctedPosition = position;
    if (solid.size.x <= 0.0f || solid.size.y <= 0.0f || solid.size.z <= 0.0f || radius <= 0.0f)
        return sample;
    const float bottom = solid.center.y - solid.size.y * 0.5f;
    const float top = solid.center.y + solid.size.y * 0.5f;
    if (position.y < bottom || position.y > top) return sample;

    Vec3 local = boundedSolidToLocal(solid, position);
    const float halfX = solid.size.x * 0.5f, halfZ = solid.size.z * 0.5f;
    const float closestX = clampf(local.x, -halfX, halfX);
    const float closestZ = clampf(local.z, -halfZ, halfZ);
    const float deltaX = local.x - closestX, deltaZ = local.z - closestZ;
    const float distanceSq = deltaX * deltaX + deltaZ * deltaZ;
    Vec3 localNormal{};
    if (distanceSq > 0.0000001f) {
        if (distanceSq >= radius * radius) return sample;
        const float distance = std::sqrt(distanceSq);
        localNormal = {deltaX / distance, 0.0f, deltaZ / distance};
        local.x = closestX + localNormal.x * radius;
        local.z = closestZ + localNormal.z * radius;
    } else {
        const float pushes[4] = {
            local.x + halfX + radius, halfX + radius - local.x,
            local.z + halfZ + radius, halfZ + radius - local.z
        };
        int face = 0;
        for (int i = 1; i < 4; ++i) if (pushes[i] < pushes[face]) face = i;
        if (face == 0) { local.x = -halfX - radius; localNormal = {-1.0f, 0.0f, 0.0f}; }
        else if (face == 1) { local.x = halfX + radius; localNormal = {1.0f, 0.0f, 0.0f}; }
        else if (face == 2) { local.z = -halfZ - radius; localNormal = {0.0f, 0.0f, -1.0f}; }
        else { local.z = halfZ + radius; localNormal = {0.0f, 0.0f, 1.0f}; }
    }
    sample.hit = true;
    sample.correctedPosition = boundedSolidToWorld(solid, local);
    sample.normal = normalized(boundedSolidDirectionToWorld(solid, localNormal));
    return sample;
}
