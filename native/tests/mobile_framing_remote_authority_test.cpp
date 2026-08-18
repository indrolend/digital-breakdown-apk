#include <cmath>
#include <cstdio>

#include "Game.hpp"

struct HostRemotePeerSimulationIsolationAccess {
    static void loadPlayerContext(Game& game, const NetworkPeerState& context) { game.loadPlayerContext(context); }
    static void setSimulationPlayer(Game& game, int playerId) { game.simulationPlayerId_ = playerId; }
    static Vec3 assistedActionDirection(const Game& game,const Vec3& origin,const Vec3& direction,float maxDistance,float minDot,float maxBlend,bool preferHead) {
        return game.assistedActionDirection(origin,direction,maxDistance,minDot,maxBlend,preferHead);
    }
};

namespace {
float vectorDistance(const Vec3& a,const Vec3& b){const Vec3 d=a-b;return std::sqrt(dot3(d,d));}

bool remoteMobileFramingUsesPeerCapability() {
    Game game;game.reset();game.configureNetworkHost();game.setNetworkPeerActive(1,true);
    GameState& state=game.networkMutableState();for(auto& target:state.targets)target=TargetState{};
    TargetState& target=state.targets[0];target.alive=true;target.pos={1.0f,0.08f,-2.5f};target.scale=1.0f;target.armor=2.0f;target.health=1.0f;
    auto& peer=state.multiplayer.peers[1];peer.player.alive=true;peer.player.downed=false;peer.player.pos={0.0f,0.08f,0.0f};peer.camera.forward={0.0f,0.0f,-1.0f};
    state.localSettings.mobileFraming=true;peer.mobileFraming=false;
    HostRemotePeerSimulationIsolationAccess::loadPlayerContext(game,peer);HostRemotePeerSimulationIsolationAccess::setSimulationPlayer(game,1);
    const Vec3 origin{0.0f,0.66f,0.0f},base{0.0f,0.0f,-1.0f};
    const Vec3 remoteDesktop=HostRemotePeerSimulationIsolationAccess::assistedActionDirection(game,origin,base,3.1f,0.80f,0.28f,true);
    game.networkMutableState().multiplayer.peers[1].mobileFraming=true;
    const Vec3 remoteMobile=HostRemotePeerSimulationIsolationAccess::assistedActionDirection(game,origin,base,3.1f,0.80f,0.28f,true);
    HostRemotePeerSimulationIsolationAccess::setSimulationPlayer(game,0);
    const float delta=vectorDistance(remoteDesktop,remoteMobile);
    const bool hostDidNotLeak=vectorDistance(remoteDesktop,base)<0.0001f;
    const bool peerCapabilityChangedAim=delta>0.01f&&remoteMobile.x>remoteDesktop.x;
    std::printf("MOBILE_FRAMING_PEER_CAPABILITY hostIsolated=%d peerChanged=%d delta=%.6f\n",hostDidNotLeak?1:0,peerCapabilityChangedAim?1:0,delta);
    return hostDidNotLeak&&peerCapabilityChangedAim;
}
}

int main(){if(!remoteMobileFramingUsesPeerCapability()){std::fprintf(stderr,"MOBILE_FRAMING_PEER_CAPABILITY_FAILED\n");return 1;}std::printf("MOBILE_FRAMING_PEER_CAPABILITY_OK\n");return 0;}
