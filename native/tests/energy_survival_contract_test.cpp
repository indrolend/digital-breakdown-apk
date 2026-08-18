#include "Game.hpp"

#include <cmath>
#include <cstdio>

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kHitFrameLimit = 120;

bool near(float actual, float expected, float tolerance = 0.02f) {
    return std::fabs(actual - expected) <= tolerance;
}

int fail(const char* caseName, const char* reason, const GameState& state) {
    std::fprintf(stderr,
                 "ENERGY_SURVIVAL_FAIL case=%s reason=%s battery=%.3f supplemental=%.3f "
                 "souls=%d alive=%d downed=%d dead=%d last_stand=%.3f regen_lock=%.3f\n",
                 caseName, reason, state.player.battery, state.energy.supplementalValue,
                 state.player.souls, state.player.alive ? 1 : 0, state.player.downed ? 1 : 0,
                 state.dead ? 1 : 0, state.progression.run.lastStandCooldown,
                 state.progression.run.batteryRegenLock);
    return 1;
}

void prepareHumanHit(Game& game, float battery) {
    GameState& state = game.networkMutableState();
    for (TargetState& target : state.targets) target = TargetState{};
    for (HumanRespawnRequest& request : state.respawnQueue) request = HumanRespawnRequest{};
    state.started = true;
    state.dead = false;
    state.uiPaused = false;
    state.cinematic = CinematicState{};
    state.enemyAttackOwner = -1;
    state.enemyAttackCadence = 0.0f;
    state.player.alive = true;
    state.player.downed = false;
    state.player.battery = battery;
    state.player.vel = {};
    state.player.jumpVel = 0.0f;
    state.player.grounded = true;
    state.player.grabbedByTarget = -1;
    state.progression.run.batteryRegenLock = 10.0f;
    state.progression.run.impactGuardTimer = 0.0f;
    state.progression.run.lastStandCooldown = 0.0f;

    TargetState& target = state.targets[0];
    target = TargetState{};
    target.alive = true;
    target.health = 1.0f;
    target.armor = 2.0f;
    target.attackCooldown = 0.0f;
    target.pos = state.player.pos + Vec3{0.0f, 0.0f, -1.0f};
    target.walkTarget = target.pos;
}

bool receiveHumanHit(Game& game) {
    for (int frame = 0; frame < kHitFrameLimit; ++frame) {
        game.setTouchControls(0.0f, 0.0f, 0.0f, 0.0f,
                              false, false, false, false, false, false);
        game.update(kDt);
        if (game.state().targets[0].attackHit) return true;
        if (game.state().dead) return false;
    }
    return false;
}

int supplementalFirst() {
    constexpr const char* kCase = "supplemental_first";
    Game game;
    game.reset();
    prepareHumanHit(game, 100.0f);
    GameState& state = game.networkMutableState();
    state.energy.supplementalActive = true;
    state.energy.supplementalValue = 10.0f;
    state.energy.supplementalMax = 10.0f;
    state.energy.flowerStacks = 1;
    if (!receiveHumanHit(game)) return fail(kCase, "hit_not_received", game.state());
    const GameState& after = game.state();
    if (!near(after.player.battery, 84.0f)) return fail(kCase, "main_battery_not_second_loss", after);
    if (after.energy.supplementalActive || !near(after.energy.supplementalValue, 0.0f))
        return fail(kCase, "supplemental_not_exhausted_first", after);
    return 0;
}

int survivalMitigation() {
    constexpr const char* kCase = "survival_mitigation";
    Game game;
    game.reset();
    game.setPersistentProgression(0, 2, 2, 2);
    prepareHumanHit(game, 100.0f);
    if (!receiveHumanHit(game)) return fail(kCase, "hit_not_received", game.state());
    const GameState& after = game.state();
    // Tier 1 survival scales the 26-point human hit by (1 - 0.11).
    if (!near(after.player.battery, 100.0f - 26.0f * 0.89f))
        return fail(kCase, "survival_damage_scale_changed", after);
    return 0;
}

int impactGuardMitigation() {
    constexpr const char* kCase = "impact_guard";
    Game game;
    game.reset();
    prepareHumanHit(game, 100.0f);
    game.networkMutableState().progression.run.impactGuardTimer = 1.0f;
    if (!receiveHumanHit(game)) return fail(kCase, "hit_not_received", game.state());
    const GameState& after = game.state();
    if (!near(after.player.battery, 100.0f - 26.0f * 0.42f))
        return fail(kCase, "impact_guard_scale_changed", after);
    return 0;
}

int lastStandPrecedence() {
    constexpr const char* kCase = "last_stand";
    Game game;
    game.reset();
    game.setPersistentProgression(0, 2, 2, 2);
    prepareHumanHit(game, 22.0f);
    if (!receiveHumanHit(game)) return fail(kCase, "hit_not_received", game.state());
    const GameState& after = game.state();
    if (!near(after.player.battery, 1.0f) || after.player.downed || after.dead ||
        after.progression.run.lastStandCooldown < 17.9f)
        return fail(kCase, "last_stand_did_not_claim_zero_hit", after);
    return 0;
}

int multiplayerDownedPrecedence() {
    constexpr const char* kCase = "multiplayer_downed";
    Game game;
    game.reset();
    game.configureNetworkHost();
    prepareHumanHit(game, 22.0f);
    if (!receiveHumanHit(game)) return fail(kCase, "hit_not_received", game.state());
    const GameState& after = game.state();
    if (!near(after.player.battery, 0.0f) || !after.player.downed || after.dead ||
        after.bleedoutTimer < 14.9f)
        return fail(kCase, "zero_hit_did_not_enter_downed_state", after);
    return 0;
}

int soloSoulRebootPrecedence() {
    constexpr const char* kCase = "solo_soul_reboot";
    Game game;
    game.reset();
    prepareHumanHit(game, 22.0f);
    GameState& state = game.networkMutableState();
    state.player.souls = 1;
    state.player.storedSoulBrute[0] = true;
    if (!receiveHumanHit(game)) return fail(kCase, "hit_not_received", game.state());
    const GameState& after = game.state();
    if (!near(after.player.battery, 15.0f) || after.player.souls != 0 ||
        !after.player.soloSoulRebootUsed || after.dead || after.player.downed ||
        after.progression.run.batteryRegenLock <= 0.70f)
        return fail(kCase, "zero_hit_did_not_consume_soul_reboot", after);
    return 0;
}

int nonHitExhaustionDeath() {
    constexpr const char* kCase = "non_hit_exhaustion_death";
    Game game;
    game.reset();
    GameState& state = game.networkMutableState();
    state.started = true;
    state.dead = false;
    state.uiPaused = false;
    state.cinematic = CinematicState{};
    state.player.alive = true;
    state.player.battery = 1.0f;
    state.player.souls = 0;
    state.player.grounded = true;
    state.progression.run.batteryRegenLock = 10.0f;
    game.setTouchControls(0.0f, 0.0f, 0.0f, 0.0f,
                          false, false, true, false, false, false);
    game.update(kDt);
    const GameState& after = game.state();
    if (!after.dead || after.player.alive || !near(after.player.battery, 0.0f))
        return fail(kCase, "ordinary_action_exhaustion_did_not_kill_run", after);
    return 0;
}

}  // namespace

int main() {
    if (supplementalFirst()) return 1;
    if (survivalMitigation()) return 1;
    if (impactGuardMitigation()) return 1;
    if (lastStandPrecedence()) return 1;
    if (multiplayerDownedPrecedence()) return 1;
    if (soloSoulRebootPrecedence()) return 1;
    if (nonHitExhaustionDeath()) return 1;
    std::printf("ENERGY_SURVIVAL_CONTRACT_OK cases=7\n");
    return 0;
}
