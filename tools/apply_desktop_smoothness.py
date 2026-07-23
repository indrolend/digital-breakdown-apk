#!/usr/bin/env python3
from pathlib import Path

path = Path("native-desktop/main.cpp")
text = path.read_text(encoding="utf-8")
original = text

initial_replacements = [
    (
        "constexpr double PRESENTATION_STEP_SECONDS = 1.0 / 60.0;\nconstexpr double MAX_FRAME_DELTA_SECONDS = 0.25;",
        "constexpr double SIMULATION_STEP_SECONDS = 1.0 / 60.0;\nconstexpr double MAX_FRAME_DELTA_SECONDS = 0.10;",
    ),
    (
        "    // A software deadline below owns the 60 Hz presentation rate. Disabling\n    // monitor-rate vsync avoids running gameplay at 120/144 Hz or being\n    // double-throttled on displays whose refresh is not an even multiple of 60.\n    glfwSwapInterval(0);",
        "    // Let the platform compositor synchronize presentation to the active display.\n    // Gameplay remains fixed at 60 Hz and the renderer interpolates the camera.\n    glfwSwapInterval(1);",
    ),
    (
        "    auto previous = std::chrono::steady_clock::now();\n    auto nextPresentation = previous;\n    double simulationAccumulator = 0.0;",
        "    auto previous = std::chrono::steady_clock::now();\n    double simulationAccumulator = 0.0;\n    auto previousCamera = host.game.state().camera;",
    ),
    (
        "        if (!capturePath) {\n            const auto beforePacing = std::chrono::steady_clock::now();\n            if (beforePacing < nextPresentation) std::this_thread::sleep_until(nextPresentation);\n        }\n        glfwPollEvents();",
        "        glfwPollEvents();",
    ),
    (
        "        if (!capturePath) {\n            if (now > nextPresentation + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(PRESENTATION_STEP_SECONDS * 4.0))) nextPresentation = now;\n            nextPresentation += std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(PRESENTATION_STEP_SECONDS));\n            simulationAccumulator += std::min(elapsed, MAX_FRAME_DELTA_SECONDS);\n        }",
        "        if (!capturePath) simulationAccumulator += std::min(elapsed, MAX_FRAME_DELTA_SECONDS);",
    ),
    (
        "        if (capturePath) {\n            host.game.update(static_cast<float>(PRESENTATION_STEP_SECONDS));\n        } else {\n            int simulationSteps = 0;\n            while (simulationAccumulator >= PRESENTATION_STEP_SECONDS && simulationSteps < MAX_SIMULATION_STEPS_PER_FRAME) {\n                host.game.update(static_cast<float>(PRESENTATION_STEP_SECONDS));\n                simulationAccumulator -= PRESENTATION_STEP_SECONDS;\n                ++simulationSteps;\n            }\n            if (simulationSteps == MAX_SIMULATION_STEPS_PER_FRAME && simulationAccumulator >= PRESENTATION_STEP_SECONDS) simulationAccumulator = 0.0;\n        }",
        "        if (capturePath) {\n            previousCamera = host.game.state().camera;\n            host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));\n        } else {\n            int simulationSteps = 0;\n            while (simulationAccumulator >= SIMULATION_STEP_SECONDS && simulationSteps < MAX_SIMULATION_STEPS_PER_FRAME) {\n                previousCamera = host.game.state().camera;\n                host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));\n                simulationAccumulator -= SIMULATION_STEP_SECONDS;\n                ++simulationSteps;\n            }\n            if (simulationSteps == MAX_SIMULATION_STEPS_PER_FRAME && simulationAccumulator >= SIMULATION_STEP_SECONDS)\n                simulationAccumulator = std::fmod(simulationAccumulator, SIMULATION_STEP_SECONDS);\n        }",
    ),
    (
        "        host.renderer.draw(host.game.state());",
        "        GameState renderState = host.game.state();\n        if (!capturePath) {\n            const float alpha = clampf(static_cast<float>(simulationAccumulator / SIMULATION_STEP_SECONDS), 0.0f, 1.0f);\n            const auto& currentCamera = host.game.state().camera;\n            const bool cameraCut = previousCamera.firstPerson != currentCamera.firstPerson ||\n                lengthSq(previousCamera.pos - currentCamera.pos) > 25.0f ||\n                lengthSq(previousCamera.lookTarget - currentCamera.lookTarget) > 25.0f;\n            renderState.time = std::max(0.0f, currentCamera.firstPerson != previousCamera.firstPerson\n                ? host.game.state().time\n                : host.game.state().time - (1.0f - alpha) * static_cast<float>(SIMULATION_STEP_SECONDS));\n            if (!cameraCut) {\n                renderState.camera.pos = previousCamera.pos + (currentCamera.pos - previousCamera.pos) * alpha;\n                renderState.camera.lookTarget = previousCamera.lookTarget + (currentCamera.lookTarget - previousCamera.lookTarget) * alpha;\n                renderState.camera.forward = normalized(renderState.camera.lookTarget - renderState.camera.pos);\n            }\n        }\n        host.renderer.draw(renderState);",
    ),
]

applied_initial = False
for old, new in initial_replacements:
    if old in text:
        if text.count(old) != 1:
            raise SystemExit(f"ambiguous anchor: {old[:80]!r}")
        text = text.replace(old, new, 1)
        applied_initial = True

incorrect_time = "                renderState.time += alpha * static_cast<float>(SIMULATION_STEP_SECONDS);"
correct_time = "                renderState.time = std::max(0.0f, host.game.state().time - (1.0f - alpha) * static_cast<float>(SIMULATION_STEP_SECONDS));"
if incorrect_time in text:
    text = text.replace(incorrect_time, correct_time, 1)

if "PRESENTATION_STEP_SECONDS" in text:
    raise SystemExit("stale PRESENTATION_STEP_SECONDS reference remains")
required = ["glfwSwapInterval(1)", "SIMULATION_STEP_SECONDS", "GameState renderState"]
for marker in required:
    if marker not in text:
        raise SystemExit(f"missing transformed marker: {marker}")
if text == original:
    print("desktop smoothness transform already applied")
else:
    path.write_text(text, encoding="utf-8")
    print("desktop smoothness transform applied")
