# Gameplay Architecture Distillation Audit

## Scope

This audit begins the behavior-preserving gameplay architecture distillation described in the project handoff. It is based on the current checkpoint branch `rescue/mac-working-tree-2026-07-21` and must be updated whenever implementation work reveals a different runtime contract.

The goal is not to redesign the game. The goal is to make the existing game easier to reason about, safer to extend, and less dependent on implicit cross-system mutation.

## Current architectural shape

The native game is presently organized around a large `Game` object and a broad `GameState` containing simulation, presentation, progression, audio, networking, room, and transient state.

The runtime is still a direct bounded simulation, which is appropriate for the project. Fixed-capacity arrays provide predictable memory use, stable indices, and compatibility with older Android hardware. The intended target is therefore not an ECS or framework rewrite. The target is a more explicit structured procedural simulation.

Conceptually, the current game contains these subsystems:

- platform input adapters
- central game/session orchestration
- player movement and traversal
- procedural phone pose and transform generation
- melee and air-lunge combat
- projectile discharge
- human AI, armor, attacks, grabs, and respawn
- exposed-soul and vacuum capture state
- capture points and room completion
- battery, supplemental energy, and survival resolution
- run and permanent progression
- room topology and special-room behavior
- HUD, audio event ring, particles, and camera
- host-authoritative multiplayer and peer snapshots
- desktop and Android rendering/audio adapters

## Authoritative gameplay loop

The intended gameplay dataflow is:

```text
platform input or network input
→ normalized player intent
→ player movement and traversal
→ player abilities
→ combat resolution
→ human/soul lifecycle transitions
→ stored souls and projectiles
→ capture-point and room progression
→ run/permanent progression
→ gameplay feedback
→ presentation and networking
```

The current implementation approximates this flow but allows many functions to mutate unrelated sections of `GameState` directly.

## Current frame-order model

The exact order must be verified against the full current `Game::update` implementation before semantic refactoring. The currently observed order and dependencies indicate this provisional frame model:

1. process start, pause, cinematic, and one-frame input actions
2. process the local player controller and traversal state
3. derive or update phone visual state and world transform
4. resolve pending melee/lunge contact
5. process queued or pending soul shots
6. update vacuum targeting and soul capture
7. commit queued soul captures
8. update enemies, attacks, grabs, armor, and respawns
9. update projectiles, capture-point deposits, pickups, and particles
10. update battery regeneration and run progression timers
11. evaluate room completion, special-room state, and transitions
12. simulate remote players on the host
13. process team revival and multiplayer-shared interactions
14. update camera and presentation-derived state
15. derive HUD values and publish audio/network output

This order is behaviorally significant. Refactors must preserve same-frame precedence, especially for:

- damage versus battery gain
- soul capture versus room-clear evaluation
- projectile deposits versus room transition
- revival versus bleedout
- melee contact versus enemy movement
- upgrade acquisition versus derived-stat use

## Input dataflow

`InputState` currently mixes:

- keyboard-like gameplay flags
- raw touch coordinates
- touch-mapped movement
- look deltas
- held state
- one-frame edge state
- grab-escape input
- communication signals

Network input reconstructs edge-triggered actions from button bitfields and writes them into a structurally similar input state.

Risk: platform and network paths can diverge in how they produce gameplay intent.

Target boundary:

```text
keyboard / mouse / touch / controller / network
→ PlayerCommand
→ player simulation
```

## Player movement dataflow

Player movement currently draws from shared input and mutates player position, velocity, jump state, grounding, ledge state, battery, camera-facing intent, and procedural phone movement inputs.

Movement is mechanically coupled to battery and combat by design. That coupling should remain, but the ownership should become explicit:

```text
PlayerCommand
+ PlayerRuntime
+ collision query access
→ movement result
→ energy transaction if required
→ action/presentation inputs
```

Wall climbing is disabled in the current checkpoint. Remaining wall-climb constants, fields, battery reasons, or helper paths should be removed only after confirming they are not used by ledge traversal, air-lunge wall interaction, camera collision, or ordinary wall response.

## Melee dataflow

Melee combines:

- input edges
- battery cost
- combo sequencing
- procedural phone pose
- dash/lunge motion
- hit detection
- armor and health mutation
- headshot logic
- knockback
- energy gain
- particles, HUD, audio, and camera response

Current tuning is split across a primary melee definition table and parallel pose arrays. The first low-risk refactor should consolidate each variant into one immutable definition while preserving every value and array ordering contract.

## Vacuum and soul-capture dataflow

The soul lifecycle is already partially formalized through `SoulState`:

```text
Free → Attracted → Latched → Ingesting → Recoiling / Revolving
```

`TargetState` combines living-human AI/combat data, soul-capture data, lattice presentation data, tether data, and network ownership.

The main near-term requirement is not to split this aggressively. First document legal phase transitions and which subsystem owns mutation in each phase. Later restructuring can separate human runtime and soul runtime without changing target identity.

## Projectile dataflow

Stored souls become pending shots and then bounded projectile entities. A projectile may:

- hit an enemy
- contribute to armor/health changes
- fill a capture point
- produce a near-miss response
- expire and consume the stored resource

Projectile resolution therefore affects combat, inventory, room progression, audio, HUD, and particles. The update order must keep deposits and room-clear evaluation deterministic.

## Battery and survival dataflow

Battery is the central gameplay resource. It is simultaneously:

- health
- action energy
- movement pressure
- revive currency
- survival threshold
- feedback source

The current spend path can also apply:

- supplemental battery absorption
- progression modifiers
- impact guard
- last stand
- multiplayer downed state
- solo soul reboot
- run death
- HUD ticker output
- audio arming and playback events

This is a strong game-design contract, not a reason to split battery into unrelated meters. The implementation target is a centralized transaction result that separates authoritative state mutation from presentation feedback.

## Room progression dataflow

Room progression depends on:

- capture-point state
- required souls/goals
- active enemies and respawns
- room heat and elapsed time
- door/transition state
- special-room availability
- player and network state

Universal room rules and one-off features such as the secret TV currently share the central game implementation. Future work should isolate special-room behavior behind a lightweight explicit boundary without introducing a scripting framework.

## Permanent and run progression

Progression is already divided into permanent and run structures, but gameplay functions can still calculate modifiers in multiple places.

Target flow:

```text
permanent levels
+ temporary levels
+ run synergies
+ room modifiers
+ active buffs
→ DerivedPlayerStats
→ combat, movement, capture, and energy systems
```

The first implementation should not alter formulas. Representative old/new value tests should precede consolidation.

## Presentation dataflow

Presentation-relevant state currently includes:

- procedural phone pose
- final phone transform and screen basis
- camera
- human animation/reaction fields
- soul lattice and tether visualization
- particles
- HUD values and temporary ticker messages
- audio event ring

The desired boundary is:

```text
authoritative simulation
→ derived presentation state
→ desktop/Android renderer and audio adapter
```

The phone transform evaluator must eventually be shared by local and remote players. Snapshot code should not manually reconstruct a simplified peer transform that differs from the local path.

## Multiplayer dataflow

The host currently reuses single-player update code by saving the local player context, loading a peer context into shared state, simulating the peer, saving it back, and restoring the local context.

This was an effective transitional design, but it creates hidden mutation risk because the shared `GameState` also contains world, room, progression, HUD, audio, and networking data.

Long-term target:

```text
for each active player:
    updatePlayer(PlayerRuntime&, WorldState&, PlayerCommand, dt)
```

The first cleanup pass must not begin this migration. It requires dedicated tests for remote actions, shared-world mutation, HUD/audio isolation, and cooperative revival.

## State-lifetime classification

### Persistent

- permanent progression tokens and levels
- save revision
- user settings and platform preferences
- control, audio, and graphics configuration

### Run

- temporary upgrade levels
- accuracy and synergy stacks
- room index and run-wide modifiers
- run survival/reboot allowances where confirmed
- run scoring/capture totals where present

### Room

- target roster and respawn requests
- capture points
- room seed, topology, colliders, and clear state
- room heat and elapsed time
- door and transition state
- secret-room availability and room-local feature state

### Player runtime

- position, velocity, grounding, and jumps
- battery and stored souls
- ledge, grab, downed, revive, and communication state
- vacuum, melee, pending-shot, and per-player camera intent

### Transient simulation

- input edges
- pending/queued captures
- frame-local target selections
- temporary collision results
- pending damage/hit resolution

### Presentation

- phone pose and final render transform
- camera smoothing
- hit flashes and reactions
- particles
- HUD formatting/tickers
- audio events
- debug visualization

### Network

- authority and connection status
- room code and status text
- local player ID
- peer activity
- input and snapshot sequences
- packet/interpolation state

## Observed cross-system mutations

The following paths require special care because one operation affects multiple conceptual systems:

- battery spend mutates energy, survival, progression effects, HUD, audio, input, vacuum, and run lifecycle
- melee mutates movement, phone pose, battery, targets, combo state, particles, camera, HUD, and audio
- vacuum mutates target ownership, target phase, phone anchors, inventory, battery, HUD, and audio
- projectile deposits mutate inventory, capture points, room-clear state, particles, HUD, and audio
- target attacks mutate player battery/life state, knockback, target cooldowns, multiplayer downed state, and feedback
- room transition resets or preserves fields across room, run, player, presentation, and multiplayer lifetimes
- peer simulation temporarily replaces local player-facing state while still accessing shared world state
- secret-room logic mutates player location, inventory, room-local state, audio, HUD, and multiplayer-visible player state

## Behavior-preservation risks

Highest-risk refactors:

1. changing frame update order
2. replacing peer context swapping before explicit per-player tests exist
3. changing battery mutation and survival precedence
4. changing soul phase transitions or capture commit timing
5. changing phone render/collision/reference-point ownership
6. restructuring reset logic without field-lifetime tests
7. consolidating progression formulas without equivalence tests
8. changing target data layout used by networking or snapshots
9. treating visual animation timers as non-authoritative when they affect hit timing
10. formatting dense code while unintentionally changing chained conditions

## Recommendations already present or partially present

- fixed-capacity arrays and bounded ring buffers are already used and should be retained
- permanent and run progression are already separated structurally
- the soul lifecycle already uses a scoped state enum
- audio already uses an event-like ring-buffer model
- multiplayer is host authoritative
- protocol and parity tests already exist in the checkpoint validation
- wall climbing is already disabled, so the task is removal of dead remnants rather than behavior removal
- phone presentation already has dedicated pose/transform structures, but ownership and local/peer evaluation remain inconsistent

## First implementation sequence

The first code pass should remain mechanical:

1. confirm and remove dead wall-climb-only constants, state, helpers, and battery reasons
2. retain ledge traversal and air-lunge wall interaction
3. replace duplicate stored-soul capacity constants with one canonical constant
4. consolidate melee tuning and pose arrays into one definition table
5. add enum counts and static assertions where table ordering is contractual
6. expand only directly touched dense code
7. run desktop builds, smoke/model tests, parity tests, and multiplayer protocol tests

Do not begin `PlayerRuntime`, lifecycle enum migration, event queues, reset restructuring, or simulation/presentation separation in this pass.

## Planned pull-request sequence

1. **Mechanical gameplay cleanup** — dead wall-climb remnants, canonical capacities, melee definitions, enum/table assertions
2. **State lifetime and reset contracts** — document and test reset behavior before restructuring
3. **Platform-neutral player commands** — adapters for desktop, Android, and network input
4. **Canonical player runtime** — local player first, then network peers, then removal of context swapping
5. **Simulation/presentation boundary** — shared phone evaluator and derived HUD/presentation state
6. **Gameplay events and energy transactions** — bounded events with battery as the first migration
7. **Derived progression statistics** — formula-equivalence tests and centralized reads
8. **Room/encounter separation** — isolate special-room behavior
9. **Protocol and asset resilience** — versioned fixtures, audio metadata, and canonical generated assets

## Validation contract

Every implementation PR must report:

- exact source branch and commit
- files changed
- runtime contracts intentionally preserved
- builds/tests executed
- tests unavailable in the current environment
- remaining risks and next dependency

No performance claim should be made without measurement.
