#include "Game.hpp"
#include "gameplay/TargetRoles.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kCycles = 120;
constexpr int kCaptureFrameLimit = 360;
constexpr int kRespawnFrameLimit = 240;

bool finiteVec(const Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool validState(const GameState& state) {
    return state.started && !state.dead && state.player.alive &&
           finiteVec(state.player.pos) && finiteVec(state.player.vel) &&
           std::isfinite(state.player.battery);
}

void setControls(Game& game, bool vacuum, bool melee) {
    game.setTouchControls(0.0f, 0.0f, 0.0f, 0.0f,
                          vacuum, false, false, melee, false, false);
}

void step(Game& game, bool vacuum = false, bool melee = false) {
    setControls(game, vacuum, melee);
    game.update(kDt);
}

int activeHumanCount(const GameState& state) {
    int count = 0;
    for (const TargetState& target : state.targets) {
        if (gameplay::isActiveHuman(target)) ++count;
    }
    return count;
}

int activeHumanIndex(const GameState& state) {
    for (int i = 0; i < TARGET_COUNT; ++i) {
        if (gameplay::isActiveHuman(state.targets[i])) return i;
    }
    return -1;
}

int activeRespawnCount(const GameState& state) {
    return static_cast<int>(std::count_if(
        state.respawnQueue.begin(), state.respawnQueue.end(),
        [](const HumanRespawnRequest& request) { return request.active; }));
}

int fail(int cycle, const char* phase, const GameState& state) {
    std::fprintf(stderr,
                 "FLOW_PROBE_FAIL cycle=%d phase=%s souls=%d active_humans=%d "
                 "battery=%.3f player=(%.3f,%.3f,%.3f)\n",
                 cycle, phase, state.player.souls, activeHumanCount(state),
                 state.player.battery, state.player.pos.x, state.player.pos.y,
                 state.player.pos.z);
    return 1;
}

void prepareEncounter(GameState& state, int targetIndex) {
    TargetState& target = state.targets[targetIndex];
    target = TargetState{};
    target.alive = true;
    target.armor = 0.10f;
    target.health = 1.0f;
    target.scale = 1.0f;
    target.attackCooldown = 999.0f;
    target.pos = state.player.pos + Vec3{0.0f, 0.0f, -0.75f};
    target.walkTarget = target.pos;

    state.player.vel = {};
    state.player.jumpVel = 0.0f;
    state.player.grounded = true;
    state.player.grabbedByTarget = -1;
    state.camera.yaw = 0.0f;
    state.camera.pitch = 0.0f;
    state.camera.forward = {0.0f, 0.0f, -1.0f};
}

}  // namespace

int main() {
    Game game;
    game.reset();

    GameState& initial = game.networkMutableState();
    for (TargetState& target : initial.targets) target = TargetState{};
    for (HumanRespawnRequest& request : initial.respawnQueue) request = HumanRespawnRequest{};
    initial.cinematic = CinematicState{};
    initial.started = true;
    initial.dead = false;
    initial.uiPaused = false;
    initial.player.battery = 100.0f;
    prepareEncounter(initial, 0);

    int totalCaptureFrames = 0;
    int maximumCaptureFrames = 0;
    int totalRespawnFrames = 0;
    int maximumRespawnFrames = 0;
    int sawAttractedCycles = 0;
    int sawIngestingCycles = 0;

    for (int cycle = 1; cycle <= kCycles; ++cycle) {
        GameState& beforeAttack = game.networkMutableState();
        // Keep storage below its production capacity while retaining room heat,
        // population, RNG, particles, and every target lifecycle across the soak.
        if (beforeAttack.player.souls >= 20) {
            beforeAttack.player.souls = 0;
            beforeAttack.player.storedSoulBrute.fill(false);
        }
        const int targetIndex = activeHumanIndex(beforeAttack);
        if (targetIndex < 0 || activeHumanCount(beforeAttack) != 1) {
            return fail(cycle, "encounter_population", beforeAttack);
        }
        prepareEncounter(beforeAttack, targetIndex);
        const int soulsBefore = beforeAttack.player.souls;

        step(game, false, true);
        step(game);
        if (!game.state().targets[targetIndex].slurpable) {
            return fail(cycle, "melee_did_not_expose_soul", game.state());
        }
        if ((cycle % 2) == 0) {
            GameState& approach = game.networkMutableState();
            approach.targets[targetIndex].pos = approach.player.pos + Vec3{0.0f, 0.0f, -3.0f};
            approach.targets[targetIndex].walkTarget = approach.targets[targetIndex].pos;
        }

        bool sawAttracted = false;
        bool sawIngesting = false;
        int captureFrames = 0;
        for (; captureFrames < kCaptureFrameLimit; ++captureFrames) {
            step(game, true, false);
            const GameState& state = game.state();
            if (!validState(state)) return fail(cycle, "invalid_during_capture", state);
            if (state.player.souls > soulsBefore + 1) {
                return fail(cycle, "duplicate_capture_reward", state);
            }
            const SoulState soulState = state.targets[targetIndex].soulState;
            sawAttracted = sawAttracted || soulState == SoulState::Attracted;
            sawIngesting = sawIngesting || soulState == SoulState::Latched ||
                           soulState == SoulState::Ingesting;
            if (state.player.souls == soulsBefore + 1) break;
        }
        if (game.state().player.souls != soulsBefore + 1) {
            return fail(cycle, "capture_timeout", game.state());
        }
        if (activeRespawnCount(game.state()) != 1) {
            return fail(cycle, "missing_or_duplicate_respawn_request", game.state());
        }
        if (!sawIngesting) return fail(cycle, "missing_ingestion_transition", game.state());
        if (sawAttracted) ++sawAttractedCycles;
        ++sawIngestingCycles;
        ++captureFrames;
        totalCaptureFrames += captureFrames;
        maximumCaptureFrames = std::max(maximumCaptureFrames, captureFrames);

        step(game);
        if (game.state().player.souls != soulsBefore + 1) {
            return fail(cycle, "unstable_capture_reward", game.state());
        }

        int respawnFrames = 0;
        for (; respawnFrames < kRespawnFrameLimit; ++respawnFrames) {
            if (activeHumanCount(game.state()) > 0) break;
            step(game);
            if (!validState(game.state())) {
                return fail(cycle, "invalid_during_respawn", game.state());
            }
        }
        if (activeHumanCount(game.state()) != 1) {
            return fail(cycle, "respawn_timeout_or_duplicate", game.state());
        }
        if (activeRespawnCount(game.state()) != 0) {
            return fail(cycle, "respawn_request_not_consumed", game.state());
        }
        totalRespawnFrames += respawnFrames;
        maximumRespawnFrames = std::max(maximumRespawnFrames, respawnFrames);
    }

    std::printf(
        "FLOW_PROBE_OK cycles=%d souls=%d capture_avg=%.2f capture_max=%d "
        "respawn_avg=%.2f respawn_max=%d attracted_cycles=%d ingesting_cycles=%d\n",
        kCycles, game.state().player.souls,
        static_cast<double>(totalCaptureFrames) / kCycles, maximumCaptureFrames,
        static_cast<double>(totalRespawnFrames) / kCycles, maximumRespawnFrames,
        sawAttractedCycles, sawIngestingCycles);
    return 0;
}
