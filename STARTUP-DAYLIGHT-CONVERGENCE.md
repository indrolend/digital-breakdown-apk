# Startup Daylight Convergence

## Observed root cause
Exposed rooms initialized `RoomWeatherState::cycleTime` at 0 seconds. On the first gameplay update, the existing 180-second day/night cosine maps phase 0 to **0.5 daylight**, even though `daylight` itself defaults to 1.0. The desktop renderer immediately multiplies sky, ambient, primary, and fill lighting by that half-dawn exposure. This produces the reported startup "dark film" impression without any fullscreen gameplay overlay being involved.

## Change
The existing weather clock now begins at 45 seconds, the authored daylight peak (phase 0.25). Open rooms therefore begin at daylight 1.0 and continue through the same deterministic 180-second cycle from there. Indoor rooms are unchanged.

## Reused authority
Only `RoomWeatherState` and the existing weather-to-lighting composition are used. No brightness correction, overlay special case, renderer pass, light, shader, or startup manager was added.

## Verification
- New `weather_startup_composition_test`: PASS (`cycle=45.0 daylight=1.000`).
- `scene_response_room_pressure_test`: PASS.
- `physical_enemy_body_test`: PASS; 32 enemies / 60 simulated seconds = ~0.222 us per enemy-frame in this environment.
- Strict C++20 warnings-as-errors compilation of `Game.cpp`: PASS.
- Diff whitespace check: PASS.

## Known limitation
Desktop Windows/OpenGL execution is not available in this environment, so the visual fix is source/contract verified rather than visually captured from the shipping renderer.
