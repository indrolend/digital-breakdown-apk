#!/usr/bin/env python3
from pathlib import Path

path = Path("native-desktop/main.cpp")
text = path.read_text(encoding="utf-8")
original = text

replacements = [
    (
        "    std::uint64_t savedProgressionRevision = 0;\n};",
        "    std::uint64_t savedProgressionRevision = 0;\n    bool pendingJumpPressed = false;\n    bool pendingMeleePressed = false;\n    bool pendingShootPressed = false;\n    bool pendingCameraPressed = false;\n};",
    ),
    (
        "        host.game.setTouchControls(\n            gamepad.moveX,\n            gamepad.moveZ,\n            static_cast<float>(host.lookX)*host.game.state().localSettings.mouseLookSensitivity+gamepad.lookX*host.game.state().localSettings.controllerLookSensitivity,\n            static_cast<float>(host.lookY)*host.game.state().localSettings.mouseLookSensitivity+gamepad.lookY*host.game.state().localSettings.controllerLookSensitivity,\n            vacuumHeld,\n            sprintHeld,\n            gamepad.jumpPressed,\n            gamepad.meleePressed,\n            gamepad.shootPressed,\n            gamepad.cameraPressed\n        );\n        host.lookX = 0.0;\n        host.lookY = 0.0;",
        "        host.pendingJumpPressed = host.pendingJumpPressed || gamepad.jumpPressed;\n        host.pendingMeleePressed = host.pendingMeleePressed || gamepad.meleePressed;\n        host.pendingShootPressed = host.pendingShootPressed || gamepad.shootPressed;\n        host.pendingCameraPressed = host.pendingCameraPressed || gamepad.cameraPressed;\n\n        const auto submitSimulationInput = [&](bool consumePendingLookAndEdges) {\n            const float mouseLookX = consumePendingLookAndEdges\n                ? static_cast<float>(host.lookX) * host.game.state().localSettings.mouseLookSensitivity\n                : 0.0f;\n            const float mouseLookY = consumePendingLookAndEdges\n                ? static_cast<float>(host.lookY) * host.game.state().localSettings.mouseLookSensitivity\n                : 0.0f;\n            host.game.setTouchControls(\n                gamepad.moveX,\n                gamepad.moveZ,\n                mouseLookX + gamepad.lookX * host.game.state().localSettings.controllerLookSensitivity,\n                mouseLookY + gamepad.lookY * host.game.state().localSettings.controllerLookSensitivity,\n                vacuumHeld,\n                sprintHeld,\n                consumePendingLookAndEdges && host.pendingJumpPressed,\n                consumePendingLookAndEdges && host.pendingMeleePressed,\n                consumePendingLookAndEdges && host.pendingShootPressed,\n                consumePendingLookAndEdges && host.pendingCameraPressed\n            );\n            if (consumePendingLookAndEdges) {\n                host.lookX = 0.0;\n                host.lookY = 0.0;\n                host.pendingJumpPressed = false;\n                host.pendingMeleePressed = false;\n                host.pendingShootPressed = false;\n                host.pendingCameraPressed = false;\n            }\n        };",
    ),
    (
        "        if (capturePath) {\n            previousCamera = host.game.state().camera;\n            host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));\n        } else {\n            int simulationSteps = 0;\n            while (simulationAccumulator >= SIMULATION_STEP_SECONDS && simulationSteps < MAX_SIMULATION_STEPS_PER_FRAME) {\n                previousCamera = host.game.state().camera;\n                host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));\n                simulationAccumulator -= SIMULATION_STEP_SECONDS;\n                ++simulationSteps;\n            }",
        "        if (capturePath) {\n            submitSimulationInput(true);\n            previousCamera = host.game.state().camera;\n            host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));\n        } else {\n            int simulationSteps = 0;\n            while (simulationAccumulator >= SIMULATION_STEP_SECONDS && simulationSteps < MAX_SIMULATION_STEPS_PER_FRAME) {\n                submitSimulationInput(simulationSteps == 0);\n                previousCamera = host.game.state().camera;\n                host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));\n                simulationAccumulator -= SIMULATION_STEP_SECONDS;\n                ++simulationSteps;\n            }",
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected exactly one anchor, found {count}: {old[:100]!r}")
    text = text.replace(old, new, 1)

required = [
    "pendingJumpPressed",
    "submitSimulationInput",
    "submitSimulationInput(simulationSteps == 0)",
]
for marker in required:
    if marker not in text:
        raise SystemExit(f"missing transformed marker: {marker}")
if text == original:
    raise SystemExit("transform made no changes")

path.write_text(text, encoding="utf-8")
print("desktop input consumption transform applied")
