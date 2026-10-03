#pragma once

#include "Game.hpp"

#include <algorithm>
#include <cstdint>

// Query-time projection for the physical phone. The world remains the sole
// authority; this model only decides how its current state is communicated.
struct GameplayPhoneModel {
    float batteryFill = 0.0f;
    int batteryPercent = 0;
    bool lowBattery = false;
    int storedSouls = 0;
    int soulCapacity = PHONE_CAPACITY;
    int filledGoals = 0;
    int requiredGoals = 0;
    bool roomClear = false;
    int roomIndex = 0;
    std::int64_t tokens = 0;
    bool supplementalActive = false;
    float supplementalFill = 0.0f;
    int flowerStacks = 0;
};

inline GameplayPhoneModel makeGameplayPhoneModel(const GameState& state) {
    GameplayPhoneModel model;
    model.batteryFill = clampf(state.player.battery / 100.0f, 0.0f, 1.0f);
    model.batteryPercent = static_cast<int>(model.batteryFill * 100.0f + 0.5f);
    model.lowBattery = state.hud.lowBattery;
    model.storedSouls = std::max(0, std::min(PHONE_CAPACITY, state.player.souls));
    model.filledGoals = std::max(0, state.depositedSouls);
    model.requiredGoals = std::max(0, state.requiredSouls);
    model.roomClear = state.roomClear;
    model.roomIndex = std::max(0, state.roomIndex);
    model.tokens = std::max<std::int64_t>(0, state.progression.permanent.tokens);
    model.supplementalActive = state.energy.supplementalActive || state.energy.supplementalValue > 0.001f;
    model.supplementalFill = state.energy.supplementalMax > 0.0f
        ? clampf(state.energy.supplementalValue / state.energy.supplementalMax, 0.0f, 1.0f)
        : 0.0f;
    model.flowerStacks = std::max(0, state.energy.flowerStacks);
    return model;
}
