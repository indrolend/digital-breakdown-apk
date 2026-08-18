#include <cstdio>

#include "Game.hpp"

namespace {

bool edgesLatchUntilSimulationConsumesThem() {
    Game game;
    game.reset();
    game.configureNetworkHost();
    game.setNetworkPeerActive(1, true);

    auto& peer = game.networkMutableState().multiplayer.peers[1];
    peer.player.alive = true;
    peer.player.downed = false;
    peer.player.battery = 100.0f;
    peer.player.grounded = true;
    peer.player.grabbedByTarget = -1;

    constexpr unsigned short kOneShotButtons =
        CommandJump | CommandMelee | CommandShoot | CommandCameraToggle | CommandCommHelp;

    game.setNetworkPeerInput(1, 1u, 0.0f, 0.0f, 0.0f, 0.0f, kOneShotButtons);
    game.setNetworkPeerInput(1, 2u, 0.0f, 0.0f, 0.0f, 0.0f, 0u);

    const InputState beforeSimulation = game.state().multiplayer.peers[1].input;
    const bool latchedBeforeSimulation =
        beforeSimulation.jumpPressed &&
        beforeSimulation.meleePressed &&
        beforeSimulation.shootPressed &&
        beforeSimulation.cameraTogglePressed &&
        beforeSimulation.commSignalPressed == 1;

    game.update(1.0f / 60.0f);

    const InputState afterSimulation = game.state().multiplayer.peers[1].input;
    const bool consumedBySimulation =
        !afterSimulation.jumpPressed &&
        !afterSimulation.meleePressed &&
        !afterSimulation.shootPressed &&
        !afterSimulation.cameraTogglePressed &&
        afterSimulation.commSignalPressed == 0;

    std::printf(
        "NETWORK_INPUT_EDGE_REPAIR_OBSERVED latched=%d consumed=%d\n",
        latchedBeforeSimulation ? 1 : 0,
        consumedBySimulation ? 1 : 0);

    return latchedBeforeSimulation && consumedBySimulation;
}

}  // namespace

int main() {
    if (!edgesLatchUntilSimulationConsumesThem()) {
        std::fprintf(stderr, "NETWORK_INPUT_EDGE_REPAIR_VALIDATION_FAILED\n");
        return 1;
    }
    std::printf("NETWORK_INPUT_EDGE_REPAIR_VALIDATION_OK\n");
    return 0;
}
