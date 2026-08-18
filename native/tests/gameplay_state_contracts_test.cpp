#include "gameplay/StateContracts.hpp"

#include <cmath>
#include <cstdio>

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kHitFrameLimit = 120;

bool near(float a, float b) {
    return std::abs(a - b) < 0.0001f;
}

bool nearEnergy(float a, float b) {
    return std::abs(a - b) < 0.02f;
}

int fail(const char* contract) {
    std::fprintf(stderr, "GAMEPLAY_STATE_CONTRACT_FAIL contract=%s\n", contract);
    return 1;
}

bool persistentSeedSurvived(const GameState& state) {
    return state.progression.permanent.tokens == 37 &&
        state.progression.permanent.levels[0] == 2 &&
        state.progression.permanent.levels[1] == 4 &&
        state.progression.permanent.levels[2] == 5 &&
        near(state.localSettings.musicVolume, 0.31f) &&
        near(state.localSettings.sfxVolume, 0.82f) &&
        state.localSettings.musicMuted &&
        state.localSettings.graphicsPreset == 2;
}

void seedPersistentState(Game& game) {
    game.setPersistentProgression(37, 2, 4, 5);
    auto& settings = game.networkMutableState().localSettings;
    settings.musicVolume = 0.31f;
    settings.sfxVolume = 0.82f;
    settings.musicMuted = true;
    settings.graphicsPreset = 2;
}

void prepareHumanHit(Game& game, float battery) {
    auto& state = game.networkMutableState();
    for (auto& target : state.targets) target = TargetState{};
    for (auto& request : state.respawnQueue) request = HumanRespawnRequest{};
    state.started = true;
    state.dead = false;
    state.uiPaused = false;
    state.cinematic = CinematicState{};
    state.enemyAttackOwner = -1;
    state.enemyAttackCadence = 0.0f;
    state.player.alive = true;
    state.player.downed = false;
    state.player.battery = battery;
    state.player.souls = 0;
    state.player.storedSoulBrute.fill(false);
    state.player.soloSoulRebootUsed = false;
    state.player.vel = {};
    state.player.jumpVel = 0.0f;
    state.player.grounded = true;
    state.player.grabbedByTarget = -1;
    state.progression.run.batteryRegenLock = 10.0f;
    state.progression.run.impactGuardTimer = 0.0f;
    state.progression.run.lastStandCooldown = 0.0f;

    auto& target = state.targets[0];
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

} // namespace

int main() {
    {
        Game game;
        seedPersistentState(game);

        auto& state = game.networkMutableState();
        state.localSettings.menuPage = LocalMenuPage::Graphics;
        state.localSettings.menuScroll = 7.0f;
        state.localSettings.menuHistoryDepth = 3;
        state.localSettings.rebindingAction = 4;
        state.progression.run.temporaryLevels = {5, 6, 7};
        state.player.battery = 13.0f;
        state.roomIndex = 99;

        game.reset();
        const GameState& reset = game.state();

        if (!persistentSeedSurvived(reset))
            return fail("reset_preserves_persistent_state");

        if (reset.localSettings.menuPage != LocalMenuPage::Main ||
            !near(reset.localSettings.menuScroll, 0.0f) ||
            reset.localSettings.menuHistoryDepth != 0 ||
            reset.localSettings.rebindingAction != -1)
            return fail("reset_clears_settings_navigation_transients");

        if (reset.progression.run.temporaryLevels != std::array<int, 3>{})
            return fail("reset_clears_run_progression");

        if (reset.multiplayer.enabled)
            return fail("reset_clears_network_runtime");
    }

    {
        Game game;
        seedPersistentState(game);
        game.prepareStartScreen();
        const GameState& state = game.state();

        if (!persistentSeedSurvived(state))
            return fail("start_screen_preserves_persistent_state");
        if (state.started || state.dead || state.uiPaused || state.hud.gameOver)
            return fail("start_screen_session_flags");
        if (state.audio.nextSerial != 1 || state.audio.slurpPlaying)
            return fail("start_screen_resets_audio_event_runtime");
    }

    {
        Game game;
        seedPersistentState(game);
        game.prepareAttractScreen();
        const GameState& state = game.state();

        if (!persistentSeedSurvived(state))
            return fail("attract_screen_preserves_persistent_state");
        if (!state.attractMode || !state.started || state.dead || state.uiPaused)
            return fail("attract_screen_session_flags");
        if (state.cinematic.introActive || state.cinematic.deathActive ||
            state.cinematic.menuEnterActive || state.cinematic.menuExitActive)
            return fail("attract_screen_resets_cinematic_runtime");
        if (!near(state.player.battery, 100.0f))
            return fail("attract_screen_recharges_showcase_player");
    }

    {
        Game game;
        seedPersistentState(game);
        game.configureNetworkHost();
        game.setNetworkPeerActive(1, true);

        auto& state = game.networkMutableState();
        state.roomInspector = true;
        state.progression.run.temporaryLevels = {4, 5, 6};
        state.energy.supplementalActive = true;
        state.energy.supplementalValue = 42.0f;
        state.energy.flowerStacks = 2;
        state.player.souls = 8;
        state.player.battery = 17.0f;

        game.debugStepRoomInspector(1, false);
        const GameState& rebuilt = game.state();

        if (!persistentSeedSurvived(rebuilt))
            return fail("room_rebuild_preserves_persistent_state");
        if (rebuilt.progression.run.temporaryLevels != std::array<int, 3>{4, 5, 6})
            return fail("room_rebuild_preserves_run_progression");
        if (!rebuilt.energy.supplementalActive ||
            !near(rebuilt.energy.supplementalValue, 42.0f) ||
            rebuilt.energy.flowerStacks != 2)
            return fail("room_rebuild_preserves_supplemental_energy");
        if (!rebuilt.multiplayer.enabled || !rebuilt.multiplayer.authoritativeHost ||
            !rebuilt.multiplayer.peers[1].active)
            return fail("room_rebuild_preserves_network_authority");
        if (rebuilt.player.souls != 0)
            return fail("room_rebuild_reconstructs_local_player_inventory");
    }

    {
        Game game;
        auto& current = game.networkMutableState().localSettings;
        current.menuPage = LocalMenuPage::Controls;
        current.menuScroll = 77.0f;
        current.menuHistoryDepth = 2;
        current.rebindingAction = 4;
        current.mobileFraming = true;

        LocalSettingsState incoming = current;
        incoming.musicVolume = 0.22f;
        incoming.graphicsPreset = 2;
        incoming.keyboardBindings[0] = 73;
        incoming.menuPage = LocalMenuPage::Graphics;
        incoming.menuScroll = 9.0f;
        incoming.menuHistoryDepth = 0;
        incoming.rebindingAction = -1;
        incoming.mobileFraming = false;

        game.applyLocalPreferences(incoming);
        const auto& applied = game.state().localSettings;
        if (!near(applied.musicVolume, 0.22f) || applied.graphicsPreset != 2 ||
            applied.keyboardBindings[0] != 73)
            return fail("local_preferences_apply_persistent_fields");
        if (applied.menuPage != LocalMenuPage::Controls || !near(applied.menuScroll, 77.0f) ||
            applied.menuHistoryDepth != 2 || applied.rebindingAction != 4)
            return fail("local_preferences_preserve_menu_session");
        if (!applied.mobileFraming)
            return fail("local_preferences_do_not_own_mobile_framing");
        game.setMobileFraming(false);
        if (game.state().localSettings.mobileFraming)
            return fail("mobile_framing_has_explicit_owner_api");
    }

    {
        Game guest;
        guest.configureNetworkGuest(1);
        guest.networkMutableState().player.battery = 47.0f;
        guest.networkMutableState().frame = 321;

        guest.restart();
        const GameState& state = guest.state();

        if (!state.multiplayer.enabled || state.multiplayer.authoritativeHost ||
            state.multiplayer.localPlayerId != 1 || !near(state.player.battery, 47.0f) ||
            state.frame != 321)
            return fail("restart_is_noop_for_network_guest");
    }

    {
        Game host;
        host.configureNetworkHost();
        host.setNetworkPeerActive(1, true);
        host.restart();
        const GameState& state = host.state();

        if (!state.multiplayer.enabled || !state.multiplayer.authoritativeHost ||
            state.multiplayer.localPlayerId != 0 || !state.multiplayer.peers[1].active)
            return fail("restart_rebuilds_host_and_preserves_active_peer_membership");
    }

    // Authoritative human-hit path: battery == 22 is intentionally the boundary
    // where a swing still resolves as damage rather than the low-battery grab.
    {
        Game game;
        game.reset();
        prepareHumanHit(game, 100.0f);
        auto& state = game.networkMutableState();
        state.energy.supplementalActive = true;
        state.energy.supplementalValue = 10.0f;
        state.energy.supplementalMax = 10.0f;
        state.energy.flowerStacks = 1;
        if (!receiveHumanHit(game)) return fail("energy_supplemental_hit_arrives");
        const auto& after = game.state();
        if (!nearEnergy(after.player.battery, 84.0f) || after.energy.supplementalActive ||
            !nearEnergy(after.energy.supplementalValue, 0.0f))
            return fail("energy_supplemental_absorbs_before_main_battery");
    }

    {
        Game game;
        game.reset();
        prepareHumanHit(game, 100.0f);
        auto& state = game.networkMutableState();
        state.player.souls = 1;
        state.player.storedSoulBrute[0] = false;
        state.energy.supplementalActive = true;
        state.energy.supplementalValue = 10.0f;
        state.energy.supplementalMax = 10.0f;
        state.energy.flowerStacks = 1;
        if (!receiveHumanHit(game)) return fail("energy_soul_efficiency_hit_arrives");
        const auto& after = game.state();
        constexpr float expected = 100.0f - (26.0f / 1.16f - 10.0f);
        if (!nearEnergy(after.player.battery, expected) || after.player.souls != 1 ||
            after.energy.supplementalActive || !nearEnergy(after.energy.supplementalValue, 0.0f))
            return fail("energy_soul_efficiency_precedes_supplemental_absorption");
    }

    {
        Game game;
        game.reset();
        game.setPersistentProgression(0, 2, 2, 2);
        prepareHumanHit(game, 100.0f);
        if (!receiveHumanHit(game)) return fail("energy_survival_mitigation_hit_arrives");
        if (!nearEnergy(game.state().player.battery, 100.0f - 26.0f * 0.89f))
            return fail("energy_survival_tier_mitigates_before_resource_loss");
    }

    {
        Game game;
        game.reset();
        prepareHumanHit(game, 100.0f);
        game.networkMutableState().progression.run.impactGuardTimer = 1.0f;
        if (!receiveHumanHit(game)) return fail("energy_impact_guard_hit_arrives");
        if (!nearEnergy(game.state().player.battery, 100.0f - 26.0f * 0.42f))
            return fail("energy_impact_guard_mitigates_before_resource_loss");
    }

    {
        Game game;
        game.reset();
        game.setPersistentProgression(0, 2, 2, 2);
        prepareHumanHit(game, 22.0f);
        if (!receiveHumanHit(game)) return fail("energy_last_stand_hit_arrives");
        const auto& after = game.state();
        if (!nearEnergy(after.player.battery, 1.0f) || after.player.downed || after.dead ||
            after.progression.run.lastStandCooldown < 17.9f)
            return fail("energy_last_stand_claims_zero_hit_before_other_survival");
    }

    {
        Game game;
        game.reset();
        game.configureNetworkHost();
        prepareHumanHit(game, 22.0f);
        if (!receiveHumanHit(game)) return fail("energy_multiplayer_downed_hit_arrives");
        const auto& after = game.state();
        if (!nearEnergy(after.player.battery, 0.0f) || !after.player.downed || after.dead ||
            after.player.bleedoutTimer < 14.9f)
            return fail("energy_multiplayer_zero_hit_becomes_downed");
    }

    {
        Game game;
        game.reset();
        prepareHumanHit(game, 22.0f);
        auto& state = game.networkMutableState();
        state.player.souls = 1;
        state.player.storedSoulBrute[0] = true;
        if (!receiveHumanHit(game)) return fail("energy_solo_reboot_hit_arrives");
        const auto& after = game.state();
        if (!nearEnergy(after.player.battery, 15.0f) || after.player.souls != 0 ||
            !after.player.soloSoulRebootUsed || after.dead || after.player.downed ||
            after.progression.run.batteryRegenLock <= 0.70f)
            return fail("energy_solo_zero_hit_consumes_one_soul_reboot");
    }

    {
        Game game;
        game.reset();
        auto& state = game.networkMutableState();
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
        const auto& after = game.state();
        if (!after.dead || !nearEnergy(after.player.battery, 0.0f))
            return fail("energy_non_hit_zero_exhaustion_triggers_run_death");
    }

    std::puts("GAMEPLAY_STATE_CONTRACTS_OK");
    return 0;
}
