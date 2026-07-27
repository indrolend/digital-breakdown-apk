#!/usr/bin/env python3
from pathlib import Path

path = Path("native-desktop/main.cpp")
text = path.read_text(encoding="utf-8")
original = text

replacements = [
    (
        "    auto previousCamera = host.game.state().camera;\n    int captureFrames=0;",
        "    auto previousCamera = host.game.state().camera;\n    auto previousPhoneTransform = host.game.state().phoneTransform;\n    auto previousPlayerPosition = host.game.state().player.pos;\n    int captureFrames=0;",
    ),
    (
        "                previousCamera = host.game.state().camera;\n                host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));",
        "                previousCamera = host.game.state().camera;\n                previousPhoneTransform = host.game.state().phoneTransform;\n                previousPlayerPosition = host.game.state().player.pos;\n                host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));",
    ),
    (
        "            const auto& currentCamera = host.game.state().camera;\n            const bool cameraCut = previousCamera.firstPerson != currentCamera.firstPerson ||",
        "            const auto& currentCamera = host.game.state().camera;\n            const auto& currentPhoneTransform = host.game.state().phoneTransform;\n            const auto& currentPlayerPosition = host.game.state().player.pos;\n            const bool cameraCut = previousCamera.firstPerson != currentCamera.firstPerson ||",
    ),
    (
        "                renderState.time = std::max(0.0f, host.game.state().time - (1.0f - alpha) * static_cast<float>(SIMULATION_STEP_SECONDS));\n            }",
        "                renderState.time = std::max(0.0f, host.game.state().time - (1.0f - alpha) * static_cast<float>(SIMULATION_STEP_SECONDS));\n            }\n            const bool playerCut = lengthSq(previousPlayerPosition - currentPlayerPosition) > 25.0f ||\n                lengthSq(previousPhoneTransform.position - currentPhoneTransform.position) > 25.0f;\n            if (!playerCut) {\n                renderState.player.pos = previousPlayerPosition + (currentPlayerPosition - previousPlayerPosition) * alpha;\n                renderState.phoneTransform.position = previousPhoneTransform.position + (currentPhoneTransform.position - previousPhoneTransform.position) * alpha;\n                renderState.phoneTransform.orientation = quatSlerp(previousPhoneTransform.orientation, currentPhoneTransform.orientation, alpha);\n                renderState.phoneTransform.screenCenter = previousPhoneTransform.screenCenter + (currentPhoneTransform.screenCenter - previousPhoneTransform.screenCenter) * alpha;\n                renderState.phoneTransform.screenRight = normalized(previousPhoneTransform.screenRight + (currentPhoneTransform.screenRight - previousPhoneTransform.screenRight) * alpha);\n                renderState.phoneTransform.screenUp = normalized(previousPhoneTransform.screenUp + (currentPhoneTransform.screenUp - previousPhoneTransform.screenUp) * alpha);\n                renderState.phoneTransform.screenNormal = normalized(previousPhoneTransform.screenNormal + (currentPhoneTransform.screenNormal - previousPhoneTransform.screenNormal) * alpha);\n                renderState.phoneTransform.vacuumPullPoint = previousPhoneTransform.vacuumPullPoint + (currentPhoneTransform.vacuumPullPoint - previousPhoneTransform.vacuumPullPoint) * alpha;\n            }",
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected exactly one anchor, found {count}: {old[:100]!r}")
    text = text.replace(old, new, 1)

required = [
    "previousPhoneTransform",
    "previousPlayerPosition",
    "quatSlerp(previousPhoneTransform.orientation",
    "renderState.player.pos",
]
for marker in required:
    if marker not in text:
        raise SystemExit(f"missing transformed marker: {marker}")
if text == original:
    raise SystemExit("transform made no changes")

path.write_text(text, encoding="utf-8")
print("desktop phone interpolation transform applied")