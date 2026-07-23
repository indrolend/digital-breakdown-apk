#pragma once

#include <algorithm>
#include <cmath>

#include "TargetRoles.hpp"

namespace gameplay {

struct LooseSoulMotionConfig {
    float groundY = 0.08f;
    float gravity = 5.5f;
    float airborneDragPerSecond = 3.5f;
    float groundedDragPerSecond = 10.0f;
    float stopSpeed = 0.015f;
};

// Updates only simulation-owned Free/Recoiling soul motion. Vacuum-owned states
// remain under the vacuum system so update order and ownership stay explicit.
inline void updateLooseSoulMotion(
    TargetState& target,
    float dt,
    const LooseSoulMotionConfig& config = {}) noexcept {
    if (!isLooseSoul(target)) return;
    if (target.soulState != SoulState::Free &&
        target.soulState != SoulState::Recoiling) return;

    const float step = std::max(0.0f, dt);

    if (target.soulState == SoulState::Recoiling) {
        target.recoilTime = std::max(0.0f, target.recoilTime - step);
        if (target.recoilTime <= 0.0f) {
            target.soulState = SoulState::Free;
            target.networkOwnerPlayerId = -1;
        }
    }

    target.vel.y -= config.gravity * step;
    target.pos += target.vel * step;

    const float airborneDrag = std::exp(-config.airborneDragPerSecond * step);
    target.vel.x *= airborneDrag;
    target.vel.z *= airborneDrag;

    if (target.pos.y <= config.groundY) {
        target.pos.y = config.groundY;
        if (target.vel.y < 0.0f) target.vel.y = 0.0f;

        const float groundedDrag = std::exp(-config.groundedDragPerSecond * step);
        target.vel.x *= groundedDrag;
        target.vel.z *= groundedDrag;

        if (std::abs(target.vel.x) < config.stopSpeed) target.vel.x = 0.0f;
        if (std::abs(target.vel.z) < config.stopSpeed) target.vel.z = 0.0f;
    }
}

} // namespace gameplay
