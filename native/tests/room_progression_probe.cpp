#include "Game.hpp"
#include "gameplay/TargetRoles.hpp"
#include "EarlyBrowserVisuals.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr float kRoomDepth = 42.0f;
// Exercise seed evolution, difficulty/rule saturation, environment rebuilding,
// upgrade interludes and door ownership far beyond an ordinary test session.
constexpr int kRooms = 256;

void step(Game& game, int frames = 1) {
    for (int frame = 0; frame < frames; ++frame) {
        game.setTouchControls(0, 0, 0, 0, false, false, false, false, false, false);
        game.update(kDt);
    }
}

int filledGoals(const GameState& state) {
    int count = 0;
    for (int i = 0; i < state.requiredSouls; ++i) {
        if (state.captures[i].filled) ++count;
    }
    return count;
}

int activeHumans(const GameState& state) {
    return static_cast<int>(std::count_if(
        state.targets.begin(), state.targets.end(),
        [](const TargetState& target) { return gameplay::isActiveHuman(target); }));
}

int ruleStacks(const GameState& state) {
    return state.runRules.requiredSlotStacks + state.runRules.crowdedRoomStacks +
           state.runRules.fasterSlurpStacks;
}

bool finiteState(const GameState& state) {
    return std::isfinite(state.player.pos.x) && std::isfinite(state.player.pos.y) &&
           std::isfinite(state.player.pos.z) && std::isfinite(state.player.battery) &&
           state.requiredSouls >= 5 && state.requiredSouls <= 8 &&
           activeHumans(state) >= 1 && activeHumans(state) <= TARGET_COUNT;
}

bool freshRoomGeometryValid(const GameState& state){
    const auto plan=early_browser_visuals::roomPlan(state.roomSeed,state.roomIndex);
    if(!early_browser_visuals::requiredRouteIsTraversable(plan,state.roomSeed,state.roomIndex))return false;
    const bool propsValid=early_browser_visuals::environmentPropsValid(plan,state.roomSeed,state.roomIndex);
    int expected=plan.obstacleCount+early_browser_visuals::physicalTraversalSurfaceCount(plan);
    if(propsValid)for(int i=0;i<early_browser_visuals::environmentPropCount(plan);++i)if(early_browser_visuals::environmentPropSolid(early_browser_visuals::environmentProp(plan,state.roomSeed,state.roomIndex,i)))++expected;
    expected=std::min(ROOM_COLLIDER_COUNT,expected);
    if(state.debug.colliderCount!=expected)return false;
    for(int i=0;i<state.debug.colliderCount;++i){const RoomCollider& c=state.roomColliders[i];if(!std::isfinite(c.center.x)||!std::isfinite(c.center.y)||!std::isfinite(c.center.z)||c.minX>=c.maxX||c.minZ>=c.maxZ||c.width<=0||c.height<=0||c.depth<=0)return false;if(state.player.pos.x>c.minX-0.20f&&state.player.pos.x<c.maxX+0.20f&&state.player.pos.z>c.minZ-0.20f&&state.player.pos.z<c.maxZ+0.20f)return false;}
    for(const TargetState& target:state.targets)if(gameplay::isActiveHuman(target))for(int i=0;i<state.debug.colliderCount;++i){const RoomCollider& c=state.roomColliders[i];if(target.pos.x>c.minX-0.20f&&target.pos.x<c.maxX+0.20f&&target.pos.z>c.minZ-0.20f&&target.pos.z<c.maxZ+0.20f)return false;}
    return true;
}

int fail(int iteration, const char* phase, const GameState& state) {
    std::fprintf(stderr,
                 "ROOM_PROBE_FAIL iteration=%d phase=%s room=%d seed=%d "
                 "required=%d filled=%d tokens=%lld rules=%d humans=%d\n",
                 iteration, phase, state.roomIndex, state.roomSeed,
                 state.requiredSouls, filledGoals(state),
                 static_cast<long long>(state.progression.permanent.tokens),
                 ruleStacks(state), activeHumans(state));
    return 1;
}

}  // namespace

int main() {
    Game game;
    game.reset();
    game.setPersistentProgression(0, 0, 0, 0);

    int secretWakeCount = 0;
    int maximumRequired = 0;
    int maximumHumans = 0;

    for (int iteration = 1; iteration <= kRooms; ++iteration) {
        const GameState& roomStart = game.state();
        if (!finiteState(roomStart) || !freshRoomGeometryValid(roomStart) || roomStart.roomClear || roomStart.upgradeMenu.active ||
            filledGoals(roomStart) != 0) {
            return fail(iteration, "invalid_room_start", roomStart);
        }
        const int room = roomStart.roomIndex;
        const int seed = roomStart.roomSeed;
        const int required = roomStart.requiredSouls;
        const int rulesBefore = ruleStacks(roomStart);
        const std::int64_t tokensBefore = roomStart.progression.permanent.tokens;
        maximumRequired = std::max(maximumRequired, required);
        maximumHumans = std::max(maximumHumans, activeHumans(roomStart));

        for (int shot = 0; shot < required; ++shot) {
            GameState& state = game.networkMutableState();
            const float tileOrigin = static_cast<float>(state.topology.currentTileIndex) * kRoomDepth;
            BulletState& bullet = state.bullets[0];
            bullet = BulletState{};
            bullet.alive = true;
            bullet.life = 1.0f;
            bullet.pos = state.captures[shot].pos + Vec3{0, 0, tileOrigin + 1.9f};
            bullet.vel = {0, 0, -25.0f};
            step(game);
            if (filledGoals(game.state()) != shot + 1) {
                return fail(iteration, "goal_not_filled_once", game.state());
            }
        }

        const GameState& cleared = game.state();
        if (!cleared.roomClear || filledGoals(cleared) != required ||
            cleared.progression.permanent.tokens != tokensBefore + required) {
            return fail(iteration, "room_not_cleared", cleared);
        }
        if (room == 10) {
            if (cleared.secretTv.knockCueTimer <= 0.0f) return fail(iteration, "secret_not_woken", cleared);
            ++secretWakeCount;
        }
        const std::int64_t stableTokens = cleared.progression.permanent.tokens;
        step(game);
        if (game.state().progression.permanent.tokens != stableTokens) {
            return fail(iteration, "duplicate_goal_reward", game.state());
        }

        GameState& crossing = game.networkMutableState();
        const float tileOrigin = static_cast<float>(crossing.topology.currentTileIndex) * kRoomDepth;
        crossing.player.pos = {0, PHONE_MODEL_HEIGHT * 0.5f, tileOrigin - 20.8f};
        crossing.player.vel = {0, 0, -20.0f};
        crossing.player.grounded = true;
        step(game, 2);

        const GameState& advanced = game.state();
        const int expectedRules = std::min(11, rulesBefore + 1);
        if (advanced.roomIndex != room + 1 || advanced.roomSeed == seed ||
            advanced.roomClear || filledGoals(advanced) != 0 ||
            !advanced.upgradeMenu.active || !advanced.uiPaused ||
            ruleStacks(advanced) != expectedRules || !finiteState(advanced)) {
            return fail(iteration, "invalid_room_advance", advanced);
        }

        if (!game.chooseTemporaryUpgrade((iteration - 1) % 3)) {
            return fail(iteration, "upgrade_choice_failed", game.state());
        }
        GameState& resumed = game.networkMutableState();
        resumed.player.pos.x += 3.1f;
        step(game);
        if (game.state().doorTransition.active || game.state().uiPaused ||
            game.state().upgradeMenu.active) {
            return fail(iteration, "transition_not_resolved", game.state());
        }
    }

    const GameState& finalState = game.state();
    std::printf(
        "ROOM_PROBE_OK rooms=%d final_room=%d tokens=%lld rules=%d "
        "max_required=%d max_humans=%d secret_wakes=%d\n",
        kRooms, finalState.roomIndex,
        static_cast<long long>(finalState.progression.permanent.tokens),
        ruleStacks(finalState), maximumRequired, maximumHumans, secretWakeCount);
    return secretWakeCount == 1 ? 0 : 1;
}
