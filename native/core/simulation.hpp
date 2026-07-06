#pragma once

#include "input_intent.hpp"
#include "sim_constants.hpp"
#include "world_state.hpp"

namespace db {

void resetWorld(WorldState& world, const SimConstants& c);
void updateWorld(WorldState& world, const InputIntent& input, const SimConstants& c, float dt);
void updatePlayer(WorldState& world, const InputIntent& input, const SimConstants& c, float dt);
void updateTargets(WorldState& world, const InputIntent& input, const SimConstants& c, float dt);
void updateCaptures(WorldState& world, const SimConstants& c, float dt);
void clampToRoom(Vec3& pos, const SimConstants& c);
int countAliveTargets(const WorldState& world);
int countFilledCaptures(const WorldState& world);

} // namespace db
