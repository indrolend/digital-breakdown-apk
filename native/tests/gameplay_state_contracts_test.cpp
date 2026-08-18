#include "gameplay/StateContracts.hpp"

#include <cmath>
#include <cstdio>

namespace {

bool near(float a, float b) {
    return std::abs(a - b) < 0.0001f;
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

    std::puts("GAMEPLAY_STATE_CONTRACTS_OK");
    return 0;
}
