#pragma once

#include <algorithm>
#include <cmath>

#include "Math.hpp"

namespace gameplay {

// Prototype body authority. The planner may request velocity and facing, but
// only this force/constraint state is allowed to turn those requests into
// motion. Keeping it plain data preserves deterministic snapshots and makes a
// learned policy a later replacement for desired joint targets, not physics.
struct PhysicalEnemyBodyState {
    bool initialized = false;
    bool fallen = false;
    float bodyPitch = 0.0f;
    float bodyRoll = 0.0f;
    float pitchVelocity = 0.0f;
    float rollVelocity = 0.0f;
    float yawVelocity = 0.0f;
    float gaitPhase = 0.0f;
    float recovery = 0.0f;
};

struct PhysicalEnemyBodyInput {
    Vec3 desiredVelocity{};
    Vec3 actualVelocity{};
    float desiredYaw = 0.0f;
    float individuality = 0.0f;
    float brace = 0.0f;
    float dt = 0.0f;
    bool grounded = true;
};

struct PhysicalEnemyBodyOutput {
    Vec3 velocity{};
    float yaw = 0.0f;
    float locomotion = 0.0f;
};

inline float physicalAngleDelta(float from, float to) {
    return std::atan2(std::sin(to - from), std::cos(to - from));
}

inline PhysicalEnemyBodyOutput updatePhysicalEnemyBody(
    PhysicalEnemyBodyState& body,
    const PhysicalEnemyBodyInput& input,
    float currentYaw)
{
    const float dt = std::max(0.0f, std::min(input.dt, 1.0f / 20.0f));
    if (!body.initialized) {
        body = PhysicalEnemyBodyState{};
        body.initialized = true;
    }

    const float requestedSpeed = horizontalLength(input.desiredVelocity);
    const float actualSpeed = horizontalLength(input.actualVelocity);
    const float personality = std::max(-1.0f, std::min(1.0f, input.individuality));
    const float brace = std::max(0.0f, std::min(1.0f, input.brace));

    const float yawError = physicalAngleDelta(currentYaw, input.desiredYaw);
    const float yawTorque = yawError * (5.4f + brace * 1.8f) - body.yawVelocity * 3.6f;
    body.yawVelocity += yawTorque * dt;
    body.yawVelocity = std::max(-2.6f, std::min(2.6f, body.yawVelocity));
    float nextYaw = currentYaw + body.yawVelocity * dt;

    const float cadence = 4.4f + requestedSpeed * 1.15f + personality * 0.24f;
    if (!body.fallen && input.grounded && requestedSpeed > 0.08f)
        body.gaitPhase += cadence * dt;

    const float stride = std::sin(body.gaitPhase);
    const float leftLift = std::max(0.0f, stride);
    const float rightLift = std::max(0.0f, -stride);
    const float leftFootContact = input.grounded ? (1.0f - std::min(1.0f, leftLift * 1.8f)) : 0.0f;
    const float rightFootContact = input.grounded ? (1.0f - std::min(1.0f, rightLift * 1.8f)) : 0.0f;
    const float contact = std::max(leftFootContact, rightFootContact);

    Vec3 velocity = input.actualVelocity;
    if (!body.fallen && contact > 0.01f) {
        // Ground reaction is available only through a planted foot. The
        // alternating contact scalar makes acceleration pulse with each step.
        const float traction = 4.0f + contact * (5.8f + brace * 2.0f);
        velocity.x += (input.desiredVelocity.x - velocity.x) * std::min(1.0f, traction * dt);
        velocity.z += (input.desiredVelocity.z - velocity.z) * std::min(1.0f, traction * dt);
    } else {
        const float drag = std::exp(-(body.fallen ? 4.2f : 0.35f) * dt);
        velocity.x *= drag;
        velocity.z *= drag;
    }

    const Vec3 facing{-std::sin(nextYaw), 0.0f, -std::cos(nextYaw)};
    const Vec3 right{facing.z, 0.0f, -facing.x};
    const float forwardError = dot3(input.desiredVelocity - velocity, facing);
    const float lateralVelocity = dot3(velocity, right);
    const float desiredPitch = body.fallen ? (body.bodyPitch >= 0.0f ? 1.28f : -1.28f)
                                           : std::max(-0.34f, std::min(0.25f, -forwardError * 0.075f));
    const float desiredRoll = body.fallen ? (body.bodyRoll >= 0.0f ? 0.82f : -0.82f)
                                          : std::max(-0.30f, std::min(0.30f,
                                                lateralVelocity * 0.10f - body.yawVelocity * actualSpeed * 0.035f));
    const float balanceFrequency = body.fallen ? 2.4f : 8.0f + brace * 3.0f;
    const float wobble = std::sin(body.gaitPhase * 0.5f + personality * 2.7f) * requestedSpeed * 0.025f;
    body.pitchVelocity += ((desiredPitch - body.bodyPitch) * balanceFrequency * balanceFrequency
                           - body.pitchVelocity * (5.0f + brace * 2.0f)) * dt;
    body.rollVelocity += ((desiredRoll + wobble - body.bodyRoll) * balanceFrequency * balanceFrequency
                          - body.rollVelocity * (5.0f + brace * 2.0f)) * dt;
    body.bodyPitch += body.pitchVelocity * dt;
    body.bodyRoll += body.rollVelocity * dt;

    if (!body.fallen && (std::abs(body.bodyPitch) > 0.82f || std::abs(body.bodyRoll) > 0.76f)) {
        body.fallen = true;
        body.recovery = 0.0f;
    }
    if (body.fallen) {
        body.recovery += dt;
        if (body.recovery > 1.25f && actualSpeed < 0.45f) {
            // Get-up remains joint torque: angles converge over time and the
            // body regains locomotor authority only after it is nearly upright.
            body.pitchVelocity += (-body.bodyPitch * 17.0f - body.pitchVelocity * 4.0f) * dt;
            body.rollVelocity += (-body.bodyRoll * 17.0f - body.rollVelocity * 4.0f) * dt;
            if (std::abs(body.bodyPitch) < 0.20f && std::abs(body.bodyRoll) < 0.20f) {
                body.fallen = false;
                body.recovery = 0.0f;
            }
        }
    }

    return {velocity, nextYaw, body.fallen ? 0.0f : std::min(1.0f, requestedSpeed / 0.7f)};
}

inline void applyPhysicalEnemyImpact(PhysicalEnemyBodyState& body, const Vec3& localImpulse) {
    body.pitchVelocity += localImpulse.z * 0.34f;
    body.rollVelocity -= localImpulse.x * 0.34f;
}

} // namespace gameplay
