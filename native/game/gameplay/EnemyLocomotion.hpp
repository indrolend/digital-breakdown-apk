#pragma once

#include <algorithm>
#include <cmath>

#include "Math.hpp"

namespace gameplay {

enum class EnemyFootPhase : unsigned char {
    Planted,
    Unloading,
    Swing,
    SeekingContact,
    Loading
};

enum class EnemyRecoveryPhase : unsigned char {
    None,
    GatherFeet,
    EstablishSupport,
    Rise
};

struct EnemyFootState {
    Vec3 position{};
    Vec3 plantPosition{};
    Vec3 swingStart{};
    Vec3 swingTarget{};
    Vec3 supportNormal{0.0f, 1.0f, 0.0f};
    EnemyFootPhase phase = EnemyFootPhase::Planted;
    float contact = 0.0f;
    float load = 0.0f;
    float swingProgress = 0.0f;
    bool planted = false;
};

struct EnemyLocomotionState {
    EnemyFootState left{};
    EnemyFootState right{};
    int swingFoot = -1;
    int nextSwingFoot = 0;
    float stepCooldown = 0.0f;
    float stanceTime = 0.0f;
    Vec3 committedTravelDirection{};
    float directionalCommitmentTimer = 0.0f;
    float crouch = 0.0f;
    float recoveryUrgency = 0.0f;
    EnemyRecoveryPhase recoveryPhase = EnemyRecoveryPhase::None;
    float recoveryPhaseTime = 0.0f;
    float physicalGaitPhase = 0.0f;
    bool initialized = false;
};

struct EnemyFootSupport {
    Vec3 position{};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    bool valid = false;
};

struct EnemyLocomotionInput {
    Vec3 bodyPosition{};
    Vec3 bodyVelocity{};
    float bodyYaw = 0.0f;
    float bodyPitch = 0.0f;
    float bodyRoll = 0.0f;
    Vec3 desiredTravelDirection{};
    float desiredSpeed = 0.0f;
    float desiredYaw = 0.0f;
    float brace = 0.0f;
    float traction = 1.0f;
    float individuality = 0.0f;
    float centerOfMassHeight = 0.9f;
    float dt = 0.0f;
    bool grounded = true;
    bool fallen = false;
};

struct EnemyLocomotionOutput {
    Vec3 leftFootPosition{};
    Vec3 rightFootPosition{};
    Vec3 supportNormal{0.0f, 1.0f, 0.0f};
    Vec3 supportedDesiredVelocity{};
    float leftContact = 0.0f;
    float rightContact = 0.0f;
    float leftLoad = 0.0f;
    float rightLoad = 0.0f;
    float desiredYaw = 0.0f;
    float gaitPhase = 0.0f;
    float crouch = 0.0f;
    float recoveryUrgency = 0.0f;
    bool correctiveStepActive = false;
    bool turnStepActive = false;
    bool supportRecoveryReady = false;
};

inline float enemyLocomotionAngleDelta(float from, float to) {
    return std::atan2(std::sin(to - from), std::cos(to - from));
}

template <typename SupportQuery>
inline void initializeEnemyLocomotion(
    EnemyLocomotionState& locomotion,
    const EnemyLocomotionInput& input,
    SupportQuery&& querySupport)
{
    const Vec3 forward{-std::sin(input.bodyYaw), 0.0f, -std::cos(input.bodyYaw)};
    const Vec3 right{forward.z, 0.0f, -forward.x};
    const float stanceHalfWidth = 0.12f + std::max(-0.02f, std::min(0.02f, input.individuality * 0.02f));
    const auto initializeFoot = [&](EnemyFootState& foot, float side) {
        const Vec3 candidate = input.bodyPosition + right * (side * stanceHalfWidth);
        const EnemyFootSupport support = querySupport(candidate);
        foot = EnemyFootState{};
        foot.position = support.valid ? support.position : candidate;
        foot.plantPosition = foot.position;
        foot.swingStart = foot.position;
        foot.swingTarget = foot.position;
        foot.supportNormal = support.valid ? support.normal : Vec3{0.0f, 1.0f, 0.0f};
        foot.phase = EnemyFootPhase::Planted;
        foot.contact = input.grounded && support.valid ? 1.0f : 0.0f;
        foot.load = foot.contact * 0.5f;
        foot.planted = foot.contact > 0.0f;
    };
    initializeFoot(locomotion.left, 1.0f);
    initializeFoot(locomotion.right, -1.0f);
    locomotion.swingFoot = -1;
    locomotion.nextSwingFoot = input.individuality < 0.0f ? 1 : 0;
    locomotion.stepCooldown = 0.0f;
    locomotion.stanceTime = 0.0f;
    locomotion.committedTravelDirection = forward;
    locomotion.directionalCommitmentTimer = 0.0f;
    locomotion.crouch = 0.0f;
    locomotion.recoveryUrgency = 0.0f;
    locomotion.physicalGaitPhase = 0.0f;
    locomotion.initialized = true;
}

inline EnemyLocomotionOutput enemyLocomotionOutput(const EnemyLocomotionState& locomotion) {
    EnemyLocomotionOutput output{};
    output.leftFootPosition = locomotion.left.planted
        ? locomotion.left.plantPosition : locomotion.left.position;
    output.rightFootPosition = locomotion.right.planted
        ? locomotion.right.plantPosition : locomotion.right.position;
    output.leftContact = locomotion.left.contact;
    output.rightContact = locomotion.right.contact;
    output.leftLoad = locomotion.left.load;
    output.rightLoad = locomotion.right.load;
    const float totalLoad = output.leftLoad + output.rightLoad;
    output.supportNormal = totalLoad > 0.001f
        ? normalized(locomotion.left.supportNormal * output.leftLoad
            + locomotion.right.supportNormal * output.rightLoad)
        : Vec3{0.0f, 1.0f, 0.0f};
    output.gaitPhase = locomotion.physicalGaitPhase;
    output.crouch = locomotion.crouch;
    output.recoveryUrgency = locomotion.recoveryUrgency;
    output.correctiveStepActive = locomotion.recoveryUrgency > 0.08f
        && locomotion.swingFoot >= 0;
    output.supportRecoveryReady = locomotion.recoveryPhase == EnemyRecoveryPhase::Rise
        && output.leftContact > 0.80f && output.rightContact > 0.80f;
    return output;
}

inline Vec3 enemyLocomotionHorizontalDirection(const Vec3& value, const Vec3& fallback = {}) {
    const Vec3 horizontal{value.x, 0.0f, value.z};
    const float magnitude = horizontalLength(horizontal);
    return magnitude > 0.0001f ? horizontal * (1.0f / magnitude) : fallback;
}

template <typename SupportQuery>
inline EnemyLocomotionOutput updateEnemyLocomotion(
    EnemyLocomotionState& locomotion,
    const EnemyLocomotionInput& input,
    SupportQuery&& querySupport)
{
    if (!locomotion.initialized)
        initializeEnemyLocomotion(locomotion, input, querySupport);
    // A spawn or landing can be resolved onto terrain after locomotion's first
    // sample. Acquire an initial stance once support is real; never drag an
    // already planted stance along with the body.
    if (input.grounded && !input.fallen
        && !locomotion.left.planted && !locomotion.right.planted)
        initializeEnemyLocomotion(locomotion, input, querySupport);

    const float dt = std::max(0.0f, std::min(input.dt, 1.0f / 20.0f));
    const Vec3 forward{-std::sin(input.bodyYaw), 0.0f, -std::cos(input.bodyYaw)};
    const Vec3 right{forward.z, 0.0f, -forward.x};
    const Vec3 requestedDirection = enemyLocomotionHorizontalDirection(
        input.desiredTravelDirection, locomotion.committedTravelDirection);
    const float requestedSpeed = std::max(0.0f, input.desiredSpeed);
    const float turnError = enemyLocomotionAngleDelta(input.bodyYaw, input.desiredYaw);

    if (input.fallen) {
        if (locomotion.recoveryPhase == EnemyRecoveryPhase::None) {
            locomotion.recoveryPhase = EnemyRecoveryPhase::GatherFeet;
            locomotion.recoveryPhaseTime = 0.0f;
            locomotion.swingFoot = -1;
            locomotion.left.swingStart = locomotion.left.position;
            locomotion.right.swingStart = locomotion.right.position;
            const EnemyFootSupport leftTarget = querySupport(
                input.bodyPosition + right * 0.14f + forward * 0.04f);
            const EnemyFootSupport rightTarget = querySupport(
                input.bodyPosition - right * 0.14f + forward * 0.04f);
            locomotion.left.swingTarget = leftTarget.valid
                ? leftTarget.position : input.bodyPosition + right * 0.14f;
            locomotion.right.swingTarget = rightTarget.valid
                ? rightTarget.position : input.bodyPosition - right * 0.14f;
            locomotion.left.supportNormal = leftTarget.normal;
            locomotion.right.supportNormal = rightTarget.normal;
            locomotion.left.planted = false;
            locomotion.right.planted = false;
            locomotion.left.contact = locomotion.right.contact = 0.0f;
            locomotion.left.load = locomotion.right.load = 0.0f;
        }
        locomotion.recoveryPhaseTime += dt;
        if (locomotion.recoveryPhase == EnemyRecoveryPhase::GatherFeet) {
            const float p = std::min(1.0f, locomotion.recoveryPhaseTime / 0.45f);
            locomotion.left.position = locomotion.left.swingStart * (1.0f - p)
                + locomotion.left.swingTarget * p;
            locomotion.right.position = locomotion.right.swingStart * (1.0f - p)
                + locomotion.right.swingTarget * p;
            const float lift = std::sin(p * 3.14159265358979323846f) * 0.04f;
            locomotion.left.position.y += lift;
            locomotion.right.position.y += lift;
            locomotion.crouch = 1.0f;
            if (p >= 1.0f) {
                locomotion.left.position = locomotion.left.swingTarget;
                locomotion.right.position = locomotion.right.swingTarget;
                locomotion.left.plantPosition = locomotion.left.position;
                locomotion.right.plantPosition = locomotion.right.position;
                locomotion.left.planted = true;
                locomotion.right.planted = true;
                locomotion.recoveryPhase = EnemyRecoveryPhase::EstablishSupport;
                locomotion.recoveryPhaseTime = 0.0f;
            }
        } else if (locomotion.recoveryPhase == EnemyRecoveryPhase::EstablishSupport) {
            const float p = std::min(1.0f, locomotion.recoveryPhaseTime / 0.38f);
            locomotion.left.contact = locomotion.right.contact = p;
            locomotion.left.load = locomotion.right.load = p * 0.5f;
            locomotion.crouch = 1.0f;
            if (p >= 1.0f) {
                locomotion.recoveryPhase = EnemyRecoveryPhase::Rise;
                locomotion.recoveryPhaseTime = 0.0f;
            }
        } else if (locomotion.recoveryPhase == EnemyRecoveryPhase::Rise) {
            locomotion.left.contact = locomotion.right.contact = 1.0f;
            locomotion.left.load = locomotion.right.load = 0.5f;
            locomotion.crouch = std::max(0.25f, 1.0f - locomotion.recoveryPhaseTime * 0.75f);
        }
        EnemyLocomotionOutput recoveryOutput = enemyLocomotionOutput(locomotion);
        recoveryOutput.supportRecoveryReady = locomotion.recoveryPhase == EnemyRecoveryPhase::Rise;
        recoveryOutput.crouch = locomotion.crouch;
        recoveryOutput.desiredYaw = input.bodyYaw;
        return recoveryOutput;
    }
    if (locomotion.recoveryPhase != EnemyRecoveryPhase::None) {
        locomotion.recoveryPhase = EnemyRecoveryPhase::None;
        locomotion.recoveryPhaseTime = 0.0f;
        locomotion.crouch = 0.0f;
    }

    locomotion.stepCooldown = std::max(0.0f, locomotion.stepCooldown - dt);
    locomotion.stanceTime += dt;
    locomotion.directionalCommitmentTimer = std::max(
        0.0f, locomotion.directionalCommitmentTimer - dt);
    if (horizontalLength(requestedDirection) > 0.5f
        && (locomotion.directionalCommitmentTimer <= 0.0f
            || dot3(requestedDirection, locomotion.committedTravelDirection) < -0.65f)) {
        locomotion.committedTravelDirection = requestedDirection;
        locomotion.directionalCommitmentTimer = 0.16f;
    }

    const float leftSupportLoad = locomotion.left.planted ? locomotion.left.load : 0.0f;
    const float rightSupportLoad = locomotion.right.planted ? locomotion.right.load : 0.0f;
    const float totalLoad = leftSupportLoad + rightSupportLoad;
    Vec3 supportCenter = input.bodyPosition;
    if (totalLoad > 0.001f) {
        supportCenter = (locomotion.left.plantPosition * leftSupportLoad
            + locomotion.right.plantPosition * rightSupportLoad) * (1.0f / totalLoad);
    }
    const Vec3 projectedCom = input.bodyPosition
        + forward * (std::sin(input.bodyPitch) * input.centerOfMassHeight)
        - right * (std::sin(input.bodyRoll) * input.centerOfMassHeight);
    const float predictionTime = 0.18f + std::min(0.14f, horizontalLength(input.bodyVelocity) * 0.018f);
    const Vec3 predictedCom = projectedCom
        + Vec3{input.bodyVelocity.x, 0.0f, input.bodyVelocity.z} * predictionTime;
    const Vec3 supportEscape{predictedCom.x - supportCenter.x, 0.0f, predictedCom.z - supportCenter.z};
    const float supportRadius = leftSupportLoad > 0.20f && rightSupportLoad > 0.20f ? 0.34f : 0.19f;
    const float outsideSupport = std::max(0.0f, horizontalLength(supportEscape) - supportRadius);
    const float urgencyTarget = std::max(0.0f, std::min(1.0f, outsideSupport / 0.42f));
    locomotion.recoveryUrgency += (urgencyTarget - locomotion.recoveryUrgency)
        * std::min(1.0f, dt * (urgencyTarget > locomotion.recoveryUrgency ? 13.0f : 4.0f));
    locomotion.crouch += ((locomotion.recoveryUrgency * 0.72f) - locomotion.crouch)
        * std::min(1.0f, dt * 9.0f);

    const bool rotationalStep = std::abs(turnError) > 0.28f;
    const bool travelStep = requestedSpeed > 0.08f;
    const bool recoveryStep = locomotion.recoveryUrgency > 0.08f;
    if (locomotion.swingFoot < 0 && input.grounded && locomotion.stepCooldown <= 0.0f
        && (travelStep || rotationalStep || recoveryStep)) {
        int swingIndex = locomotion.nextSwingFoot;
        if (recoveryStep && std::abs(dot3(supportEscape, right)) > 0.04f)
            swingIndex = dot3(supportEscape, right) > 0.0f ? 0 : 1;
        EnemyFootState& swing = swingIndex == 0 ? locomotion.left : locomotion.right;
        EnemyFootState& stance = swingIndex == 0 ? locomotion.right : locomotion.left;
        if (stance.planted && stance.contact > 0.20f) {
            const float side = swingIndex == 0 ? 1.0f : -1.0f;
            const float stanceWidth = 0.12f + locomotion.crouch * 0.05f;
            Vec3 stepDirection = locomotion.committedTravelDirection;
            if (recoveryStep && horizontalLength(supportEscape) > 0.001f)
                stepDirection = supportEscape * (1.0f / horizontalLength(supportEscape));
            if (!travelStep && rotationalStep) {
                stepDirection = {-std::sin(input.desiredYaw), 0.0f, -std::cos(input.desiredYaw)};
            }
            const float stride = recoveryStep
                ? 0.24f + locomotion.recoveryUrgency * 0.24f
                : std::min(0.48f, 0.24f + requestedSpeed * 0.075f);
            EnemyFootSupport targetSupport{};
            bool reachable = false;
            constexpr float reachScales[] = {1.0f, 0.70f, 0.45f};
            for (const float reachScale : reachScales) {
                const Vec3 candidate = input.bodyPosition
                    + stepDirection * (stride * reachScale)
                    + right * (side * stanceWidth);
                const EnemyFootSupport candidateSupport = querySupport(candidate);
                const Vec3 hipDelta = candidateSupport.position - input.bodyPosition;
                if (candidateSupport.valid
                    && horizontalLength(hipDelta) <= 0.62f
                    && std::abs(hipDelta.y) <= 0.38f) {
                    targetSupport = candidateSupport;
                    reachable = true;
                    break;
                }
            }
            if (reachable) {
                swing.swingStart = swing.planted ? swing.plantPosition : swing.position;
                swing.swingTarget = targetSupport.position;
                swing.supportNormal = targetSupport.normal;
                swing.swingProgress = 0.0f;
                swing.phase = EnemyFootPhase::Unloading;
                locomotion.swingFoot = swingIndex;
                locomotion.stanceTime = 0.0f;
            } else {
                locomotion.stepCooldown = 0.05f;
                locomotion.recoveryUrgency = std::min(1.0f, locomotion.recoveryUrgency + 0.08f);
            }
        }
    }

    if (locomotion.swingFoot >= 0) {
        EnemyFootState& swing = locomotion.swingFoot == 0 ? locomotion.left : locomotion.right;
        EnemyFootState& stance = locomotion.swingFoot == 0 ? locomotion.right : locomotion.left;
        if (swing.phase == EnemyFootPhase::Unloading) {
            swing.swingProgress = std::min(1.0f, swing.swingProgress + dt / 0.07f);
            swing.load = 0.5f * (1.0f - swing.swingProgress);
            swing.contact = 1.0f - swing.swingProgress;
            stance.load = std::min(1.0f, 0.5f + swing.swingProgress * 0.5f);
            if (swing.swingProgress >= 1.0f) {
                swing.planted = false;
                swing.phase = EnemyFootPhase::Swing;
                swing.swingProgress = 0.0f;
            }
        } else if (swing.phase == EnemyFootPhase::Swing) {
            const float swingDuration = std::max(0.16f, 0.27f - requestedSpeed * 0.018f
                - locomotion.recoveryUrgency * 0.09f);
            swing.swingProgress = std::min(1.0f, swing.swingProgress + dt / swingDuration);
            const float p = swing.swingProgress;
            swing.position = swing.swingStart * (1.0f - p) + swing.swingTarget * p;
            swing.position.y += std::sin(p * 3.14159265358979323846f)
                * (0.07f + locomotion.recoveryUrgency * 0.035f);
            swing.contact = 0.0f;
            swing.load = 0.0f;
            if (p >= 1.0f) {
                swing.position = swing.swingTarget;
                swing.phase = EnemyFootPhase::SeekingContact;
            }
        } else if (swing.phase == EnemyFootPhase::SeekingContact) {
            const EnemyFootSupport contactSupport = querySupport(swing.swingTarget);
            if (contactSupport.valid
                && horizontalLength(contactSupport.position - input.bodyPosition) <= 0.64f
                && std::abs(contactSupport.position.y - input.bodyPosition.y) <= 0.40f) {
                swing.position = contactSupport.position;
                swing.plantPosition = contactSupport.position;
                swing.supportNormal = contactSupport.normal;
                swing.planted = true;
                swing.contact = 1.0f;
                swing.load = 0.0f;
                swing.swingProgress = 0.0f;
                swing.phase = EnemyFootPhase::Loading;
            } else {
                swing.swingProgress += dt;
                locomotion.recoveryUrgency = std::min(1.0f, locomotion.recoveryUrgency + dt * 2.0f);
                if (swing.swingProgress > 0.18f) {
                    swing.phase = EnemyFootPhase::Planted;
                    swing.planted = false;
                    locomotion.swingFoot = -1;
                    locomotion.stepCooldown = 0.03f;
                }
            }
        } else if (swing.phase == EnemyFootPhase::Loading) {
            swing.swingProgress = std::min(1.0f, swing.swingProgress + dt / 0.10f);
            swing.load = 0.5f * swing.swingProgress;
            stance.load = 1.0f - swing.load;
            if (swing.swingProgress >= 1.0f) {
                swing.load = 0.5f;
                stance.load = 0.5f;
                swing.phase = EnemyFootPhase::Planted;
                swing.swingProgress = 0.0f;
                locomotion.physicalGaitPhase = std::fmod(
                    locomotion.physicalGaitPhase + 3.14159265358979323846f,
                    2.0f * 3.14159265358979323846f);
                locomotion.nextSwingFoot = locomotion.swingFoot == 0 ? 1 : 0;
                locomotion.swingFoot = -1;
                locomotion.stepCooldown = std::max(0.02f, 0.08f - requestedSpeed * 0.010f
                    - locomotion.recoveryUrgency * 0.08f);
            }
        }
    } else {
        if (locomotion.left.planted) {
            locomotion.left.position = locomotion.left.plantPosition;
            locomotion.left.contact = input.grounded ? 1.0f : 0.0f;
            locomotion.left.load += (0.5f - locomotion.left.load) * std::min(1.0f, dt * 8.0f);
        }
        if (locomotion.right.planted) {
            locomotion.right.position = locomotion.right.plantPosition;
            locomotion.right.contact = input.grounded ? 1.0f : 0.0f;
            locomotion.right.load += (0.5f - locomotion.right.load) * std::min(1.0f, dt * 8.0f);
        }
    }

    EnemyLocomotionOutput output = enemyLocomotionOutput(locomotion);
    const float plantedAuthority = std::max(0.0f, std::min(1.0f,
        output.leftLoad * output.leftContact + output.rightLoad * output.rightContact));
    // The body cannot outrun its own stride cycle. Higher-level requested
    // speed changes commitment and target reach, but stance geometry bounds
    // the velocity that loaded feet can physically support.
    const float supportLimitedSpeed = std::min(
        requestedSpeed, (0.94f + locomotion.recoveryUrgency * 0.18f)
            * (1.0f + std::max(-1.0f, std::min(1.0f, input.individuality)) * 0.12f));
    output.supportedDesiredVelocity = locomotion.committedTravelDirection
        * (supportLimitedSpeed * plantedAuthority
            * std::max(0.35f, std::min(1.0f, input.traction)));
    output.desiredYaw = input.bodyYaw + turnError * plantedAuthority;
    output.correctiveStepActive = locomotion.recoveryUrgency > 0.08f
        && locomotion.swingFoot >= 0;
    output.turnStepActive = rotationalStep && locomotion.swingFoot >= 0;
    return output;
}

inline void resetEnemyLocomotion(EnemyLocomotionState& locomotion) {
    locomotion = EnemyLocomotionState{};
}

} // namespace gameplay
