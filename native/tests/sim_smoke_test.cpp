#include <iostream>

#include "../core/input_intent.hpp"
#include "../core/sim_constants.hpp"
#include "../core/simulation.hpp"
#include "../core/world_state.hpp"

int main() {
    db::SimConstants constants;
    db::WorldState world;
    db::resetWorld(world, constants);

    db::InputIntent input;

    for (int tick = 0; tick < 300; ++tick) {
        // Simple deterministic test route: walk toward the first target and hold vacuum.
        input = db::InputIntent{};
        input.moveX = -0.35f;
        input.moveZ = -1.0f;
        input.vacuum = tick > 60;
        db::updateWorld(world, input, constants, db::FIXED_DT);
    }

    std::cout << "room=" << world.room.roomIndex
              << " clear=" << (world.room.clear ? 1 : 0)
              << " battery=" << world.player.battery
              << " souls=" << world.player.souls
              << " aliveTargets=" << db::countAliveTargets(world)
              << " filledCaptures=" << db::countFilledCaptures(world)
              << " player=(" << world.player.pos.x << "," << world.player.pos.y << "," << world.player.pos.z << ")"
              << "\n";

    return 0;
}
