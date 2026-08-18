#include <cmath>
#include <cstdio>

#include "Game.hpp"

struct HostRemotePeerSimulationIsolationAccess {
    static void loadPlayerContext(Game& game, const NetworkPeerState& context) {
        game.loadPlayerContext(context);
    }

    static Vec3 assistedActionDirection(
        const Game& game,
        const Vec3& origin,
        const Vec3& direction,
        float maxDistance,
        float minDot,
        float maxBlend,
        bool preferHead) {
        return game.assistedActionDirection(
            origin, direction, maxDistance, minDot, maxBlend, preferHead);
    }
};

namespace {

float vectorDistance(const Vec3& a, const Vec3& b) {
    const Vec3 delta = a - b;
    return std::sqrt(dot3(delta, delta));
}

bool hostMobileFramingChangesLoadedRemoteAim() {
    Game game;
    game.reset();
    game.configureNetworkHost();
    game.setNetworkPeerActive(1, true);

    GameState& state = game.networkMutableState();
    for (auto& target : state.targets) target = TargetState{};

    TargetState& target = state.targets[0];
    target.alive = true;
    target.slurpable = false;
    target.captureQueued = false;
    target.captureCommitted = false;
    target.pos = {1.0f, 0.08f, -2.5f};
    target.scale = 1.0f;
    target.armor = 2.0f;
    target.health = 1.0f;

    NetworkPeerState remote = state.multiplayer.peers[1];
    remote.player.alive = true;
    remote.player.downed = false;
    remote.player.pos = {0.0f, 0.08f, 0.0f};
    remote.camera.yaw = 0.0f;
    remote.camera.pitch = 0.0f;
    remote.camera.forward = {0.0f, 0.0f, -1.0f};

    HostRemotePeerSimulationIsolationAccess::loadPlayerContext(game, remote);

    const Vec3 origin{0.0f, 0.66f, 0.0f};
    const Vec3 base{0.0f, 0.0f, -1.0f};

    game.networkMutableState().localSettings.mobileFraming = false;
    const Vec3 unassisted = HostRemotePeerSimulationIsolationAccess::assistedActionDirection(
        game, origin, base, 3.1f, 0.80f, 0.28f, true);

    game.networkMutableState().localSettings.mobileFraming = true;
    const bool hostSettingSurvivesRemoteContext = game.state().localSettings.mobileFraming;
    const Vec3 assisted = HostRemotePeerSimulationIsolationAccess::assistedActionDirection(
        game, origin, base, 3.1f, 0.80f, 0.28f, true);

    const float delta = vectorDistance(unassisted, assisted);
    const bool unassistedMatchesBase = vectorDistance(unassisted, base) < 0.0001f;
    const bool materiallyChanged = delta > 0.01f && assisted.x > unassisted.x;

    std::printf(
        "MOBILE_FRAMING_REMOTE_AUTHORITY_OBSERVED preserved=%d changed=%d delta=%.6f offX=%.6f onX=%.6f\n",
        hostSettingSurvivesRemoteContext ? 1 : 0,
        materiallyChanged ? 1 : 0,
        delta,
        unassisted.x,
        assisted.x);

    return hostSettingSurvivesRemoteContext && unassistedMatchesBase && materiallyChanged;
}

}  // namespace

int main() {
    if (!hostMobileFramingChangesLoadedRemoteAim()) {
        std::fprintf(stderr, "MOBILE_FRAMING_REMOTE_AUTHORITY_FAILED\n");
        return 1;
    }
    std::printf("MOBILE_FRAMING_REMOTE_AUTHORITY_OK\n");
    return 0;
}
