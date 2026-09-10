#pragma once

#include <algorithm>

namespace soul_economy {

// Stored souls are useful capital and ammunition, but carrying them makes
// passive recovery less efficient. This deliberately affects recovery only:
// action costs and locomotion remain independent of inventory weight.
constexpr float PASSIVE_REGEN_WEIGHT_PER_SOUL = 0.025f;

inline float passiveRegenMultiplier(int storedSouls) {
    const float souls = static_cast<float>(std::max(0, storedSouls));
    return 1.0f / (1.0f + souls * PASSIVE_REGEN_WEIGHT_PER_SOUL);
}

}  // namespace soul_economy
