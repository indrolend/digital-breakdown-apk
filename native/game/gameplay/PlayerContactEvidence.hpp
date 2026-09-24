#pragma once

#include <algorithm>
#include <cmath>

#include "Math.hpp"

namespace gameplay {

struct PlayerContactEvidence {
    float movement = 0.0f;
    float landing = 0.0f;
};

inline float finiteContactValue(float value, float fallback = 0.0f) {
    return std::isfinite(value) ? value : fallback;
}

// Stateless physical evidence derived from actual body motion. Slow analogue
// input, defensive movement, ordinary walking, and sprinting need no separate
// stealth flags: their resolved velocity naturally produces different contact.
inline PlayerContactEvidence playerContactEvidence(
    const Vec3& velocity,
    bool grounded,
    float landingImpact)
{
    const float speed = std::max(0.0f, std::min(12.0f,
        horizontalLength({finiteContactValue(velocity.x), 0.0f,
            finiteContactValue(velocity.z)})));
    const float moving = grounded
        ? std::max(0.0f, std::min(1.0f, (speed - 0.06f) / 5.5f))
        : 0.0f;
    // Contact energy rises progressively rather than switching modes. This
    // keeps deliberate movement useful without making it silent.
    const float movement = moving * (0.32f + moving * 0.68f);
    const float impact = std::max(0.0f, std::min(12.0f,
        finiteContactValue(landingImpact)));
    const float landing = std::max(0.0f, std::min(1.0f,
        (impact - 1.4f) / 5.0f));
    return {movement, landing};
}

} // namespace gameplay
