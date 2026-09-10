#pragma once

#include <type_traits>

#include "../Game.hpp"

namespace gameplay {

static_assert(sizeof(SoulState) == 1, "SoulState is protocol-sensitive and must remain byte-sized");
static_assert(std::is_standard_layout_v<Vec3>, "Vec3 must remain standard-layout");
static_assert(std::is_standard_layout_v<InputState>, "InputState must remain standard-layout");
static_assert(std::is_standard_layout_v<PlayerState>, "PlayerState must remain standard-layout");
static_assert(std::is_standard_layout_v<TargetState>, "TargetState must remain standard-layout");
static_assert(std::is_standard_layout_v<BulletState>, "BulletState must remain standard-layout");

} // namespace gameplay
