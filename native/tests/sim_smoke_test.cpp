#include <cmath>
#include <iostream>

#include "../core/input_intent.hpp"
#include "../core/render_state.hpp"
#include "../core/sim_constants.hpp"
#include "../core/simulation.hpp"
#include "../core/world_state.hpp"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        return false;
    }
    std::cout << "PASS: " << message << "\n";
    return true;
}

bool changed(float a, float b) {
    return std::fabs(a - b) > 0.0001f;
}

void step(db::WorldState& world, const db::InputIntent& input, const db::SimConstants& constants, int ticks) {
    for (int i = 0; i < ticks; ++i) {
        db::updateWorld(world, input, constants, db::FIXED_DT);
    }
}

} // namespace

int main() {
    db::SimConstants constants;
    db::WorldState world;
    db::resetWorld(world, constants);

    bool ok = true;

    ok &= expect(world.targetCount == constants.activeTargets, "reset spawns active targets");
    ok &= expect(world.captureCount == constants.capturePoints, "reset spawns capture points");
    ok &= expect(world.player.souls == 0, "reset clears stored souls");
    ok &= expect(db::countAliveTargets(world) == constants.activeTargets, "all targets alive after reset");

    db::RenderFrame renderFrame;
    db::buildRenderFrame(renderFrame, world, constants);
    ok &= expect(renderFrame.boxCount > 12, "render frame emits room/player/target boxes");

    db::InputIntent look;
    look.lookX = 3.0f;
    look.lookY = 2.0f;
    const float oldYaw = world.camera.yaw;
    const float oldPitch = world.camera.pitch;
    step(world, look, constants, 6);
    ok &= expect(changed(world.camera.yaw, oldYaw), "look input changes camera yaw");
    ok &= expect(changed(world.camera.pitch, oldPitch), "look input changes camera pitch");
    ok &= expect(world.camera.pitch <= constants.cursorMaxPitch, "camera pitch remains clamped high");

    db::InputIntent move;
    move.moveZ = 1.0f;
    const float startZ = world.player.pos.z;
    step(world, move, constants, 12);
    ok &= expect(changed(world.player.pos.z, startZ), "forward movement changes player z using camera basis");

    db::InputIntent jump;
    jump.jump = true;
    step(world, jump, constants, 1);
    ok &= expect(!world.player.grounded, "jump leaves ground");
    ok &= expect(world.player.battery < constants.batteryMax, "jump costs battery");

    db::resetWorld(world, constants);
    world.player.pos.x = -8.0f;
    world.player.pos.z = -11.25f;

    db::InputIntent vacuum;
    vacuum.vacuum = true;
    step(world, vacuum, constants, 90);

    ok &= expect(world.player.souls >= 1, "vacuum captures at least one target");
    ok &= expect(db::countAliveTargets(world) < constants.activeTargets, "captured target is removed from alive count");
    ok &= expect(world.player.battery > 0.0f, "battery stays positive during capture route");

    db::resetWorld(world, constants);
    world.player.souls = 1;
    world.player.pos = world.captures[0].pos;
    world.player.pos.y = constants.playerGroundY;
    step(world, db::InputIntent{}, constants, 1);

    ok &= expect(world.player.souls == 0, "deposit spends one stored soul");
    ok &= expect(db::countFilledCaptures(world) == 1, "deposit fills capture point");

    db::resetWorld(world, constants);
    for (int i = 0; i < world.captureCount; ++i) {
        world.player.souls = 1;
        world.player.pos = world.captures[i].pos;
        world.player.pos.y = constants.playerGroundY;
        step(world, db::InputIntent{}, constants, 1);
    }

    ok &= expect(world.room.clear, "all capture points clear room");
    ok &= expect(world.room.roomIndex == 2, "room index advances on clear");

    db::buildRenderFrame(renderFrame, world, constants);
    ok &= expect(renderFrame.boxCount > 0, "render frame still emits after room clear");

    std::cout << "summary room=" << world.room.roomIndex
              << " clear=" << (world.room.clear ? 1 : 0)
              << " battery=" << world.player.battery
              << " souls=" << world.player.souls
              << " aliveTargets=" << db::countAliveTargets(world)
              << " filledCaptures=" << db::countFilledCaptures(world)
              << " boxes=" << renderFrame.boxCount
              << " player=(" << world.player.pos.x << "," << world.player.pos.y << "," << world.player.pos.z << ")"
              << " camera=(" << world.camera.pos.x << "," << world.camera.pos.y << "," << world.camera.pos.z << ")"
              << "\n";

    return ok ? 0 : 1;
}
