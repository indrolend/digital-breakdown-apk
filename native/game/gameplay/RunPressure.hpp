#pragma once
#include <algorithm>

namespace gameplay {

// One bounded description of run escalation. It separates "later" from
// individual stat multipliers so future objectives/generation can consume the
// same pressure signal without each inventing its own roomIndex formula.
struct RunPressure {
    float depth = 0.0f;
    float population = 0.0f;
    float complexity = 0.0f;
    float environment = 0.0f;
};

inline RunPressure runPressureForRoom(int roomIndex, int crowdedStacks, int ruleStacks) {
    const float depth = std::min(1.0f, std::max(0, roomIndex - 1) / 24.0f);
    RunPressure p{};
    p.depth = depth;
    p.population = std::min(1.0f, depth * 0.72f + std::max(0, crowdedStacks) * 0.08f);
    p.complexity = std::min(1.0f, depth * 0.88f + std::max(0, ruleStacks) * 0.035f);
    p.environment = std::min(1.0f, depth * 0.64f);
    return p;
}

} // namespace gameplay
