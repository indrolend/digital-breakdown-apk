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
- `roomSeed` and `roomIndex` are existing deterministic inputs. They now derive one compact `RoomEnvironmentPlan` shared by gameplay and both renderers, so physical obstacles and presentation agree without adding replicated state.
- The first room is a field. Later rooms deterministically select field, city, or sterile premises; occasional recovery rooms are sparse fields with two fewer active enemies.
- Vacuum response uses `VacuumState::power`; shot response uses the already-decaying `EnergyState::dischargePositionAmount`.
- Low graphics uses 40 grass segments; higher presets use 96. Android submits all grass as one `GL_LINES` call per visible tile.

## Derived mappings (ancestry uncertain)

- The supplied `DATA-Windows-Test-8df5e28.zip` is a binary runtime package, not the historical browser source archive. City proportions, fog density, grass counts, response radii, and colors are therefore conservative native interpretations of the described browser behavior.
- Historical grass, sidewalks, and block forms are treated as a vocabulary, not universal decoration. Field rooms own grass and sparse rocks, city rooms own sidewalks and paired block/building obstacles, and sterile rooms own symmetrical geometric cover.
- Desktop uses one bounded positional phone light. GLES uses emissive screen brightening and shader atmosphere because a dynamic-light system is inappropriate for the target low-end hardware.
- Billboarded symbols were deliberately removed from this foundation pass. Existing enemy and soul identity remains intact while room premise and pacing are established first.

## Performance bounds

- City: 10 deterministic authoritative obstacles plus two renderer-only sidewalk strips.
- Grass: field rooms only, bounded to 32/78 typical blades from the 40/96 low/high budgets; one batch per visible tile.
- No allocations in gameplay update, new replicated fields, textures, text rasterizers, dynamic shadows, or unbounded emitters.

## Validation

- The unmodified recovered source passed all 11 registered native behavioral tests.
- `EarlyBrowserVisualsTest`, `Pass7ParityTest`, and `PhoneDisplayStateTest` pass after the port.
- The Windows desktop executable compiles and links. Its archive-only post-build packaging step cannot copy omitted audio/model/TV asset directories; this is a pre-existing source-package limitation.
- The complete Android native library compiles and links for `armeabi-v7a`, Android API 23, with NDK 27.2.12479018. This directly validates the GLES renderer on the relevant 32-bit ARM path.
