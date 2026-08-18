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

} // namespace

int main() {
    {
        Game game;
        game.setPersistentProgression(37, 2, 4, 5);

        auto& state = game.networkMutableState();
        state.localSettings.musicVolume = 0.31f;
        state.localSettings.sfxVolume = 0.82f;
        state.localSettings.musicMuted = true;
        state.localSettings.graphicsPreset = 2;
        state.localSettings.menuPage = LocalMenuPage::Graphics;
        state.localSettings.menuScroll = 7.0f;
        state.localSettings.menuHistoryDepth = 3;
        state.localSettings.rebindingAction = 4;
        state.progression.run.temporaryLevels = {5, 6, 7};
        state.player.battery = 13.0f;
        state.roomIndex = 99;

        game.reset();
        const GameState& reset = game.state();

        if (reset.progression.permanent.tokens != 37 ||
            reset.progression.permanent.levels[0] != 2 ||
            reset.progression.permanent.levels[1] != 4 ||
            reset.progression.permanent.levels[2] != 5)
            return fail("reset_preserves_permanent_progression");

        if (!near(reset.localSettings.musicVolume, 0.31f) ||
            !near(reset.localSettings.sfxVolume, 0.82f) ||
            !reset.localSettings.musicMuted ||
            reset.localSettings.graphicsPreset != 2)
            return fail("reset_preserves_persistent_settings");

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
