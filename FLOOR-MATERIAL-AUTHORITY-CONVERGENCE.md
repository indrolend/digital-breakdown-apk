# Floor Material Authority Convergence

## Purpose
Converge the existing weather/goal-driven floor presentation into the existing material-response authority instead of leaving wetness and progression math private to `DesktopRenderer`.

## What changed
- Added `FloorMaterialResponse` / `floorMaterialResponse` to `MaterialResponse.hpp`.
- Existing room setting, goal progress, exposure, and weather wetness now deterministically produce floor color, wetness, specular amount, and shininess in one bounded contract.
- `DesktopRenderer` consumes that contract; numerical presentation is intentionally preserved.
- Extended `material_response_test` to verify wet exposed floors darken/gain restrained specular response, sheltered floors ignore outdoor wetness, and progression still changes the floor material.

## Authorities reused
`RoomWeatherState`, room setting, `SceneResponse.goalProgress`, `MaterialResponse`, graphics preset gating, and the existing fixed-function material path.

## Deliberately not added
No wet-surface manager, reflection pass, screen-space reflection, shader, texture set, puddle simulation, dynamic light, particle system, allocation, or draw call.

## Verification
- `material_response_test`: pass.
- `weather_startup_composition_test`: pass.
- `scene_response_room_pressure_test`: pass.
- `early_browser_visuals_test`: pass.
- Strict C++20 `Game.cpp` compile with `-Wall -Wextra -Wpedantic -Werror`: pass.
- `git diff --check`: pass.

## Limitation
Desktop OpenGL runtime presentation is not executable in this environment, so the preserved renderer values are contract/source verified rather than visually captured.
