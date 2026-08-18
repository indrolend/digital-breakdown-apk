#include <cstdio>

#include "Game.hpp"

namespace {

bool batchedCommandsOverwriteUnconsumedEdges() {
    Game game;
    game.reset();
    game.configureNetworkHost();
    game.setNetworkPeerActive(1, true);

    constexpr unsigned short kOneShotButtons =
        CommandJump | CommandMelee | CommandShoot | CommandCameraToggle | CommandCommHelp;

    game.setNetworkPeerInput(1, 1u, 0.0f, 0.0f, 0.0f, 0.0f, kOneShotButtons);
    const InputState afterPress = game.state().multiplayer.peers[1].input;

    game.setNetworkPeerInput(1, 2u, 0.0f, 0.0f, 0.0f, 0.0f, 0u);
    const InputState afterRelease = game.state().multiplayer.peers[1].input;

    const bool pressEdgesObserved =
        afterPress.jumpPressed &&
        afterPress.meleePressed &&
        afterPress.shootPressed &&
        afterPress.cameraTogglePressed &&
        afterPress.commSignalPressed == 1;

    const bool booleanEdgesOverwritten =
        !afterRelease.jumpPressed &&
        !afterRelease.meleePressed &&
        !afterRelease.shootPressed &&
        !afterRelease.cameraTogglePressed;

    const bool commSignalRemainsLatched = afterRelease.commSignalPressed == 1;

    std::printf(
        "NETWORK_INPUT_EDGE_OVERWRITE_OBSERVED initial=%d overwritten=%d commLatched=%d\n",
        pressEdgesObserved ? 1 : 0,
        booleanEdgesOverwritten ? 1 : 0,
        commSignalRemainsLatched ? 1 : 0);

    return pressEdgesObserved && booleanEdgesOverwritten && commSignalRemainsLatched;
}

}  // namespace

int main() {
    if (!batchedCommandsOverwriteUnconsumedEdges()) {
        std::fprintf(stderr, "NETWORK_INPUT_EDGE_CHARACTERIZATION_FAILED\n");
        return 1;
    }
    std::printf("NETWORK_INPUT_EDGE_CHARACTERIZATION_OK\n");
    return 0;
}
