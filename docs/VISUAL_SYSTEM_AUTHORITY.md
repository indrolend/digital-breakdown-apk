# Visual system authority

This is a compact map of current ownership. It is a coordination aid, not a
request to redesign every system. Simulation owns deterministic visible state;
desktop and Android consume it.

| System | Source authority | Renderer | Trigger/state | Communicates | Decision |
| --- | --- | --- | --- | --- | --- |
| DATA visual state | `GameState::phoneVisual`, `makePhoneVisualState` | Both native renderers | Vacuum/contact/camera mode | Precise tool state and ingestion contact | KEEP |
| Human reaction/damage | `TargetState`, `HumanReactionVisual`, `HumanVisual.hpp` | Skinned human paths in both renderers | Armor, hit direction/flash, vacuum, capture | Direction, damage and loss of stable form | STRENGTHEN |
| Soul visual state | `TargetState::soulVisual` and lattice state | Both native renderers | Soul state, pull, latch and ingestion | Elastic matter becoming capturable data | KEEP |
| Particles | Fixed `ParticleState[256]` pool in `GameState` | Bounded box draws in both renderers | Impact, shell damage, shot/capture/deposit | Event origin, force and material | CONSOLIDATE |
| Grass reaction | `EnvironmentVisualState`, vacuum state, deterministic grass helpers | Both native renderers | Player proximity, vacuum and latest shot | Directional environmental force | KEEP |
| Field/city surfaces | Deterministic room plan and surface helpers | Both native renderers | Room seed/index and setting/form | Generated setting identity | KEEP |
| Room palette/substrate | Room setting plus shared visual constants | Both native renderers | Current deterministic room plan | Matter family of the generated world | EXTEND |
| Environment primitives | `RoomEnvironmentPlan` and semantic placements | Both native renderers | Setting, form, role and inclusion plan | Boundary, mass, landmark, traversal, detail | EXTEND |
| Colliders | `Game::buildRoomColliders()` from bounded shared specs | Debug/presentation consumers | Accepted room geometry | Physical support and obstruction | KEEP |
| Traversal graph | Shared action-labelled graph and calibration | Inspector/debug presentation | Room playstyle and fixtures | Intended movement verbs and difficulty | KEEP |
| Doorway/framebuffer transition | Transition state owned by game | Desktop/Android transition paths | Room transition | Rendered image becoming divisible data | KEEP |
| Secret TV | `SecretTvState` and `TvGifWall` channel policy | Both native renderers | Secret-room signal and time | Real media as an unstable transmission | STRENGTHEN |
| HUD | Shared gameplay/HUD state | Platform renderers | Battery, capture, menu and encounter state | Precise readable status | KEEP |
| Camera feedback | Camera/gameplay state | Platform camera paths | Movement, cinematic and selected impacts | Player orientation and event magnitude | CONSOLIDATE |
| Audio/rumble | `AudioState`/cues and local feedback consumers | Desktop/Android platform paths | Gameplay contact and state thresholds | Timing, magnitude and confirmation | CONSOLIDATE |

## Current particle emitter inventory

| Emitter | Trigger/source | Direction source | Current presentation | Lifetime | Current meaning |
| --- | --- | --- | --- | --- | --- |
| `spawnParticleBurst` | Shot launch, shell hit, headshot reward, ingestion and deposit | Seeded radial/upward random | Red cubes | 0.55–0.90 s | Opaque mixture of flesh, soul and data |
| `spawnFlameBurst` | Melee contact/headshot | Seeded radial/upward force scaled by impact | Red cubes | 0.42–0.74 s | Impact magnitude |
| `spawnShellShatter` | Armor reaches zero | Deterministic body-local radial/upward breakup | Teal cubes; gravity, ground settle | 0.72–1.10 s | Shell fragments/reclaimed matter |

The fixed pool is sufficient. The first semantic cleanup should distinguish
Impact, Flesh, Environment, Soul and Data only at the existing event sites.
Environment fragments should reuse their current gravity/settle behavior and
converge from flesh toward the current room substrate while settling.

## Current material and damage authority

- `roomPlan(roomSeed, roomIndex).setting` is the smallest stable shared answer
  for environmental substrate family.
- Existing setting-specific floor/obstacle colors are renderer presentation;
  combat should consume one small shared substrate helper rather than copy them.
- Armor and `humanShellThinningAmount` already provide deterministic damage
  progression. New surface response should use that same armor fraction.
- `hitDirectionLocal`, hit flash, shell thinning and crit-weighted deformation
  already agree on the contact event. Do not add a parallel damage timeline.
- Particles are not replicated. Their deterministic host/gameplay generation is
  bounded presentation state; no network protocol expansion is required for
  semantic classification.

## Deferred

Large terminal shards, richer support queries, faceted rocks, tree deployment,
wedges/slopes, glyph rendering, and broad camera/audio changes remain separate
review slices. No slope or room-generation authority changes belong in the
semantic particle/material slice.
