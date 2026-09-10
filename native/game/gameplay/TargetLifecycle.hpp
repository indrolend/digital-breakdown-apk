#pragma once

#include <algorithm>

#include "TargetRoles.hpp"

namespace gameplay {

inline void initializeActiveHuman(
    TargetState& target,
    bool brute,
    float normalArmor,
    float bruteArmor,
    float bruteScale) noexcept {
    target = TargetState{};
    target.alive = true;
    target.brute = brute;
    target.armor = brute ? bruteArmor : normalArmor;
    target.scale = brute ? bruteScale : 1.0f;
    target.health = 1.0f;
    target.soulState = SoulState::Free;
}

inline bool convertHumanToLooseSoul(TargetState& target) noexcept {
    if (!isActiveHuman(target)) return false;
    target.armor = 0.0f;
    target.slurpable = true;
    target.soulState = SoulState::Free;
    target.soulMorph = 0.0f;
    target.capture = 0.0f;
    target.ingestProgress = 0.0f;
    target.captureQueued = false;
    target.captureCommitted = false;
    target.latchedToScreen = false;
    target.networkOwnerPlayerId = -1;
    return true;
}

inline bool queueCapture(TargetState& target, float commitPhase) noexcept {
    if (!isLooseSoul(target)) return false;
    target.captureQueued = true;
    target.ingestProgress = std::max(target.ingestProgress, commitPhase);
    return true;
}

inline bool commitCapture(TargetState& target) noexcept {
    if (!target.captureQueued || target.captureCommitted || !isLooseSoul(target)) return false;
    target.captureQueued = false;
    target.captureCommitted = true;
    return true;
}

inline bool deactivateCapturedSoul(TargetState& target) noexcept {
    if (!target.captureCommitted || !target.alive || !target.slurpable) return false;
    target.alive = false;
    target.visibility = 0.0f;
    target.soulCubeAmount = 0.0f;
    target.captureQueued = false;
    target.captureCommitted = false;
    target.soulState = SoulState::Free;
    target.networkOwnerPlayerId = -1;
    return true;
}

inline bool isReusableTargetSlot(const TargetState& target) noexcept {
    return !target.alive &&
           !target.captureQueued &&
           !target.captureCommitted &&
           target.soulState == SoulState::Free;
}

} // namespace gameplay
