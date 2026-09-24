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
    float supportContact = 0.0f;
    float disruption = 0.0f;
    float impactInstability = 0.0f;
    float scrambleAmount = 0.0f;
    Vec3 leftFootPlant{};
    Vec3 rightFootPlant{};
    float leftPlantWeight = 0.0f;
    float rightPlantWeight = 0.0f;
    bool leftFootPlanted = false;
    bool rightFootPlanted = false;
};

struct PhysicalEnemyBodyInput {
    Vec3 desiredVelocity{};
    Vec3 actualVelocity{};
    Vec3 bodyPosition{};
    float centerOfMassHeight = 0.90f;
    float desiredYaw = 0.0f;
    float individuality = 0.0f;
    float brace = 0.0f;
    float surfaceTraction = 1.0f;
    float dt = 0.0f;
    bool grounded = true;
    float leftFootContact = 1.0f;
    float rightFootContact = 1.0f;
    Vec3 leftFootPosition{};
    Vec3 rightFootPosition{};
    Vec3 supportNormal{0.0f, 1.0f, 0.0f};
};

struct PhysicalEnemyBodyOutput {
    Vec3 velocity{};
    float yaw = 0.0f;
    float locomotion = 0.0f;
};

inline float physicalAngleDelta(float from, float to) {
    return std::atan2(std::sin(to - from), std::cos(to - from));
}

inline float finitePhysicalValue(float value, float fallback = 0.0f) {
    return std::isfinite(value) ? value : fallback;
}

inline Vec3 finitePhysicalVector(const Vec3& value, const Vec3& fallback = {}) {
    return {
        finitePhysicalValue(value.x, fallback.x),
        finitePhysicalValue(value.y, fallback.y),
        finitePhysicalValue(value.z, fallback.z)
    };
}

inline Vec3 boundedPhysicalMotion(const Vec3& value) {
    const Vec3 finite = finitePhysicalVector(value);
    return {
        std::max(-100.0f, std::min(100.0f, finite.x)),
        std::max(-100.0f, std::min(100.0f, finite.y)),
        std::max(-100.0f, std::min(100.0f, finite.z))
    };
}

inline float physicalWrappedAngle(float angle) {
    const float finite = finitePhysicalValue(angle);
    return std::atan2(std::sin(finite), std::cos(finite));
}

struct PhysicalSupportBalance {
    Vec3 center{};
    Vec3 projectedCenterOfMass{};
    Vec3 error{};
    float distance = 0.0f;
    float totalWeight = 0.0f;
    float radius = 0.0f;
    bool doubleSupport = false;
};

inline Vec3 projectedEnemyCenterOfMass(
    const Vec3& bodyPosition,
    float yaw,
    float pitch,
    float roll,
    float centerOfMassHeight)
{
    const Vec3 facing{-std::sin(yaw), 0.0f, -std::cos(yaw)};
    const Vec3 right{facing.z, 0.0f, -facing.x};
    const float height = std::max(0.10f, finitePhysicalValue(centerOfMassHeight, 0.90f));

    // The horizontal projection is what determines whether gravity can be
    // reacted through the planted feet. Lean therefore moves the COM instead
    // of merely rotating a visual shell around an unchanged support point.
    return bodyPosition
        + facing * (std::sin(pitch) * height)
        - right * (std::sin(roll) * height);
}

inline PhysicalSupportBalance physicalSupportBalance(
    const PhysicalEnemyBodyState& body,
    const Vec3& projectedCenterOfMass)
{
    constexpr float plantedWeightThreshold = 0.32f;
    constexpr float singleSupportRadius = 0.25f;
    constexpr float doubleSupportRadius = 0.46f;

    const float leftWeight = body.leftFootPlanted ? body.leftPlantWeight : 0.0f;
    const float rightWeight = body.rightFootPlanted ? body.rightPlantWeight : 0.0f;

    PhysicalSupportBalance support;
    support.totalWeight = leftWeight + rightWeight;
    support.doubleSupport = leftWeight > plantedWeightThreshold && rightWeight > plantedWeightThreshold;
    support.radius = support.doubleSupport ? doubleSupportRadius : singleSupportRadius;
    support.projectedCenterOfMass = projectedCenterOfMass;
    support.center = projectedCenterOfMass;
    if (support.totalWeight > 0.001f) {
        support.center = (body.leftFootPlant * leftWeight + body.rightFootPlant * rightWeight)
            * (1.0f / support.totalWeight);
    }
    support.error = {
        projectedCenterOfMass.x - support.center.x,
        0.0f,
        projectedCenterOfMass.z - support.center.z
    };
    support.distance = horizontalLength(support.error);
    return support;
}

inline PhysicalEnemyBodyOutput updatePhysicalEnemyBody(
    PhysicalEnemyBodyState& body,
    const PhysicalEnemyBodyInput& input,
    float currentYaw)
{
    const float dt = std::max(0.0f, std::min(finitePhysicalValue(input.dt), 1.0f / 20.0f));
    if (!body.initialized) {
        body = PhysicalEnemyBodyState{};
        body.initialized = true;
    }

    body.bodyPitch = std::max(-1.45f, std::min(1.45f, finitePhysicalValue(body.bodyPitch)));
    body.bodyRoll = std::max(-1.45f, std::min(1.45f, finitePhysicalValue(body.bodyRoll)));
    body.pitchVelocity = std::max(-4.5f, std::min(4.5f, finitePhysicalValue(body.pitchVelocity)));
    body.rollVelocity = std::max(-4.5f, std::min(4.5f, finitePhysicalValue(body.rollVelocity)));
    body.yawVelocity = std::max(-2.6f, std::min(2.6f, finitePhysicalValue(body.yawVelocity)));
    body.gaitPhase = std::fmod(finitePhysicalValue(body.gaitPhase), 2.0f * 3.14159265358979323846f);
    if (body.gaitPhase < 0.0f) body.gaitPhase += 2.0f * 3.14159265358979323846f;
    body.recovery = std::max(0.0f, finitePhysicalValue(body.recovery));
    body.supportContact = std::max(0.0f, std::min(1.0f, finitePhysicalValue(body.supportContact)));
    body.disruption = std::max(0.0f, std::min(1.0f, finitePhysicalValue(body.disruption)));
    body.impactInstability = std::max(0.0f, std::min(1.0f, finitePhysicalValue(body.impactInstability)));
    body.scrambleAmount = std::max(0.0f, std::min(1.0f, finitePhysicalValue(body.scrambleAmount)));
    body.leftFootPlant = finitePhysicalVector(body.leftFootPlant);
    body.rightFootPlant = finitePhysicalVector(body.rightFootPlant);
    body.leftPlantWeight = std::max(0.0f, std::min(1.0f, finitePhysicalValue(body.leftPlantWeight)));
    body.rightPlantWeight = std::max(0.0f, std::min(1.0f, finitePhysicalValue(body.rightPlantWeight)));

    const Vec3 desiredVelocity = boundedPhysicalMotion(input.desiredVelocity);
    const Vec3 actualVelocity = boundedPhysicalMotion(input.actualVelocity);
    const float requestedSpeed = horizontalLength(desiredVelocity);
    const float actualSpeed = horizontalLength(actualVelocity);
    const float personality = std::max(-1.0f, std::min(1.0f, finitePhysicalValue(input.individuality)));
    const float brace = std::max(0.0f, std::min(1.0f, finitePhysicalValue(input.brace)));
    const float surfaceTraction = std::max(0.45f, std::min(1.0f,
        finitePhysicalValue(input.surfaceTraction, 1.0f)));
    const float currentYawSafe = physicalWrappedAngle(currentYaw);
    const float desiredYaw = physicalWrappedAngle(finitePhysicalValue(input.desiredYaw, currentYawSafe));

    const float yawError = physicalAngleDelta(currentYawSafe, desiredYaw);
    float nextYaw = currentYawSafe;
    const Vec3 facing{-std::sin(currentYawSafe), 0.0f, -std::cos(currentYawSafe)};
    const Vec3 right{facing.z, 0.0f, -facing.x};
    const float forwardError = dot3(desiredVelocity - actualVelocity, facing);
    Vec3 supportNormal = normalized(finitePhysicalVector(
        input.supportNormal, {0.0f, 1.0f, 0.0f}));
    if (lengthSq(supportNormal) < 0.5f)
        supportNormal = {0.0f, 1.0f, 0.0f};


    const float leftFootContact = input.grounded ? std::max(0.0f, std::min(1.0f, finitePhysicalValue(input.leftFootContact))) : 0.0f;
    const float rightFootContact = input.grounded ? std::max(0.0f, std::min(1.0f, finitePhysicalValue(input.rightFootContact))) : 0.0f;
    const float contact = std::max(leftFootContact, rightFootContact);
    // Foot contacts are sensed by the world, but plant ownership belongs here.
    // The planner and Game integration cannot silently move a planted support.
    const auto updatePlant = [&](float sensedContact, const Vec3& sensedPosition,
                                 bool& planted, Vec3& plant, float& weight) {
        const Vec3 foot = finitePhysicalVector(sensedPosition);
        const Vec3 separation{foot.x - plant.x, 0.0f, foot.z - plant.z};
        if (planted && (sensedContact < 0.07f || horizontalLength(separation) > 0.43f))
            planted = false;
        if (!planted && sensedContact > 0.64f) {
            planted = true;
            plant = foot;
        }
        const float targetWeight = planted ? 1.0f : 0.0f;
        weight += (targetWeight - weight) * std::min(
            1.0f, dt * (planted ? 8.0f : 6.0f));
    };
    updatePlant(leftFootContact, input.leftFootPosition,
                body.leftFootPlanted, body.leftFootPlant, body.leftPlantWeight);
    updatePlant(rightFootContact, input.rightFootPosition,
                body.rightFootPlanted, body.rightFootPlant, body.rightPlantWeight);
    body.supportContact = contact;
    const float velocityError = horizontalLength(desiredVelocity - actualVelocity);
    const float supportSlip = std::min(1.0f, velocityError / 3.0f) * (1.0f - contact);
    // Surface traction is already the environment's authority over available
    // ground reaction. Let that same fact become physically readable: when an
    // animal asks its planted feet for a large velocity change on a slick
    // surface, some of that request appears as scrambling rather than hidden
    // perfect grip. No second weather or locomotion model is introduced.
    const float tractionSlip = std::min(1.0f, velocityError / 2.5f)
        * (1.0f - surfaceTraction) * 1.6f;
    const float slip = std::min(1.0f, std::max(supportSlip, tractionSlip));
    body.scrambleAmount = slip;
    // Cadence is intentionally slow. Speed comes from carrying the body
    // through a longer stance/stride, not rapidly tapping the toes. Scrambling
    // raises cadence only modestly so a disturbed creature still has weight.
    const float pursuitGait = std::max(0.0f, std::min(1.0f, (actualSpeed - 1.65f) / 2.2f));
    const float cadence = std::min(6.2f, 1.55f + std::min(actualSpeed, 5.0f) * 0.62f
                                         + pursuitGait * 0.65f + slip * 0.55f + personality * 0.10f);
    if (!body.fallen && input.grounded && (actualSpeed > 0.06f || requestedSpeed > 0.20f))
        body.gaitPhase += cadence * dt;
    if (body.gaitPhase >= 2.0f * 3.14159265358979323846f)
        body.gaitPhase = std::fmod(body.gaitPhase, 2.0f * 3.14159265358979323846f);

    Vec3 velocity = actualVelocity;
    const float centerOfMassHeight = std::max(
        0.10f, finitePhysicalValue(input.centerOfMassHeight, 0.90f));
    const Vec3 projectedCom = projectedEnemyCenterOfMass(
        finitePhysicalVector(input.bodyPosition),
        currentYawSafe,
        body.bodyPitch,
        body.bodyRoll,
        centerOfMassHeight);

    constexpr float minimumSupportWeight = 0.10f;
    constexpr float fallMargin = 0.34f;
    const PhysicalSupportBalance support = physicalSupportBalance(body, projectedCom);
    const float outsideSupport = std::max(0.0f, support.distance - support.radius);

    // One locomotion authority: horizontal changes come from the ground
    // reaction available at actual planted support. Planner velocity is an
    // intention, not a velocity assignment.
    if (!body.fallen && contact > 0.01f && support.totalWeight > minimumSupportWeight) {
        const float supportLoad = std::max(0.18f, std::min(1.0f, contact));
        const float accelerationLimit = (5.5f + supportLoad * (4.0f + brace * 1.2f)) * surfaceTraction;
        Vec3 groundReaction = desiredVelocity - velocity;
        groundReaction.y = 0.0f;

        const float requestedAcceleration = horizontalLength(groundReaction);
        if (requestedAcceleration > accelerationLimit && requestedAcceleration > 0.001f)
            groundReaction = groundReaction * (accelerationLimit / requestedAcceleration);

        // If gravity's projection has escaped the support region, the same
        // planted-foot reaction must first catch the mass. There is no second
        // hidden stabilizer.
        if (outsideSupport > 0.0f && support.distance > 0.001f) {
            const Vec3 towardSupport = support.error * (-1.0f / support.distance);
            const float catchAcceleration = std::min(
                accelerationLimit, outsideSupport * 18.0f);
            groundReaction += towardSupport * catchAcceleration;
            const float combined = horizontalLength(groundReaction);
            if (combined > accelerationLimit && combined > 0.001f)
                groundReaction = groundReaction * (accelerationLimit / combined);
        }

        velocity += groundReaction * dt;
    } else {
        const float drag = std::exp(-(body.fallen ? 4.2f : 0.35f) * dt);
        velocity.x *= drag;
        velocity.z *= drag;
    }

    if (!body.fallen && support.totalWeight > minimumSupportWeight
        && support.distance > support.radius + fallMargin) {
        body.fallen = true;
        body.recovery = 0.0f;
        body.rollVelocity += std::max(
            -2.4f, std::min(2.4f, dot3(support.error, right) * 5.0f));
        body.pitchVelocity += std::max(
            -2.4f, std::min(2.4f, -dot3(support.error, facing) * 5.0f));
    }

    // Turning is also a ground reaction. No planted support means no newly
    // generated yaw torque. Single support turns more slowly than a settled
    // two-foot stance.
    const float turnAuthority = body.fallen || support.totalWeight <= minimumSupportWeight
        ? 0.0f
        : (support.doubleSupport ? 1.0f : std::max(0.20f, contact * 0.48f));
    const float yawTorque = yawError * (2.65f + brace * 0.75f) * turnAuthority
        - body.yawVelocity * (2.70f + brace * 0.70f);
    body.yawVelocity += yawTorque * dt;
    body.yawVelocity = std::max(-2.6f, std::min(2.6f, body.yawVelocity));
    nextYaw = currentYawSafe + body.yawVelocity * dt;

    // Upright posture follows the support plane. Lean is no longer used as a
    // substitute for a missing center of mass.
    const float supportPitch=std::atan2(dot3(supportNormal,facing),std::max(0.1f,supportNormal.y))*0.14f;
    const float supportRoll=-std::atan2(dot3(supportNormal,right),std::max(0.1f,supportNormal.y))*0.14f;
    // Let the same support error that owns the catch reaction also own visible
    // weight transfer. A body carried by one foot leans its COM toward that
    // support instead of remaining visually centered while a hidden horizontal
    // correction does all the work. This is bounded posture, not another
    // locomotion authority.
    const float supportForwardError=dot3(support.error,facing);
    const float supportSideError=dot3(support.error,right);
    const float catchPitch=-supportForwardError*0.34f;
    const float catchRoll=supportSideError*0.42f;
    // Falling and getting back up share this one posture spring. Previously the
    // fall spring kept demanding the prone pose while a second recovery torque
    // demanded upright, leaving some bodies at a permanent tilted equilibrium.
    // Once a settled body has spent enough time down, the same spring simply
    // changes its target back to upright.
    const bool recovering = body.fallen && body.recovery > 1.25f && actualSpeed < 0.45f;
    const float desiredPitch=body.fallen&&!recovering?(body.bodyPitch>=0.0f?1.28f:-1.28f)
        :std::max(-0.16f,std::min(0.16f,-forwardError*0.025f+supportPitch+catchPitch));
    const float desiredRoll=body.fallen&&!recovering?(body.bodyRoll>=0.0f?0.82f:-0.82f)
        :std::max(-0.14f,std::min(0.14f,supportRoll+catchRoll));
    const float balanceFrequency=body.fallen?2.4f:4.4f+brace*0.8f;
    body.pitchVelocity+=((desiredPitch-body.bodyPitch)*balanceFrequency*balanceFrequency-body.pitchVelocity*(4.2f+brace))*dt;
    body.rollVelocity+=((desiredRoll-body.bodyRoll)*balanceFrequency*balanceFrequency-body.rollVelocity*(4.2f+brace))*dt;
    body.pitchVelocity=std::max(-4.5f,std::min(4.5f,body.pitchVelocity));
    body.rollVelocity=std::max(-4.5f,std::min(4.5f,body.rollVelocity));
    body.bodyPitch+=body.pitchVelocity*dt;
    body.bodyRoll+=body.rollVelocity*dt;
    body.bodyPitch=std::max(-1.45f,std::min(1.45f,body.bodyPitch));
    body.bodyRoll=std::max(-1.45f,std::min(1.45f,body.bodyRoll));
    body.impactInstability *= std::exp(-1.8f * dt);

    if (!body.fallen && (std::abs(body.bodyPitch) > 0.82f || std::abs(body.bodyRoll) > 0.76f)) {
        body.fallen = true;
        body.recovery = 0.0f;
    }
    if (body.fallen) {
        body.recovery += dt;
        if (recovering && std::abs(body.bodyPitch) < 0.20f && std::abs(body.bodyRoll) < 0.20f) {
            body.fallen = false;
            body.recovery = 0.0f;
        }
    }

    body.pitchVelocity = std::max(-4.5f, std::min(4.5f, finitePhysicalValue(body.pitchVelocity)));
    body.rollVelocity = std::max(-4.5f, std::min(4.5f, finitePhysicalValue(body.rollVelocity)));
    const float lostSupport = !input.grounded ? 1.0f : std::max(0.0f, std::min(1.0f, (0.08f - contact) / 0.08f));
    const float excessPitch = std::max(0.0f, std::min(1.0f, (std::abs(body.bodyPitch) - 0.50f) / 0.32f));
    const float excessRoll = std::max(0.0f, std::min(1.0f, (std::abs(body.bodyRoll) - 0.44f) / 0.32f));
    const float abnormalAngularMotion = std::max(
        std::max(0.0f, (std::abs(body.pitchVelocity) - 2.2f) / 2.3f),
        std::max(0.0f, (std::abs(body.rollVelocity) - 2.2f) / 2.3f));
    const float tractionDisruption = body.scrambleAmount * 0.55f;
    body.disruption = std::max(body.fallen ? 1.0f : 0.0f,
        std::max(lostSupport, std::max(body.impactInstability,
            std::max(tractionDisruption, std::max(excessPitch,
                std::max(excessRoll, std::min(1.0f, abnormalAngularMotion)))))));

    const float integratedSpeed = horizontalLength(velocity);
    return {velocity, physicalWrappedAngle(nextYaw), body.fallen ? 0.0f : std::min(1.0f, integratedSpeed / 0.7f)};
}

inline void applyPhysicalEnemyImpact(PhysicalEnemyBodyState& body, const Vec3& localImpulse) {
    if (!body.initialized) body.initialized = true;
    const Vec3 impulse = boundedPhysicalMotion(localImpulse);
    body.pitchVelocity = std::max(-4.5f, std::min(4.5f,
        finitePhysicalValue(body.pitchVelocity) + impulse.z * 0.34f));
    body.rollVelocity = std::max(-4.5f, std::min(4.5f,
        finitePhysicalValue(body.rollVelocity) - impulse.x * 0.34f));
    body.impactInstability = std::max(
        std::max(0.0f, std::min(1.0f, finitePhysicalValue(body.impactInstability))),
        std::min(1.0f, horizontalLength(impulse) / 4.0f));
}

} // namespace gameplay
