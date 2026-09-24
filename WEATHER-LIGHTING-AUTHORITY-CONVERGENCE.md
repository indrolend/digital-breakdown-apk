# Weather Lighting Authority Convergence

Purpose: make the startup/day-night fix test the same transformation the desktop renderer actually consumes, rather than only retesting the weather clock formula.

## Changed
- Moved the existing exposed-room daylight/rain modulation from `DesktopRenderer::draw` into `render_contract::weatherLightingProfile`.
- `DesktopRenderer` now consumes that contract directly; numerical lighting behavior is intentionally unchanged.
- Extended `weather_startup_composition_test` to prove peak startup daylight preserves authored sky/ambient/key exposure and that night/rain still dim the key and increase fog density.

## Authorities reused
`RoomWeatherState -> roomLightingProfile -> weatherLightingProfile -> DesktopRenderer`. No second weather state or lighting pipeline was introduced.

## Deliberately not added
No post process, brightness workaround, new light, shader, weather manager, allocation, draw call, particle, or graphics-preset exception.

## Verification
- `weather_startup_composition_test`: PASS (`cycle=45.0 daylight=1.000`)
- `render_contracts_test`: PASS
- strict C++20 `native/game/Game.cpp` compile with `-Wall -Wextra -Wpedantic -Werror`: PASS
- `git diff --check`: PASS
- Desktop renderer syntax compilation attempted, but this environment lacks `GL/gl.h`; Windows/OpenGL runtime remains unverified.
