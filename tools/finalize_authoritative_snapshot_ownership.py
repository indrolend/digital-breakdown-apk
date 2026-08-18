#!/usr/bin/env python3
"""Move authoritative snapshot mutation behind the multiplayer protocol boundary."""
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
HPP=ROOT/"native-network/MultiplayerProtocol.hpp"
CPP=ROOT/"native-network/MultiplayerProtocol.cpp"
DESKTOP=ROOT/"native-desktop/DesktopMultiplayer.cpp"
MAIN=ROOT/"native-desktop/main.cpp"
ANDROID=ROOT/"native-android/app/src/main/cpp/native_bridge.cpp"
TEST=ROOT/"native/tests/multiplayer_protocol_test.cpp"
RATCHET=ROOT/"tools/check_ownership_boundaries.py"


def once(text,old,new,label):
    n=text.count(old)
    if n!=1:
        raise RuntimeError(f"{label}: expected one match, found {n}")
    return text.replace(old,new,1)


def main():
    try:
        hpp=HPP.read_text(encoding="utf-8")
        cpp=CPP.read_text(encoding="utf-8")
        desktop=DESKTOP.read_text(encoding="utf-8")
        main_cpp=MAIN.read_text(encoding="utf-8")
        android=ANDROID.read_text(encoding="utf-8")
        test=TEST.read_text(encoding="utf-8")
        ratchet=RATCHET.read_text(encoding="utf-8")

        hpp=once(hpp,
            "void applyWorld(GameState& state, const WorldSnapshot& snapshot, std::uint8_t localPlayerId);\nvoid prepareForAuthoritativeWorldReplacement(GameState& state);\n",
            "void applyWorld(GameState& state, const WorldSnapshot& snapshot, std::uint8_t localPlayerId);\nvoid applyWorld(Game& game, const WorldSnapshot& snapshot, std::uint8_t localPlayerId);\nvoid prepareForAuthoritativeWorldReplacement(GameState& state);\n",
            "protocol Game snapshot boundary declaration")

        cpp=once(cpp,
            "\nvoid prepareForAuthoritativeWorldReplacement(GameState& state){\n",
            "\nvoid applyWorld(Game& game,const WorldSnapshot& snapshot,std::uint8_t localPlayerId){\n  applyWorld(game.networkMutableState(),snapshot,localPlayerId);\n}\n\nvoid prepareForAuthoritativeWorldReplacement(GameState& state){\n",
            "protocol Game snapshot boundary implementation")

        android=once(android,
            "dbnet::applyWorld(gGame.networkMutableState(),snapshot,static_cast<std::uint8_t>(gNetworkPlayerId));",
            "dbnet::applyWorld(gGame,snapshot,static_cast<std::uint8_t>(gNetworkPlayerId));",
            "Android authoritative snapshot boundary")

        desktop=once(desktop,
            "GameState& mutableState=game.networkMutableState();const auto local=",
            "const GameState& beforeState=game.state();const auto local=",
            "desktop mutable snapshot handle")
        desktop=once(desktop,"const Vec3 before=mutableState.player.pos;","const Vec3 before=beforeState.player.pos;","desktop previous position")
        desktop=once(desktop,"const auto beforeAction=mutableState.meleeVisual.actionSequence;","const auto beforeAction=beforeState.meleeVisual.actionSequence;","desktop previous action")
        desktop=once(desktop,
            "const float localProgress=mutableState.meleeVisual.visualDuration>0?1.0f-mutableState.meleeVisual.visualTimer/mutableState.meleeVisual.visualDuration:0.0f;",
            "const float localProgress=beforeState.meleeVisual.visualDuration>0?1.0f-beforeState.meleeVisual.visualTimer/beforeState.meleeVisual.visualDuration:0.0f;",
            "desktop previous action progress")
        desktop=once(desktop,
            "dbnet::applyWorld(mutableState,snapshot,local);auto applied=dbnet::captureWorld(mutableState,dbnet::capturePlayers(mutableState),snapshot.tick);",
            "dbnet::applyWorld(game,snapshot,local);const GameState& appliedState=game.state();auto applied=dbnet::captureWorld(appliedState,dbnet::capturePlayers(appliedState),snapshot.tick);",
            "desktop authoritative apply transaction")

        main_cpp=once(main_cpp,
            "    auto& settings=source.networkMutableState().localSettings;\n",
            "    LocalSettingsState settings=source.state().localSettings;\n",
            "desktop save fixture does not need mutation authority")

        test=once(test,
            "  applyWorld(completeGuest.networkMutableState(), roundtrip, 1);\n",
            "  applyWorld(completeGuest, roundtrip, 1);\n",
            "protocol Game overload contract")

        ratchet=once(ratchet,'    Path("native-desktop/main.cpp"),\n',"","remove migrated desktop main allowance")
        ratchet=once(ratchet,'    Path("native-desktop/DesktopMultiplayer.cpp"),\n',"","remove migrated desktop multiplayer allowance")
        ratchet=once(ratchet,'    Path("native-android/app/src/main/cpp/native_bridge.cpp"),\n',"","remove migrated Android bridge allowance")

        HPP.write_text(hpp,encoding="utf-8")
        CPP.write_text(cpp,encoding="utf-8")
        DESKTOP.write_text(desktop,encoding="utf-8")
        MAIN.write_text(main_cpp,encoding="utf-8")
        ANDROID.write_text(android,encoding="utf-8")
        TEST.write_text(test,encoding="utf-8")
        RATCHET.write_text(ratchet,encoding="utf-8")
    except (OSError,RuntimeError) as exc:
        print(f"AUTHORITATIVE_SNAPSHOT_OWNERSHIP_FAIL {exc}",file=sys.stderr)
        return 1
    print("AUTHORITATIVE_SNAPSHOT_OWNERSHIP_APPLIED=PASS")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
