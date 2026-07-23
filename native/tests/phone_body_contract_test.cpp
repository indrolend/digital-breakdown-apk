#include <cassert>
#include <cmath>

#include "gameplay/PhoneBody.hpp"

namespace {

bool near(float a, float b, float epsilon = 0.0001f) {
    return std::abs(a - b) <= epsilon;
}

} // namespace

int main() {
    using gameplay::PHONE_BODY;

    static_assert(gameplay::validPhoneBodyGeometry(PHONE_BODY));

    // Behavior-preservation anchors copied from the current Game.cpp runtime.
    assert(near(PHONE_BODY.collisionRadius, 0.34f));
    assert(near(PHONE_BODY.supportRadius, 0.06f));
    assert(near(PHONE_BODY.ceilingClearance, 0.42f));
    assert(near(PHONE_BODY.wallMargin, PHONE_BODY.collisionRadius + 0.06f));
    assert(near(PHONE_BODY.cameraCollisionRadius, 0.42f));
    assert(near(PHONE_BODY.cameraCollisionBackoff, 0.16f));
    assert(near(PHONE_BODY.ledgeGrabReach, 0.48f));
    assert(near(PHONE_BODY.airMeleeRadius + PHONE_BODY.airMeleeBodyForgiveness, 0.17f));

    gameplay::PhoneBodyGeometry invalid = PHONE_BODY;
    invalid.supportRadius = invalid.collisionRadius + 0.01f;
    assert(!gameplay::validPhoneBodyGeometry(invalid));

    return 0;
}
