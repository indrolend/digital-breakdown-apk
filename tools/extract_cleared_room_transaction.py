#!/usr/bin/env python3
"""Extract cleared-room advancement policy into one named Game transaction.

This is a mechanical extraction. The existing transition body is lifted from
updateRoomTopology() after validating the policy anchors that matter to its
characterized behavior. Only the old enclosing block's closing indentation is
removed when the body becomes its own function.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
GAME_CPP = ROOT / "native-android/app/src/main/cpp/game/Game.cpp"
GAME_HPP = ROOT / "native-android/app/src/main/cpp/game/Game.hpp"

FUNCTION = "void Game::updateRoomTopology(float previousZ, float currentZ) {"
IF_MARKER = "    if (state_.roomClear && currentTile < previousTile) {"
DECL_MARKER = "    void updateRoomTopology(float previousZ, float currentZ);\n"

POLICY_ANCHORS = (
    "state_.doorTransition.active=true",
    "++state_.roomIndex",
    "advanceRunRulesForRoom();",
    "gainBattery(18.0f,BatteryReason::NextRoom);",
    "state_.player.souls=0;",
    "state_.player.storedSoulBrute.fill(false);",
    "state_.depositedSouls=0;",
    "state_.progression.run.roomHeat=0.0f;",
    "state_.progression.run.roomElapsed=0.0f;",
    "state_.progression.run.roomCaptures=0;",
    "state_.vacuum=VacuumState{};",
    "state_.meleeVisual=MeleeVisualState{};",
    "state_.energy.dischargeTimer=0.0f;",
    "for(auto& bullet:state_.bullets) bullet=BulletState{};",
    "for(auto& pending:state_.pendingShots) pending=PendingShotState{};",
    "for(auto& flower:state_.flowers) flower=FlowerPowerupState{};",
    "buildRoomColliders();",
    "for(auto& request:state_.respawnQueue) request=HumanRespawnRequest{};",
    "respawnTarget(i)",
    "state_.upgradeMenu.active=true;",
    "state_.uiPaused=true;",
    "clearInputState();",
)


def matching_brace(text: str, open_index: int) -> int:
    depth = 0
    in_string = False
    in_char = False
    escaped = False
    i = open_index
    while i < len(text):
        ch = text[i]
        if escaped:
            escaped = False
        elif ch == "\\" and (in_string or in_char):
            escaped = True
        elif ch == '"' and not in_char:
            in_string = not in_string
        elif ch == "'" and not in_string:
            in_char = not in_char
        elif not in_string and not in_char:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return i
        i += 1
    raise RuntimeError("unterminated brace block")


def main() -> int:
    try:
        cpp = GAME_CPP.read_text(encoding="utf-8")
        hpp = GAME_HPP.read_text(encoding="utf-8")

        if cpp.count("void Game::advanceClearedRoom()") != 0:
            raise RuntimeError("advanceClearedRoom already exists")
        if hpp.count("void advanceClearedRoom();") != 0:
            raise RuntimeError("advanceClearedRoom declaration already exists")
        if cpp.count(FUNCTION) != 1:
            raise RuntimeError(f"expected one updateRoomTopology definition, found {cpp.count(FUNCTION)}")
        if cpp.count(IF_MARKER) != 1:
            raise RuntimeError(f"expected one cleared-room transition block, found {cpp.count(IF_MARKER)}")
        if hpp.count(DECL_MARKER) != 1:
            raise RuntimeError(f"expected one updateRoomTopology declaration, found {hpp.count(DECL_MARKER)}")

        function_at = cpp.index(FUNCTION)
        marker_at = cpp.index(IF_MARKER, function_at)
        open_brace = cpp.index("{", marker_at + len(IF_MARKER) - 1)
        close_brace = matching_brace(cpp, open_brace)
        body = cpp[open_brace + 1:close_brace]

        for anchor in POLICY_ANCHORS:
            count = body.count(anchor)
            if count != 1:
                raise RuntimeError(f"policy anchor {anchor!r}: expected one match, found {count}")

        if "currentTile" in body or "previousTile" in body:
            raise RuntimeError("transition body unexpectedly depends on topology detector locals")
        if not body.endswith("\n    "):
            raise RuntimeError("cleared-room block closing indentation drifted")

        # The final four spaces belong to updateRoomTopology()'s closing brace,
        # not to the policy body itself. Remove that indentation plus its line
        # break, then add the new function's closing line normally.
        body = body[:-5]

        replacement = IF_MARKER + "\n        advanceClearedRoom();\n    }"
        cpp = cpp[:marker_at] + replacement + cpp[close_brace + 1:]

        extracted = "void Game::advanceClearedRoom() {" + body + "\n}\n\n"
        function_at = cpp.index(FUNCTION)
        cpp = cpp[:function_at] + extracted + cpp[function_at:]

        hpp = hpp.replace(DECL_MARKER, "    void advanceClearedRoom();\n" + DECL_MARKER, 1)

        GAME_CPP.write_text(cpp, encoding="utf-8")
        GAME_HPP.write_text(hpp, encoding="utf-8")
    except (OSError, RuntimeError) as exc:
        print(f"CLEARED_ROOM_TRANSACTION_FAIL {exc}", file=sys.stderr)
        return 1

    print("CLEARED_ROOM_TRANSACTION_APPLIED=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
