#!/usr/bin/env python3
"""Finalize the proven network input edge-latching batch.

This script is intentionally exact-match and fail-closed. It converts the
characterization/temporary-repair setup into the permanent production contract.
"""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "native-android/app/src/main/cpp/game/Game.cpp"
CHAR = ROOT / "native/tests/network_input_edge_characterization_test.cpp"
REPAIR = ROOT / "native/tests/network_input_edge_repair_validation_test.cpp"
CMAKE = ROOT / "native-desktop/CMakeLists.txt"
BASH = ROOT / "scripts/verify-gameplay.sh"
PS = ROOT / "scripts/verify-gameplay.ps1"

EDGE_REPLACEMENTS = [
    (
        "peer.input.jumpPressed=(buttons&(1u<<5))!=0&&(previous&(1u<<5))==0;",
        "peer.input.jumpPressed=peer.input.jumpPressed||((buttons&(1u<<5))!=0&&(previous&(1u<<5))==0);",
    ),
    (
        "peer.input.meleePressed=(buttons&(1u<<7))!=0&&(previous&(1u<<7))==0;",
        "peer.input.meleePressed=peer.input.meleePressed||((buttons&(1u<<7))!=0&&(previous&(1u<<7))==0);",
    ),
    (
        "peer.input.shootPressed=(buttons&(1u<<8))!=0&&(previous&(1u<<8))==0;",
        "peer.input.shootPressed=peer.input.shootPressed||((buttons&(1u<<8))!=0&&(previous&(1u<<8))==0);",
    ),
    (
        "peer.input.cameraTogglePressed=(buttons&(1u<<9))!=0&&(previous&(1u<<9))==0;",
        "peer.input.cameraTogglePressed=peer.input.cameraTogglePressed||((buttons&(1u<<9))!=0&&(previous&(1u<<9))==0);",
    ),
]

PERMANENT_TEST = '''#include <cstdio>\n\n#include "Game.hpp"\n\nnamespace {\n\nbool edgesLatchUntilSimulationConsumesThem() {\n    Game game;\n    game.reset();\n    game.configureNetworkHost();\n    game.setNetworkPeerActive(1, true);\n\n    auto& peer = game.networkMutableState().multiplayer.peers[1];\n    peer.player.alive = true;\n    peer.player.downed = false;\n    peer.player.battery = 100.0f;\n    peer.player.grounded = true;\n    peer.player.grabbedByTarget = -1;\n\n    constexpr unsigned short kOneShotButtons =\n        CommandJump | CommandMelee | CommandShoot | CommandCameraToggle | CommandCommHelp;\n\n    game.setNetworkPeerInput(1, 1u, 0.0f, 0.0f, 0.0f, 0.0f, kOneShotButtons);\n    game.setNetworkPeerInput(1, 2u, 0.0f, 0.0f, 0.0f, 0.0f, 0u);\n\n    const InputState beforeSimulation = game.state().multiplayer.peers[1].input;\n    const bool latchedBeforeSimulation =\n        beforeSimulation.jumpPressed &&\n        beforeSimulation.meleePressed &&\n        beforeSimulation.shootPressed &&\n        beforeSimulation.cameraTogglePressed &&\n        beforeSimulation.commSignalPressed == 1;\n\n    game.update(1.0f / 60.0f);\n\n    const InputState afterSimulation = game.state().multiplayer.peers[1].input;\n    const bool consumedBySimulation =\n        !afterSimulation.jumpPressed &&\n        !afterSimulation.meleePressed &&\n        !afterSimulation.shootPressed &&\n        !afterSimulation.cameraTogglePressed &&\n        afterSimulation.commSignalPressed == 0;\n\n    std::printf(\n        "NETWORK_INPUT_EDGE_CONTRACT latched=%d consumed=%d\\n",\n        latchedBeforeSimulation ? 1 : 0,\n        consumedBySimulation ? 1 : 0);\n\n    return latchedBeforeSimulation && consumedBySimulation;\n}\n\n}  // namespace\n\nint main() {\n    if (!edgesLatchUntilSimulationConsumesThem()) {\n        std::fprintf(stderr, "NETWORK_INPUT_EDGE_CONTRACT_FAILED\\n");\n        return 1;\n    }\n    std::printf("NETWORK_INPUT_EDGE_CONTRACT_OK\\n");\n    return 0;\n}\n'''

CMAKE_REPAIR_BLOCK = '''\nadd_executable(\n    NetworkInputEdgeRepairValidationTest\n    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/network_input_edge_repair_validation_test.cpp"\n    "${DB_SHARED_ROOT}/game/Game.cpp"\n)\ntarget_include_directories(NetworkInputEdgeRepairValidationTest PRIVATE "${DB_SHARED_ROOT}/game")\ntarget_compile_features(NetworkInputEdgeRepairValidationTest PRIVATE cxx_std_17)\n'''

BASH_PLAN_LINE = "run_logged network-input-edge-latch-plan python3 tools/apply_network_input_edge_latching.py\n"
BASH_REPAIR_BLOCK = '''\nrun_logged network-input-edge-repair-apply python3 tools/apply_network_input_edge_latching.py --apply\nrun_logged network-input-edge-repair-build cmake --build "$BUILD_DIR" --config Release --target \\\n  NetworkInputEdgeRepairValidationTest \\\n  MultiplayerDeterminismTest \\\n  HostRemotePeerSimulationIsolationTest \\\n  --parallel\nrun_logged network-input-edge-repair-contract "$BUILD_DIR/NetworkInputEdgeRepairValidationTest"\nrun_logged network-input-edge-repair-determinism "$BUILD_DIR/MultiplayerDeterminismTest"\nrun_logged network-input-edge-repair-peer-isolation "$BUILD_DIR/HostRemotePeerSimulationIsolationTest"\nrun_logged network-input-edge-repair-restore git checkout -- "$GAME_CPP"\n'''

PS_PLAN_BLOCK = '''python tools/apply_network_input_edge_latching.py\nif ($LASTEXITCODE -ne 0) { throw "Network input edge latch plan check failed with exit code $LASTEXITCODE" }\n\n'''
PS_REPAIR_BLOCK = '''\npython tools/apply_network_input_edge_latching.py --apply\nif ($LASTEXITCODE -ne 0) { throw "Network input edge repair apply failed with exit code $LASTEXITCODE" }\n\ntry {\n    cmake --build $BuildDir --config Release --target NetworkInputEdgeRepairValidationTest MultiplayerDeterminismTest HostRemotePeerSimulationIsolationTest --parallel\n    if ($LASTEXITCODE -ne 0) { throw "Network input edge repair build failed with exit code $LASTEXITCODE" }\n\n    & (Join-Path $ReleaseDir "NetworkInputEdgeRepairValidationTest.exe")\n    if ($LASTEXITCODE -ne 0) { throw "NetworkInputEdgeRepairValidationTest failed with exit code $LASTEXITCODE" }\n\n    & (Join-Path $ReleaseDir "MultiplayerDeterminismTest.exe")\n    if ($LASTEXITCODE -ne 0) { throw "Patched MultiplayerDeterminismTest failed with exit code $LASTEXITCODE" }\n\n    & (Join-Path $ReleaseDir "HostRemotePeerSimulationIsolationTest.exe")\n    if ($LASTEXITCODE -ne 0) { throw "Patched HostRemotePeerSimulationIsolationTest failed with exit code $LASTEXITCODE" }\n}\nfinally {\n    git checkout -- $GameCpp\n    if ($LASTEXITCODE -ne 0) { throw "Failed to restore Game.cpp after repair validation with exit code $LASTEXITCODE" }\n}\n'''


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def main() -> int:
    try:
        game = GAME.read_text(encoding="utf-8")
        for old, new in EDGE_REPLACEMENTS:
            game = replace_once(game, old, new, old)

        cmake = CMAKE.read_text(encoding="utf-8")
        cmake = replace_once(cmake, CMAKE_REPAIR_BLOCK, "", "remove temporary repair target")

        bash = BASH.read_text(encoding="utf-8")
        bash = replace_once(bash, BASH_PLAN_LINE, "", "remove obsolete Bash patch-plan check")
        bash = replace_once(bash, BASH_REPAIR_BLOCK, "", "remove Bash temporary repair phase")
        bash = bash.replace('GAME_CPP="native-android/app/src/main/cpp/game/Game.cpp"\n\n', '', 1)

        ps = PS.read_text(encoding="utf-8")
        ps = replace_once(ps, PS_PLAN_BLOCK, "", "remove obsolete PowerShell patch-plan check")
        ps = replace_once(ps, PS_REPAIR_BLOCK, "", "remove PowerShell temporary repair phase")
        ps = ps.replace('$GameCpp = "native-android/app/src/main/cpp/game/Game.cpp"\n', '', 1)

        GAME.write_text(game, encoding="utf-8")
        CHAR.write_text(PERMANENT_TEST, encoding="utf-8")
        if not REPAIR.exists():
            raise RuntimeError("temporary repair validation file is missing")
        REPAIR.unlink()
        CMAKE.write_text(cmake, encoding="utf-8")
        BASH.write_text(bash, encoding="utf-8")
        PS.write_text(ps, encoding="utf-8")
    except (OSError, RuntimeError) as exc:
        print(f"NETWORK_INPUT_BATCH_FAIL {exc}", file=sys.stderr)
        return 1

    print("NETWORK_INPUT_BATCH_APPLIED=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
