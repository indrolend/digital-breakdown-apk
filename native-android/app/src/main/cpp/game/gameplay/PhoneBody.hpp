#pragma once

namespace gameplay {

// Canonical model-space/runtime occupancy contract for the playable phone.
// These values intentionally preserve current gameplay behavior while giving
// collision, ledge, camera, and melee tuning one shared source of truth.
struct PhoneBodyGeometry {
    float collisionRadius = 0.34f;
    float supportRadius = 0.06f;
    float ceilingClearance = 0.42f;
    float wallMargin = 0.40f;

    float cameraCollisionRadius = 0.42f;
    float cameraCollisionBackoff = 0.16f;

    float ledgeGrabVerticalBelow = 0.24f;
    float ledgeGrabVerticalAbove = 0.13f;
    float ledgeGrabReach = 0.48f;
    float ledgeFaceGap = 0.025f;
    float ledgeCornerInset = 0.10f;

    float airMeleeRadius = 0.10f;
    float airMeleeBodyForgiveness = 0.07f;
};

inline constexpr PhoneBodyGeometry PHONE_BODY{};

constexpr bool validPhoneBodyGeometry(const PhoneBodyGeometry& body) noexcept {
    return body.collisionRadius > 0.0f &&
           body.supportRadius > 0.0f &&
           body.supportRadius <= body.collisionRadius &&
           body.wallMargin >= body.collisionRadius &&
           body.ceilingClearance >= body.collisionRadius &&
           body.cameraCollisionRadius >= body.collisionRadius &&
           body.cameraCollisionBackoff >= 0.0f &&
           body.ledgeGrabVerticalBelow >= 0.0f &&
           body.ledgeGrabVerticalAbove >= 0.0f &&
           body.ledgeGrabReach >= body.collisionRadius &&
           body.ledgeFaceGap >= 0.0f &&
           body.ledgeCornerInset >= 0.0f &&
           body.airMeleeRadius > 0.0f &&
           body.airMeleeBodyForgiveness >= 0.0f;
}

static_assert(validPhoneBodyGeometry(PHONE_BODY), "phone body geometry contract must remain internally consistent");

} // namespace gameplay
