# Early-browser visual port

## Current native ownership observed before the port

- Room generation and collision: `Game::resetRoom`, `Game::buildRoomColliders`, `Game::updateRoomTopology`, `RoomCollider`, and `RoomTopologyState` in `game/Game.cpp` and `game/Game.hpp`.
- Desktop rendering: `DesktopRenderer::draw`, `drawRoomTile`, `drawHumanModel`, `drawSoulFlesh`, and `drawHud`.
- Android GLES rendering: `Renderer::draw`, `drawRoomTile`, `drawHumanModel`, `drawSoulFlesh`, and `drawHud`.
- Phone pose: `Game::updatePhoneGait`, `updatePhoneActionPose`, and `updatePhoneTransform`.
- Vacuum, shooting, and battery: `Game::updateVacuum`, `shootStoredSoul`, `processPendingShots`, and `updateBattery`; exposed through `VacuumState`, `EnergyState`, and `PlayerState`.
- Target and soul visuals: `TargetState`, `SoulVisualState`, `syncSoulVisual`, `makeSoulVisualState`, `drawSoulFlesh`, and the existing translucent soul box pass.
- Glyphs: `bitmapGlyph` in `BitmapFont.hpp`, consumed by both native HUD renderers.
- Hit particles: the existing fixed `PARTICLE_COUNT` 256-slot ring pool, `spawnParticleBurst`, `spawnShellShatter`, and `updateParticles`.
- Tests: `native/tests`, registered by `native-desktop/CMakeLists.txt`.

## Observed mappings

- The native runtime already had layered sun/fill/ambient lighting, translucent soul cubes, bitmap glyph rendering, pooled impact particles, and movement/action-driven phone pose. These were extended or retained rather than replaced.
- `roomSeed` and tile index are existing deterministic inputs. Visual city and grass layouts derive only from these values and never enter gameplay or multiplayer state.
- Vacuum response uses `VacuumState::power`; shot response uses the already-decaying `EnergyState::dischargePositionAmount`.
- Low graphics uses 40 grass segments; higher presets use 96. Android submits all grass as one `GL_LINES` call per visible tile.

## Derived mappings (ancestry uncertain)

- The supplied `DATA-Windows-Test-8df5e28.zip` is a binary runtime package, not the historical browser source archive. City proportions, fog density, grass counts, response radii, and colors are therefore conservative native interpretations of the described browser behavior.
- Buildings are renderer-only silhouettes beyond the room sidewalks. Making them gameplay colliders would change current authority and was intentionally avoided.
- Desktop uses one bounded positional phone light. GLES uses emissive screen brightening and shader atmosphere because a dynamic-light system is inappropriate for the target low-end hardware.
- `soulSymbol` provides stable base-36 identity through the existing bitmap-compatible character set. Both renderers project that stable glyph onto each visible soul cube using their existing bitmap HUD paths; no text textures or runtime font system were added.

## Performance bounds

- City: 10 deterministic primitives per visible tile, including two sidewalks.
- Grass: 40 or 96 blades; one batch on GLES and one immediate line batch on desktop.
- No allocations in gameplay update, new replicated fields, textures, text rasterizers, dynamic shadows, or unbounded emitters.

## Validation

- The unmodified recovered source passed all 11 registered native behavioral tests.
- `EarlyBrowserVisualsTest`, `Pass7ParityTest`, and `PhoneDisplayStateTest` pass after the port.
- The Windows desktop executable compiles and links. Its archive-only post-build packaging step cannot copy omitted audio/model/TV asset directories; this is a pre-existing source-package limitation.
- The complete Android native library compiles and links for `armeabi-v7a`, Android API 23, with NDK 27.2.12479018. This directly validates the GLES renderer on the relevant 32-bit ARM path.
