#include <cassert>
#include <cmath>

#include "gameplay/MeleeConfig.hpp"
#include "gameplay/VacuumGeometry.hpp"
#include "world/RoomCoordinates.hpp"

namespace {

bool near(float a, float b, float epsilon = 0.0001f) {
    return std::abs(a - b) <= epsilon;
}

} // namespace

int main() {
    const world::RoomCoordinates rooms{42.0f};

    assert(near(rooms.wrapLocalZ(0.0f), 0.0f));
    assert(near(rooms.wrapLocalZ(21.0f), -21.0f));
    assert(near(rooms.wrapLocalZ(43.0f), 1.0f));
    assert(rooms.tileIndex(0.0f) == 0);
    assert(rooms.tileIndex(22.0f) == 1);
    assert(rooms.tileIndex(-22.0f) == -1);
    assert(near(rooms.canonicalZ(43.0f, 0), 1.0f));
    assert(near(rooms.nearestPeriodicZ(-20.0f, 23.0f), 22.0f));

    const world::RoomCoordinates disabled{0.0f};
    assert(near(disabled.wrapLocalZ(17.0f), 17.0f));
    assert(disabled.tileIndex(17.0f) == 0);
    assert(near(disabled.nearestPeriodicZ(17.0f, -100.0f), 17.0f));

    gameplay::VacuumGeometryConfig geometry;
    const Vec3 camera{0.0f, 1.0f, 0.0f};
    const Vec3 forward{0.0f, 0.0f, -1.0f};
    assert(gameplay::insideVacuumOffer({0.0f, 1.0f, -5.0f}, camera, forward, geometry));
    assert(!gameplay::insideVacuumOffer({0.0f, 1.0f, 5.0f}, camera, forward, geometry));
    assert(!gameplay::insideVacuumOffer({8.0f, 1.0f, -5.0f}, camera, forward, geometry));

    assert(gameplay::insideCaptureCylinder({1.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, geometry));
    assert(!gameplay::insideCaptureCylinder({2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, geometry));

    const Vec3 periodic = gameplay::nearestPeriodicPosition({0.0f, 0.0f, -20.0f}, 23.0f, rooms);
    assert(near(periodic.z, 22.0f));

    Vec3 canonical;
    gameplay::writeCanonicalPosition(canonical, {1.0f, 2.0f, 43.0f}, 0, rooms);
    assert(near(canonical.x, 1.0f));
    assert(near(canonical.y, 2.0f));
    assert(near(canonical.z, 1.0f));

    static_assert(gameplay::MELEE_COMBOS.size() == 4);
    assert(gameplay::MELEE_COMBOS[0].variant == 0);
    assert(near(gameplay::MELEE_COMBOS[2].damage, 1.48f));
    assert(near(gameplay::MELEE_VARIANT_SIDE[1], -1.0f));

    return 0;
}
