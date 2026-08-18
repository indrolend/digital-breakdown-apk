#!/usr/bin/env python3
"""Apply the per-player mobile framing/action-assist ownership batch.

Exact-match and fail-closed: protocol, platform adapters, host simulation and
focused tests move together or nothing should be committed.
"""

from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
FILES={
  "game_hpp": ROOT/"native-android/app/src/main/cpp/game/Game.hpp",
  "game_cpp": ROOT/"native-android/app/src/main/cpp/game/Game.cpp",
  "protocol_hpp": ROOT/"native-network/MultiplayerProtocol.hpp",
  "protocol_cpp": ROOT/"native-network/MultiplayerProtocol.cpp",
  "android": ROOT/"native-android/app/src/main/cpp/native_bridge.cpp",
  "server": ROOT/"multiplayer-server/src/protocol.ts",
  "protocol_test": ROOT/"native/tests/multiplayer_protocol_test.cpp",
  "mobile_test": ROOT/"native/tests/mobile_framing_remote_authority_test.cpp",
}

def once(text,old,new,label):
  n=text.count(old)
  if n!=1: raise RuntimeError(f"{label}: expected one match, found {n}")
  return text.replace(old,new,1)

def main():
  try:
    text={k:p.read_text(encoding="utf-8") for k,p in FILES.items()}

    text["game_hpp"]=once(text["game_hpp"],
      "    std::uint16_t buttons = 0;\n};\n",
      "    std::uint16_t buttons = 0;\n    std::uint8_t capabilities = 0;\n};\n\nenum PlayerCommandCapability : std::uint8_t {\n    CommandMobileFraming = 1u << 0\n};\n",
      "PlayerCommand capability field")
    text["game_hpp"]=once(text["game_hpp"],
      "    unsigned short inputButtons = 0;\n    InputState input;\n",
      "    unsigned short inputButtons = 0;\n    bool mobileFraming = false;\n    InputState input;\n",
      "peer mobile capability state")
    text["game_hpp"]=once(text["game_hpp"],
      "    void setNetworkPeerInput(int playerId, unsigned int sequence, float moveX, float moveZ, float yaw, float pitch, unsigned short buttons);\n",
      "    void setNetworkPeerInput(int playerId, unsigned int sequence, float moveX, float moveZ, float yaw, float pitch, unsigned short buttons, unsigned char capabilities = 0);\n",
      "network input capability parameter")
    text["game_hpp"]=once(text["game_hpp"],
      "    Vec3 cameraRightFlat() const;\n",
      "    Vec3 cameraRightFlat() const;\n    bool mobileFramingEnabled() const;\n",
      "mobile framing owner helper declaration")

    text["game_cpp"]=once(text["game_cpp"],
      "    else if(input.commSignalPressed==4)command.buttons|=CommandCommOk;\n    return command;\n",
      "    else if(input.commSignalPressed==4)command.buttons|=CommandCommOk;\n    if(state_.localSettings.mobileFraming)command.capabilities|=CommandMobileFraming;\n    return command;\n",
      "capture local capability")
    text["game_cpp"]=once(text["game_cpp"],
      "    setNetworkPeerInput(playerId,command.sequence,command.moveX,command.moveZ,command.yaw,command.pitch,command.buttons);\n",
      "    setNetworkPeerInput(playerId,command.sequence,command.moveX,command.moveZ,command.yaw,command.pitch,command.buttons,command.capabilities);\n",
      "forward command capability")
    text["game_cpp"]=once(text["game_cpp"],
      "void Game::setNetworkPeerInput(int playerId,unsigned int sequence,float moveX,float moveZ,float yaw,float pitch,unsigned short buttons){",
      "void Game::setNetworkPeerInput(int playerId,unsigned int sequence,float moveX,float moveZ,float yaw,float pitch,unsigned short buttons,unsigned char capabilities){",
      "network input capability signature")
    text["game_cpp"]=once(text["game_cpp"],
      "peer.lastInputSequence=sequence;peer.inputButtons=buttons;peer.input.touchMoveX=",
      "peer.lastInputSequence=sequence;peer.inputButtons=buttons;peer.mobileFraming=(capabilities&CommandMobileFraming)!=0;peer.input.touchMoveX=",
      "store peer capability")
    text["game_cpp"]=once(text["game_cpp"],
      "Vec3 Game::cameraRightFlat() const {\n    return normalized({std::cos(state_.camera.yaw), 0.0f, -std::sin(state_.camera.yaw)});\n}\n",
      "Vec3 Game::cameraRightFlat() const {\n    return normalized({std::cos(state_.camera.yaw), 0.0f, -std::sin(state_.camera.yaw)});\n}\nbool Game::mobileFramingEnabled() const {\n    if(simulationPlayerId_>0&&state_.multiplayer.enabled&&state_.multiplayer.authoritativeHost)\n        return state_.multiplayer.peers[simulationPlayerId_].mobileFraming;\n    return state_.localSettings.mobileFraming;\n}\n",
      "mobile framing owner helper")
    text["game_cpp"]=once(text["game_cpp"],
      "    if(!state_.localSettings.mobileFraming || lengthSq(base)<0.0001f) return base;\n",
      "    if(!mobileFramingEnabled() || lengthSq(base)<0.0001f) return base;\n",
      "action assist owner")
    text["game_cpp"]=once(text["game_cpp"],
      "    const bool mobile = state_.localSettings.mobileFraming;\n",
      "    const bool mobile = mobileFramingEnabled();\n",
      "camera framing owner")

    text["protocol_hpp"]=once(text["protocol_hpp"],
      "constexpr std::uint16_t PROTOCOL_VERSION = 7;\n",
      "constexpr std::uint16_t PROTOCOL_VERSION = 8;\n",
      "native protocol version")
    text["protocol_cpp"]=once(text["protocol_cpp"],
      "payload.f32(input.moveX);payload.f32(input.moveZ);payload.f32(input.yaw);payload.f32(input.pitch);payload.u16(input.buttons);",
      "payload.f32(input.moveX);payload.f32(input.moveZ);payload.f32(input.yaw);payload.f32(input.pitch);payload.u16(input.buttons);payload.u8(input.capabilities);",
      "encode input capabilities")
    text["protocol_cpp"]=once(text["protocol_cpp"],
      "r.f32(input.moveX)&&r.f32(input.moveZ)&&r.f32(input.yaw)&&r.f32(input.pitch)&&r.u16(input.buttons)&&r.done();",
      "r.f32(input.moveX)&&r.f32(input.moveZ)&&r.f32(input.yaw)&&r.f32(input.pitch)&&r.u16(input.buttons)&&r.u8(input.capabilities)&&r.done();",
      "decode input capabilities")
    text["server"]=once(text["server"],"export const PROTOCOL_VERSION = 7;","export const PROTOCOL_VERSION = 8;","server protocol version")

    text["android"]=once(text["android"],
      "gGame.setNetworkPeerInput(header.playerId,input.sequence,input.moveX,input.moveZ,input.yaw,input.pitch,input.buttons);",
      "gGame.setNetworkPeerInput(header.playerId,input.sequence,input.moveX,input.moveZ,input.yaw,input.pitch,input.buttons,input.capabilities);",
      "Android host capability receive")
    text["android"]=once(text["android"],
      "dbnet::InputCommand input;input.sequence=++gNetworkSequence;input.localTick=",
      "dbnet::InputCommand input;input.sequence=++gNetworkSequence;input.capabilities=state.localSettings.mobileFraming?CommandMobileFraming:0;input.localTick=",
      "Android guest capability send")

    text["protocol_test"]=once(text["protocol_test"],
      "  input.buttons = Vacuum | Sprint;\n",
      "  input.buttons = Vacuum | Sprint;\n  input.capabilities = CommandMobileFraming;\n",
      "protocol capability fixture")
    text["protocol_test"]=once(text["protocol_test"],
      "         decoded.buttons == (Vacuum | Sprint) &&\n         std::abs(decoded.moveZ - 0.75f) < 0.0001f;\n",
      "         decoded.buttons == (Vacuum | Sprint) &&\n         decoded.capabilities == CommandMobileFraming &&\n         std::abs(decoded.moveZ - 0.75f) < 0.0001f;\n",
      "protocol capability roundtrip")
    text["protocol_test"]=once(text["protocol_test"],
      "  Game commandGame;\n  commandGame.reset();\n",
      "  Game commandGame;\n  commandGame.reset();\n  commandGame.networkMutableState().localSettings.mobileFraming = true;\n",
      "canonical command mobile fixture")
    text["protocol_test"]=once(text["protocol_test"],
      "        (canonical.buttons & CommandMelee) != 0;\n",
      "        (canonical.buttons & CommandMelee) != 0 &&\n        (canonical.capabilities & CommandMobileFraming) != 0;\n",
      "canonical command capability assertion")

    text["mobile_test"]='''#include <cmath>\n#include <cstdio>\n\n#include "Game.hpp"\n\nstruct HostRemotePeerSimulationIsolationAccess {\n    static void loadPlayerContext(Game& game, const NetworkPeerState& context) { game.loadPlayerContext(context); }\n    static void setSimulationPlayer(Game& game, int playerId) { game.simulationPlayerId_ = playerId; }\n    static Vec3 assistedActionDirection(const Game& game,const Vec3& origin,const Vec3& direction,float maxDistance,float minDot,float maxBlend,bool preferHead) {\n        return game.assistedActionDirection(origin,direction,maxDistance,minDot,maxBlend,preferHead);\n    }\n};\n\nnamespace {\nfloat vectorDistance(const Vec3& a,const Vec3& b){const Vec3 d=a-b;return std::sqrt(dot3(d,d));}\n\nbool remoteMobileFramingUsesPeerCapability() {\n    Game game;game.reset();game.configureNetworkHost();game.setNetworkPeerActive(1,true);\n    GameState& state=game.networkMutableState();for(auto& target:state.targets)target=TargetState{};\n    TargetState& target=state.targets[0];target.alive=true;target.pos={1.0f,0.08f,-2.5f};target.scale=1.0f;target.armor=2.0f;target.health=1.0f;\n    auto& peer=state.multiplayer.peers[1];peer.player.alive=true;peer.player.downed=false;peer.player.pos={0.0f,0.08f,0.0f};peer.camera.forward={0.0f,0.0f,-1.0f};\n    state.localSettings.mobileFraming=true;peer.mobileFraming=false;\n    HostRemotePeerSimulationIsolationAccess::loadPlayerContext(game,peer);HostRemotePeerSimulationIsolationAccess::setSimulationPlayer(game,1);\n    const Vec3 origin{0.0f,0.66f,0.0f},base{0.0f,0.0f,-1.0f};\n    const Vec3 remoteDesktop=HostRemotePeerSimulationIsolationAccess::assistedActionDirection(game,origin,base,3.1f,0.80f,0.28f,true);\n    game.networkMutableState().multiplayer.peers[1].mobileFraming=true;\n    const Vec3 remoteMobile=HostRemotePeerSimulationIsolationAccess::assistedActionDirection(game,origin,base,3.1f,0.80f,0.28f,true);\n    HostRemotePeerSimulationIsolationAccess::setSimulationPlayer(game,0);\n    const float delta=vectorDistance(remoteDesktop,remoteMobile);\n    const bool hostDidNotLeak=vectorDistance(remoteDesktop,base)<0.0001f;\n    const bool peerCapabilityChangedAim=delta>0.01f&&remoteMobile.x>remoteDesktop.x;\n    std::printf("MOBILE_FRAMING_PEER_CAPABILITY hostIsolated=%d peerChanged=%d delta=%.6f\\n",hostDidNotLeak?1:0,peerCapabilityChangedAim?1:0,delta);\n    return hostDidNotLeak&&peerCapabilityChangedAim;\n}\n}\n\nint main(){if(!remoteMobileFramingUsesPeerCapability()){std::fprintf(stderr,"MOBILE_FRAMING_PEER_CAPABILITY_FAILED\\n");return 1;}std::printf("MOBILE_FRAMING_PEER_CAPABILITY_OK\\n");return 0;}\n'''

    for k,p in FILES.items(): p.write_text(text[k],encoding="utf-8")
  except (OSError,RuntimeError) as exc:
    print(f"MOBILE_PLAYER_CAPABILITY_BATCH_FAIL {exc}",file=sys.stderr);return 1
  print("MOBILE_PLAYER_CAPABILITY_BATCH_APPLIED=PASS");return 0

if __name__=="__main__": raise SystemExit(main())
