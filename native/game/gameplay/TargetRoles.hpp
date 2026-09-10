#pragma once

#include "../Game.hpp"

namespace gameplay {

// These predicates derive semantic role from the existing pooled TargetState.
// They deliberately add no replicated state and do not change protocol layout.
inline bool isActiveHuman(const TargetState& target) noexcept {
    return target.alive &&
           !target.slurpable &&
           !target.captureQueued &&
           !target.captureCommitted;
}

inline bool isLooseSoul(const TargetState& target) noexcept {
    return target.alive &&
           target.slurpable &&
           !target.captureQueued &&
           !target.captureCommitted;
}

inline bool isCombatTarget(const TargetState& target) noexcept {
    return isActiveHuman(target);
}

inline bool isVacuumTarget(const TargetState& target) noexcept {
    return isLooseSoul(target) &&
           target.soulState != SoulState::Recoiling &&
           target.soulState != SoulState::Revolving;
}

inline bool isFreeVacuumOffer(const TargetState& target) noexcept {
    return isVacuumTarget(target) &&
           (target.soulState == SoulState::Free ||
            target.soulState == SoulState::Attracted);
}

} // namespace gameplay
