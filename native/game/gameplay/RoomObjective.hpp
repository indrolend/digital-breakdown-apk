#pragma once

#include <algorithm>

namespace gameplay {

enum class RoomObjectiveType : unsigned char {
    Harvest,
    Eliminate,
    Survive,
    Hunt,
    Escape,
    Infiltrate,
    Chain,
    Traversal
};

inline const char* roomObjectiveName(RoomObjectiveType type) {
    switch (type) {
        case RoomObjectiveType::Harvest: return "HARVEST";
        case RoomObjectiveType::Eliminate: return "ELIMINATE";
        case RoomObjectiveType::Survive: return "SURVIVE";
        case RoomObjectiveType::Hunt: return "HUNT";
        case RoomObjectiveType::Escape: return "ESCAPE";
        case RoomObjectiveType::Infiltrate: return "INFILTRATE";
        case RoomObjectiveType::Chain: return "CHAIN";
        case RoomObjectiveType::Traversal: return "TRAVERSAL";
    }
    return "UNKNOWN";
}

// Canonical room-purpose state. Pass 2 deliberately activates Harvest only;
// the other types are now vocabulary owned by one authority rather than future
// bespoke booleans spread through Game.cpp.
struct RoomObjectiveState {
    RoomObjectiveType type = RoomObjectiveType::Harvest;
    int target = 5;
    int progress = 0;
    float elapsed = 0.0f;
    float targetTime = 0.0f;
    bool complete = false;
    bool failed = false;
};

inline RoomObjectiveState makeHarvestObjective(int requiredSouls) {
    RoomObjectiveState objective{};
    objective.type = RoomObjectiveType::Harvest;
    objective.target = std::max(1, requiredSouls);
    return objective;
}

inline bool updateHarvestObjective(RoomObjectiveState& objective, int filledSlots, float dt) {
    objective.elapsed += std::max(0.0f, dt);
    objective.progress = std::max(0, std::min(objective.target, filledSlots));
    objective.complete = objective.progress >= objective.target;
    objective.failed = false;
    return objective.complete;
}

} // namespace gameplay
