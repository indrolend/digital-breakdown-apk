# Room Pressure Lighting Authority Convergence

## Purpose
Converge the remaining room-history lighting transform into the same testable render-contract boundary used by weather lighting.

## Observed state
`sceneResponse()` already owned the bounded `roomPressure` fact, but `DesktopRenderer::draw()` privately repeated the pressure-to-ambient/fill/key/fog transformation. This left the relationship outside the contract tests even after weather lighting had been converged.

## Change
Added `roomPressureLightingProfile(RoomLightingProfile, float)` to `RenderContracts.hpp` and made `DesktopRenderer` consume it. The existing numerical presentation is unchanged: pressure slightly reduces ambient/fill, strengthens the primary light, and thickens existing fog. Clearing a room still immediately releases most pressure through `sceneResponse()`.

Authority is now:
`room history -> SceneResponse.roomPressure -> roomPressureLightingProfile -> DesktopRenderer`.

## Deliberately not added
No new light, fog system, post-process, manager, state, allocation, draw call, shader, particle effect, or graphics-preset exception.

## Verification
- `scene_response_room_pressure_test`: PASS; now verifies the actual lighting transform and cleared-room release.
- `weather_startup_composition_test`: PASS.
- `render_contracts_test`: PASS.
- `physical_enemy_body_test`: PASS; 32 enemies / 60 simulated seconds ~= 0.192 us per enemy-frame in this environment.
- Strict C++20 `Game.cpp` compile with `-Wall -Wextra -Wpedantic -Werror`: PASS.
- `git diff --check`: PASS.

## Limitation
Desktop OpenGL/Windows runtime presentation was not executed in this environment.
