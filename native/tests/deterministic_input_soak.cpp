#include "Game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

constexpr std::uint32_t kSeed = 0xD1617A1u;
constexpr int kFrames = 36000;
constexpr float kDt = 1.0f / 60.0f;

struct Random {
    std::uint32_t value = kSeed;
    std::uint32_t next() {
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        return value;
    }
    float signedUnit() { return static_cast<float>(next() & 0xffffu) / 32767.5f - 1.0f; }
    bool oneIn(std::uint32_t count) { return next() % count == 0; }
};

bool finite(float value) { return std::isfinite(value); }
bool finite(const Vec3& value) {
    return finite(value.x) && finite(value.y) && finite(value.z) &&
           std::abs(value.x) < 1000000.0f && std::abs(value.y) < 1000000.0f &&
           std::abs(value.z) < 1000000.0f;
}

const char* invalidState(const GameState& state) {
    if (!finite(state.time) || !finite(state.player.pos) || !finite(state.player.vel) ||
        !finite(state.player.jumpVel) || !finite(state.player.yaw) ||
        !finite(state.player.battery) || state.player.battery < -0.001f ||
        state.player.battery > 100.001f) return "player";
    if (state.player.souls < 0 || state.player.souls > PHONE_CAPACITY ||
        state.player.grabbedByTarget < -1 || state.player.grabbedByTarget >= TARGET_COUNT)
        return "player_indices";
    if (!finite(state.camera.pos) || !finite(state.camera.forward) ||
        !finite(state.camera.lookTarget) || !finite(state.camera.yaw) ||
        !finite(state.camera.pitch)) return "camera";
    if (!finite(state.energy.supplementalValue) || !finite(state.energy.supplementalMax) ||
        state.energy.supplementalValue < -0.001f ||
        state.energy.supplementalValue > state.energy.supplementalMax + 0.001f)
        return "energy";
    if (state.vacuum.target < -1 || state.vacuum.target >= TARGET_COUNT ||
        !finite(state.vacuum.power) || !finite(state.vacuum.pose)) return "vacuum";
    if (state.debug.colliderCount < 0 || state.debug.colliderCount > ROOM_COLLIDER_COUNT)
        return "colliders";
    for (const TargetState& target : state.targets) {
        if (!finite(target.pos) || !finite(target.vel) || !finite(target.walkTarget) ||
            !finite(target.armor) || !finite(target.health) ||
            !finite(target.ingestProgress) || !finite(target.scale)) return "target";
        if (target.grabbedPlayerId < -1 || target.grabbedPlayerId >= NETWORK_PLAYER_COUNT)
            return "target_owner";
    }
    for (const BulletState& bullet : state.bullets)
        if (!finite(bullet.pos) || !finite(bullet.vel) || !finite(bullet.life)) return "bullet";
    for (const ParticleState& particle : state.particles)
        if (!finite(particle.pos) || !finite(particle.vel) || !finite(particle.life)) return "particle";
    return nullptr;
}

std::uint64_t mix(std::uint64_t hash, std::uint64_t value) {
    return (hash ^ value) * 1099511628211ull;
}

}  // namespace

int main() {
    Game game;
    game.reset();
    Random random;
    float moveX = 0.0f;
    float moveZ = 0.0f;
    int deaths = 0;
    int restarts = 0;
    int firstPersonToggles = 0;
    int upgradeChoices = 0;
    int deadFrames = 0;
    int maximumSouls = 0;
    int maximumRoom = game.state().roomIndex;

    for (int frame = 0; frame < kFrames; ++frame) {
        if (const char* invalid = invalidState(game.state())) {
            std::fprintf(stderr,
                         "INPUT_SOAK_FAIL seed=%u frame=%d subsystem=%s room=%d battery=%.3f deaths=%d\n",
                         kSeed, frame, invalid, game.state().roomIndex,
                         game.state().player.battery, deaths);
            return 1;
        }
        if (game.state().upgradeMenu.active) {
            if (!game.chooseTemporaryUpgrade(static_cast<int>(random.next() % 3u))) {
                std::fprintf(stderr, "INPUT_SOAK_FAIL seed=%u frame=%d subsystem=upgrade\n", kSeed, frame);
                return 1;
            }
            ++upgradeChoices;
        }
        if (game.state().dead) {
            if (deadFrames++ == 0) ++deaths;
            game.setTouchControls(0, 0, 0, 0, false, false, false, false, false, false);
            game.update(kDt);
            if (deadFrames >= 90) {
                game.restart();
                deadFrames = 0;
                ++restarts;
            }
            continue;
        }

        if ((frame % 75) == 0) {
            moveX = random.signedUnit();
            moveZ = random.signedUnit();
        }
        const bool vacuum = (random.next() % 100u) < 24u;
        const bool sprint = (random.next() % 100u) < 38u;
        const bool jump = random.oneIn(70);
        const bool melee = random.oneIn(42);
        const bool shoot = random.oneIn(95);
        const bool camera = random.oneIn(1200);
        if (camera) ++firstPersonToggles;
        game.setTouchControls(moveX, moveZ, random.signedUnit() * 2.4f,
                              random.signedUnit() * 1.5f, vacuum, sprint,
                              jump, melee, shoot, camera);
        game.update(kDt);
        maximumSouls = std::max(maximumSouls, game.state().player.souls);
        maximumRoom = std::max(maximumRoom, game.state().roomIndex);
    }

    std::uint64_t hash = 1469598103934665603ull;
    hash = mix(hash, static_cast<std::uint64_t>(deaths));
    hash = mix(hash, static_cast<std::uint64_t>(restarts));
    hash = mix(hash, static_cast<std::uint64_t>(game.state().frame));
    hash = mix(hash, static_cast<std::uint64_t>(game.state().roomIndex));
    hash = mix(hash, static_cast<std::uint64_t>(game.state().player.battery * 1000.0f));
    std::printf(
        "INPUT_SOAK_OK seed=%u frames=%d deaths=%d restarts=%d camera_toggles=%d "
        "upgrades=%d max_souls=%d max_room=%d hash=%llu\n",
        kSeed, kFrames, deaths, restarts, firstPersonToggles, upgradeChoices,
        maximumSouls, maximumRoom, static_cast<unsigned long long>(hash));
    return 0;
}
