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
    return output;
}

inline void resetEnemyLocomotion(EnemyLocomotionState& locomotion) {
    locomotion = EnemyLocomotionState{};
}

} // namespace gameplay
