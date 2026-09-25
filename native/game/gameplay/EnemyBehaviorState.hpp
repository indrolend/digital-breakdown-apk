#pragma once

#include <algorithm>
#include <cmath>

namespace gameplay {

enum class EnemyBehaviorMode {
    Rest,
    Orient,
    Investigate,
    Alert,
    Engage,
    Search
};

struct EnemyBehaviorState {
    EnemyBehaviorMode mode = EnemyBehaviorMode::Rest;
    float modeTime = 0.0f;
    float quietTime = 0.0f;
    float lostContactTime = 0.0f;
    bool hasSeenThreat = false;
};

struct EnemyBehaviorInput {
    float confidence = 0.0f;
    float uncertainty = 1.0f;
    float vagueAwareness = 0.0f;
    float physicalDisruption = 0.0f;
    bool confirmed = false;
    bool hasSpatialBelief = false;
    float dt = 0.0f;
};

struct EnemyBehaviorOutput {
    EnemyBehaviorMode mode = EnemyBehaviorMode::Rest;
    float travelScale = 0.0f;
    float commitment = 0.0f;
    bool mayAttack = false;
    bool settled = true;
};

inline float behaviorClamp01(float v) {
    return std::max(0.0f, std::min(1.0f, std::isfinite(v) ? v : 0.0f));
}

inline EnemyBehaviorOutput updateEnemyBehavior(EnemyBehaviorState& state, const EnemyBehaviorInput& raw) {
    const float dt = std::max(0.0f, std::min(0.1f, std::isfinite(raw.dt) ? raw.dt : 0.0f));
    const float confidence = behaviorClamp01(raw.confidence);
    const float uncertainty = behaviorClamp01(raw.uncertainty);
    const float awareness = behaviorClamp01(raw.vagueAwareness);
    const float disruption = behaviorClamp01(raw.physicalDisruption);
    state.modeTime = std::max(0.0f, state.modeTime + dt);

    if (raw.confirmed) {
        state.hasSeenThreat = true;
        state.lostContactTime = 0.0f;
        state.quietTime = 0.0f;
    } else {
        state.lostContactTime += dt;
        const bool quiet = !raw.hasSpatialBelief && confidence < 0.055f && awareness < 0.075f && disruption < 0.08f;
        state.quietTime = quiet ? state.quietTime + dt : 0.0f;
    }

    EnemyBehaviorMode next = state.mode;
    if (raw.confirmed && confidence >= 0.42f) {
        next = EnemyBehaviorMode::Engage;
    } else if (raw.confirmed) {
        next = EnemyBehaviorMode::Alert;
    } else if (state.hasSeenThreat && raw.hasSpatialBelief && confidence >= 0.07f) {
        next = EnemyBehaviorMode::Search;
    } else if (raw.hasSpatialBelief && confidence >= 0.16f) {
        next = EnemyBehaviorMode::Investigate;
    } else if (awareness >= 0.12f || (raw.hasSpatialBelief && confidence >= 0.055f)) {
        next = EnemyBehaviorMode::Orient;
    } else if (state.hasSeenThreat && state.lostContactTime < 5.0f && (confidence >= 0.035f || uncertainty < 0.96f)) {
        next = EnemyBehaviorMode::Search;
    } else if (state.quietTime >= 0.55f) {
        next = EnemyBehaviorMode::Rest;
        if (state.lostContactTime > 7.0f) state.hasSeenThreat = false;
    }

    // Hysteresis: cognition is punctuation, not another frame-by-frame oscillator.
    // A newly entered non-emergency mode owns a short readable beat before weak
    // evidence can demote it. Confirmed contact can always promote immediately.
    const bool promotion = static_cast<int>(next) > static_cast<int>(state.mode);
    if (next != state.mode && (raw.confirmed || promotion || state.modeTime >= 0.32f || next == EnemyBehaviorMode::Rest)) {
        state.mode = next;
        state.modeTime = 0.0f;
    }

    EnemyBehaviorOutput out{};
    out.mode = state.mode;
    switch (state.mode) {
        case EnemyBehaviorMode::Rest:        out.travelScale = 0.0f;  out.commitment = 0.0f; break;
        case EnemyBehaviorMode::Orient:      out.travelScale = 0.0f;  out.commitment = 0.18f; break;
        case EnemyBehaviorMode::Investigate: out.travelScale = 0.58f; out.commitment = 0.46f; break;
        case EnemyBehaviorMode::Alert:       out.travelScale = 0.72f; out.commitment = 0.66f; break;
        case EnemyBehaviorMode::Engage:      out.travelScale = 1.0f;  out.commitment = 1.0f; out.mayAttack = true; break;
        case EnemyBehaviorMode::Search:      out.travelScale = 0.48f; out.commitment = 0.52f; break;
    }
    out.settled = state.mode == EnemyBehaviorMode::Rest || state.mode == EnemyBehaviorMode::Orient;
    return out;
}

} // namespace gameplay
