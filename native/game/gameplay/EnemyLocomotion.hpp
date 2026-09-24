#pragma once

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

inline void resetEnemyLocomotion(EnemyLocomotionState& locomotion) {
    locomotion = EnemyLocomotionState{};
}

} // namespace gameplay
