#include "BoundedSolid.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
bool near(float a, float b, float tolerance = 0.0001f) { return std::abs(a - b) <= tolerance; }
}

int main() {
    const BoundedSolid solid{{2.0f, 1.0f, -3.0f}, {4.0f, 2.0f, 2.0f}, 0.6f};
    const Vec3 local{1.25f, 0.2f, -0.4f};
    const Vec3 world = boundedSolidToWorld(solid, local);
    const Vec3 roundTrip = boundedSolidToLocal(solid, world);
    assert(near(local.x, roundTrip.x) && near(local.y, roundTrip.y) && near(local.z, roundTrip.z));

    const auto center = sampleBoundedSolidSupport(solid, solid.center.x, solid.center.z);
    assert(center.inside && near(center.height, 2.0f) && near(center.normal.y, 1.0f));
    const Vec3 outsideLocal{solid.size.x * 0.5f + 0.2f, 0.0f, 0.0f};
    const Vec3 outsideWorld = boundedSolidToWorld(solid, outsideLocal);
    assert(!sampleBoundedSolidSupport(solid, outsideWorld.x, outsideWorld.z, 0.1f).inside);
    assert(sampleBoundedSolidSupport(solid, outsideWorld.x, outsideWorld.z, 0.21f).inside);

    const Vec3 sideWorld = boundedSolidToWorld(solid, {solid.size.x * 0.5f + 0.1f, 1.0f, 0.0f});
    const auto sideHit = obstructCircleWithBoundedSolid(solid, sideWorld, 0.25f);
    assert(sideHit.hit && std::isfinite(sideHit.correctedPosition.x) && std::isfinite(sideHit.correctedPosition.z));
    const Vec3 correctedSide = boundedSolidToLocal(solid, sideHit.correctedPosition);
    assert(near(correctedSide.x, solid.size.x * 0.5f + 0.25f));
    const Vec3 expectedNormal = boundedSolidDirectionToWorld(solid, {1.0f, 0.0f, 0.0f});
    assert(near(sideHit.normal.x, expectedNormal.x) && near(sideHit.normal.z, expectedNormal.z));

    const Vec3 cornerWorld = boundedSolidToWorld(solid, {2.1f, 1.0f, 1.1f});
    const auto cornerHit = obstructCircleWithBoundedSolid(solid, cornerWorld, 0.2f);
    assert(cornerHit.hit);
    const Vec3 correctedCorner = boundedSolidToLocal(solid, cornerHit.correctedPosition);
    const float cornerDistance = std::sqrt((correctedCorner.x - 2.0f) * (correctedCorner.x - 2.0f) +
                                           (correctedCorner.z - 1.0f) * (correctedCorner.z - 1.0f));
    assert(near(cornerDistance, 0.2f));

    const Vec3 clearWorld = boundedSolidToWorld(solid, {2.4f, 1.0f, 0.0f});
    assert(!obstructCircleWithBoundedSolid(solid, clearWorld, 0.25f).hit);
    assert(!obstructCircleWithBoundedSolid(solid, solid.center + Vec3{0.0f, 2.1f, 0.0f}, 0.25f).hit);
    std::puts("Bounded solid tests passed.");
    return 0;
}
