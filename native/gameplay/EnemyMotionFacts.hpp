#pragma once

#include "../Math.hpp"

// Facts produced by authoritative enemy locomotion and consumed by presentation.
// This type is intentionally geometry-agnostic: it describes what happened to
// the actor, never what named object caused it.
struct EnemyMotionFacts {
    Vec3 supportNormal{0.0f, 1.0f, 0.0f};
    Vec3 desiredVelocity{};
    Vec3 actualVelocity{};
    Vec3 contactNormal{};
    Vec3 climbNormal{};
    float supportAmount = 1.0f;
    float slipAmount = 0.0f;
    float obstructionAmount = 0.0f;
    float impactAmount = 0.0f;
    float balanceError = 0.0f;
    float climbAmount = 0.0f;
    bool airborne = false;
};
