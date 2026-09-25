#pragma once
#include <algorithm>
#include <cmath>

namespace gameplay {

// Interpretable run history for future invitation-based adaptation. These are
// observations, never hidden difficulty buffs. Composition may consume them;
// combat math must not directly punish a preferred verb.
struct PlayerBehaviorProfile {
    float observedTime = 0.0f;
    float movingTime = 0.0f;
    float sprintingTime = 0.0f;
    float airborneTime = 0.0f;
    float vacuumTime = 0.0f;
    float combatTime = 0.0f;
};

struct PlayerBehaviorSample {
    bool moving = false;
    bool sprinting = false;
    bool airborne = false;
    bool vacuuming = false;
    bool combatActive = false;
    float dt = 0.0f;
};

inline void observePlayerBehavior(PlayerBehaviorProfile& p, const PlayerBehaviorSample& s) {
    const float dt = std::max(0.0f, std::min(0.1f, std::isfinite(s.dt) ? s.dt : 0.0f));
    p.observedTime += dt;
    if (s.moving) p.movingTime += dt;
    if (s.sprinting) p.sprintingTime += dt;
    if (s.airborne) p.airborneTime += dt;
    if (s.vacuuming) p.vacuumTime += dt;
    if (s.combatActive) p.combatTime += dt;
}

inline float behaviorRatio(float value, const PlayerBehaviorProfile& p) {
    return p.observedTime > 0.001f ? std::max(0.0f, std::min(1.0f, value / p.observedTime)) : 0.0f;
}

} // namespace gameplay
