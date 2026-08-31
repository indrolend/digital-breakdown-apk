#include <cmath>
#include <cstdio>

#include "Game.hpp"
#include "SoulEconomy.hpp"

namespace {

constexpr float kDt = 1.0f / 60.0f;

bool near(float a, float b, float epsilon = 0.0005f) {
    return std::fabs(a - b) <= epsilon;
}

void prepare(Game& game, int souls, float battery) {
    game.reset();
    GameState& state = game.networkMutableState();
    state.player.souls = souls;
    state.player.battery = battery;
    state.player.grounded = true;
    state.player.vel = {};
    state.player.jumpVel = 0.0f;
    state.progression.run.batteryRegenLock = 0.0f;
    state.progression.run.headshotRegenTax = 0.0f;
    state.progression.run.headshotRechargeBoost = 0.0f;
    state.secretTv.signal = 0;
    for (auto& target : state.targets) target.alive = false;
}

}  // namespace

int main() {
    bool ok = true;
    const float empty = soul_economy::passiveRegenMultiplier(0);
    const float moderate = soul_economy::passiveRegenMultiplier(PHONE_CAPACITY / 2);
    const float full = soul_economy::passiveRegenMultiplier(PHONE_CAPACITY);
    ok &= near(empty, 1.0f);
    ok &= empty > moderate && moderate > full && full > 0.0f;

    Game emptyIdle;
    Game moderateIdle;
    Game fullIdle;
    prepare(emptyIdle, 0, 20.0f);
    prepare(moderateIdle, PHONE_CAPACITY / 2, 20.0f);
    prepare(fullIdle, PHONE_CAPACITY, 20.0f);
    emptyIdle.update(kDt);
    moderateIdle.update(kDt);
    fullIdle.update(kDt);
    const float emptyGain = emptyIdle.state().player.battery - 20.0f;
    const float moderateGain = moderateIdle.state().player.battery - 20.0f;
    const float fullGain = fullIdle.state().player.battery - 20.0f;
    ok &= emptyGain > moderateGain && moderateGain > fullGain;
    ok &= near(moderateGain / emptyGain, moderate, 0.002f);
    ok &= near(fullGain / emptyGain, full, 0.002f);

    Game emptySprint;
    Game fullSprint;
    prepare(emptySprint, 0, 80.0f);
    prepare(fullSprint, PHONE_CAPACITY, 80.0f);
    emptySprint.setTouchControls(0.0f, 1.0f, 0.0f, 0.0f, false, true, false, false, false, false);
    fullSprint.setTouchControls(0.0f, 1.0f, 0.0f, 0.0f, false, true, false, false, false, false);
    emptySprint.update(kDt);
    fullSprint.update(kDt);
    const float emptyCost = 80.0f - emptySprint.state().player.battery;
    const float fullCost = 80.0f - fullSprint.state().player.battery;
    ok &= near(emptyCost, fullCost);
    ok &= near(emptySprint.state().player.pos.x, fullSprint.state().player.pos.x);
    ok &= near(emptySprint.state().player.pos.z, fullSprint.state().player.pos.z);
    ok &= near(emptySprint.state().player.vel.x, fullSprint.state().player.vel.x);
    ok &= near(emptySprint.state().player.vel.z, fullSprint.state().player.vel.z);

    Game firedSoul;
    prepare(firedSoul, PHONE_CAPACITY / 2, 80.0f);
    firedSoul.setTouchControls(0.0f, 0.0f, 0.0f, 0.0f, false, false, false, false, true, false);
    firedSoul.update(kDt);
    ok &= firedSoul.state().player.souls == PHONE_CAPACITY / 2 - 1;
    GameState& afterShot = firedSoul.networkMutableState();
    afterShot.input = {};
    afterShot.player.battery = 20.0f;
    afterShot.progression.run.batteryRegenLock = 0.0f;
    afterShot.energy.dischargeTimer = 0.0f;
    for (auto& pending : afterShot.pendingShots) pending.active = false;
    firedSoul.update(kDt);
    const float postShotGain = firedSoul.state().player.battery - 20.0f;
    ok &= postShotGain > moderateGain;
    ok &= near(postShotGain / emptyGain,
        soul_economy::passiveRegenMultiplier(PHONE_CAPACITY / 2 - 1), 0.002f);

    if (!ok) {
        std::fprintf(stderr,
            "SOUL_ECONOMY_FAILED multipliers=%.4f/%.4f/%.4f regen=%.4f/%.4f/%.4f post_shot=%.4f costs=%.4f/%.4f\n",
            empty, moderate, full, emptyGain, moderateGain, fullGain, postShotGain, emptyCost, fullCost);
        return 1;
    }
    std::printf(
        "SOUL_ECONOMY_OK multipliers=%.4f/%.4f/%.4f regen=%.4f/%.4f/%.4f post_shot=%.4f costs=%.4f/%.4f movement=MATCH\n",
        empty, moderate, full, emptyGain, moderateGain, fullGain, postShotGain, emptyCost, fullCost);
    return 0;
}
