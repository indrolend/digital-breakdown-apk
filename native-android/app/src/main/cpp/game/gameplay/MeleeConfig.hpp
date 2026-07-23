#pragma once

#include <array>

namespace gameplay {

struct MeleeCombo {
    int variant;
    float range;
    float damage;
    float hitRadius;
    float visual;
    float dash;
    float dashSpeed;
    float cooldown;
    float recoilDistance;
    float recoilSpeed;
    float lunge;
    float cost;
};

inline constexpr std::array<MeleeCombo, 4> MELEE_COMBOS{{
    {0, 2.35f, 0.82f, 0.78f, 0.20f, 0.13f, 12.5f, 0.22f, 0.08f, 1.25f, 0.15f, 2.8f},
    {1, 2.85f, 1.08f, 0.90f, 0.25f, 0.18f, 14.0f, 0.27f, 0.12f, 1.75f, 0.22f, 3.6f},
    {2, 3.18f, 1.48f, 1.02f, 0.31f, 0.23f, 15.2f, 0.38f, 0.15f, 2.10f, 0.29f, 5.0f},
    {3, 3.00f, 1.22f, 0.96f, 0.29f, 0.20f, 13.8f, 0.34f, 0.12f, 1.80f, 0.25f, 4.2f},
}};

inline constexpr std::array<float, 4> MELEE_VARIANT_SIDE{{1.0f, -1.0f, 1.0f, -1.0f}};
inline constexpr std::array<float, 4> MELEE_VARIANT_ROLL{{-0.72f, 0.72f, -0.42f, 0.42f}};
inline constexpr std::array<float, 4> MELEE_VARIANT_YAW{{0.62f, -0.62f, 0.42f, -0.42f}};
inline constexpr std::array<float, 4> MELEE_VARIANT_PITCH{{-0.32f, -0.32f, 0.42f, 0.42f}};
inline constexpr std::array<float, 4> MELEE_VARIANT_LIFT{{0.012f, 0.012f, -0.006f, -0.006f}};

static_assert(MELEE_COMBOS.size() == MELEE_VARIANT_SIDE.size());
static_assert(MELEE_COMBOS.size() == MELEE_VARIANT_ROLL.size());
static_assert(MELEE_COMBOS.size() == MELEE_VARIANT_YAW.size());
static_assert(MELEE_COMBOS.size() == MELEE_VARIANT_PITCH.size());
static_assert(MELEE_COMBOS.size() == MELEE_VARIANT_LIFT.size());

} // namespace gameplay
