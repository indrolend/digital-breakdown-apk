# Digital Breakdown Current-Main Ownership Contracts

Base audited: `main@14ba2e530ff6f7d66dde6ef01a6fb75a7688b674`

## Purpose

This document is the current architecture contract for behavior-preserving cleanup. The goal is not to redesign the game or introduce a framework. The goal is to make future changes mechanically obvious: each piece of state has one canonical owner, mutation crosses named boundaries, update order remains explicit, and platform/render/network code cannot silently become gameplay authority.

The runtime remains a direct bounded C++ simulation with fixed-capacity state, stable target indices, shared desktop/Android gameplay, and host-authoritative multiplayer.

## Governing rule

For every mutable gameplay value:

```text
one state value
→ one canonical owner
→ named mutation path
→ explicit result/event when other systems must react
```

Broad `GameState&` access is transitional infrastructure, not an ownership model.

## Runtime layers

```text
PLATFORM INPUT
keyboard / mouse / controller / touch / network
        ↓
PlayerCommand
        ↓
────────────────────────────────────
AUTHORITATIVE SIMULATION
session / room / player / targets
combat / vacuum / projectiles
energy / survival / progression
        ↓
GameState
────────────────────────────────────
DERIVED OUTPUT
presentation / HUD / audio events / snapshots
        ↓
PLATFORM ADAPTERS
desktop renderer/audio/network
Android renderer/audio
```

Dependencies should normally flow downward. Presentation and platform adapters may read authoritative state but must not become alternate gameplay authorities.

## State lifetime contract

### Persistent

Canonical owner: progression/settings persistence boundary.

- `ProgressionState::permanent`
- persistent `LocalSettingsState` fields
- save revision / format data

May survive process restart. Must not be reset by room or run transitions.

### Run

Canonical owner: run progression/session orchestration.

- `ProgressionState::run`
- run rules and run-wide modifiers
- run survival allowances confirmed by gameplay behavior

Reset only when the run contract says a new run begins.

### Room

Canonical owner: room/encounter lifecycle.

- `targets`
- respawn requests
- capture points
- room colliders / topology
- room heat / room elapsed
- room clear and door transition state
- room-local secret/special behavior

Room transition code must explicitly preserve or reset fields according to this lifetime.

### Player runtime

Canonical owner: player simulation plus named resource/ability owners.

- position / velocity / grounding / jumps / ledge state
- battery / supplemental energy
- stored souls and pending shots
- vacuum runtime
- melee runtime
- downed / revive / grab / communication state

Network peers must eventually use the same player-runtime contract rather than depending on whole-`GameState` context swapping.

### Transient simulation

Canonical owner: the subsystem performing the current transaction.

- input edges
- queued capture commit
- temporary target selections
- collision query results
- pending damage/hit decisions

Transient state must not accidentally survive reset boundaries.

### Presentation

Canonical owner: presentation derivation after authoritative simulation.

- `PhonePoseState`
- `PhoneTransformState`
- camera presentation
- human reaction/animation presentation
- soul lattice/tether presentation
- particles
- HUD formatting/tickers
- audio event publication
- debug visualization

Presentation may describe gameplay state but must not independently decide gameplay outcomes.

### Network

Canonical owner: multiplayer transport/authority layer.

- connection/room status
- local player identity
- peer activity
- command/snapshot sequence state
- interpolation / prediction correction

Network code may apply explicit authoritative inputs/snapshots through `Game` APIs. It should not acquire generic simulation ownership merely because packets need to update state.

## Current canonical boundaries already present

### Semantic player input

`PlayerCommand` is the canonical semantic input boundary after platform input merging. Keyboard, controller, touch, network input, prediction, replay, and tests should converge on this shape rather than inventing parallel action semantics.

### Target identity and lifecycle

`TargetState` remains pooled and fixed-size. Human/soul role is derived from existing target state. `TargetState::pos` remains canonical simulation position. Human-to-soul conversion occurs in place; do not introduce parallel entity identity.

Use the focused helpers under `game/gameplay/` for role and lifecycle semantics.

### Capture transaction

`captureSoul()` remains the inventory-awarding capture transaction. Capture commitment and inventory award must remain atomic with respect to frame order.

### Multiplayer authority

Multiplayer remains host authoritative. Guests may predict presentation/locomotion but do not decide authoritative outcomes.

### Renderer ownership

Renderers consume simulation/presentation state. Lighting/shadow ownership is now canonical on desktop; renderer state must not feed back into simulation authority.

## Ownership ledger

| State / operation | Canonical owner | Allowed external interaction | Current debt |
|---|---|---|---|
| `player.pos`, velocity, grounding, ledge state | player movement/traversal | semantic command + collision queries | peer simulation still depends on context swapping |
| `player.battery` + supplemental energy | energy/survival transaction | spend/damage/reward request | mutation currently fans into survival, HUD and audio |
| `player.souls`, stored soul metadata | inventory/capture/discharge transactions | capture result / shot request | must keep capture award atomic |
| `targets[]` identity and phase | target lifecycle | combat/vacuum requests | living/soul/presentation fields share one struct by design |
| target damage/armor | combat resolution | damage request/result | feedback side effects remain coupled |
| `captures[]`, deposited progress | room progression | projectile deposit transaction | deposit and room-clear timing must stay deterministic |
| `bullets[]`, pending shots | projectile/discharge owner | shot request/result | touches inventory, combat, room progression and feedback |
| `progression.permanent` | progression | award/purchase API | persistence reads are explicit; derived reads remain distributed |
| `progression.run` | progression/run owner | upgrade/synergy result | modifier formulas may still be read in multiple systems |
| `localSettings` | local settings/persistence | explicit settings API | currently reachable via `networkMutableState()` |
| camera / phone pose / phone transform | presentation | derived from simulation | local/peer derivation must remain consistent |
| `audio.events` | feedback publisher | append event; adapters consume | gameplay should publish intent rather than platform audio calls |
| HUD state | presentation/feedback | derive/publish | should not become gameplay input |
| multiplayer peers / room code / status | multiplayer | named network APIs | broad mutable state escape hatch remains |

## Broad mutable escape hatch

`Game::networkMutableState()` currently returns the entire mutable `GameState`. Current-main searches show consumers in desktop host code, desktop multiplayer, Android bridge code, and tests.

This function is transitional and must not gain new production call sites.

Every existing call site should be classified before migration:

1. **test fixture mutation** — keep behind an explicitly named test/debug access contract;
2. **authoritative network snapshot/application** — replace with named network APIs;
3. **platform settings/lifecycle** — replace with settings/session APIs;
4. **gameplay mutation** — move behind the owning gameplay transaction.

First confirmed ownership mismatch: persistence/platform code restores `LocalSettingsState` through `networkMutableState()`. Settings are not network-owned. The first code migration should introduce a named settings boundary and remove settings restoration from the network mutable escape hatch.

## Transaction contracts to establish

### Energy / survival

Target shape:

```text
EnergyRequest
+ player/energy/progression state
→ EnergyResult
→ authoritative mutation
→ feedback publication
```

The transaction must preserve existing precedence for supplemental absorption, impact guard, last stand, downed state, solo reboot, death, and regeneration locks.

### Capture / inventory

Target shape:

```text
eligible target
+ capture completion
→ CaptureResult
→ target phase commit + inventory award
→ feedback
```

No presentation or networking adapter may award inventory independently.

### Projectile deposit

Target shape:

```text
projectile collision
→ DepositResult
→ capture-point mutation
→ room progression evaluation
→ feedback
```

Preserve same-frame deposit versus room-clear order.

### Room transition

Room transition is the primary lifetime boundary. Before restructuring it, tests must assert exactly which persistent/run/player/room/transient/presentation/network fields survive and which reset.

## Update-order contract

Refactors must preserve current `Game::update` order unless a gameplay change explicitly intends otherwise. Particularly sensitive precedence:

- damage versus battery gain;
- soul capture versus room-clear evaluation;
- projectile deposit versus room transition;
- revival versus bleedout;
- melee contact versus target movement;
- upgrade acquisition versus derived-stat use.

Moving code to another source file does not grant permission to reorder it.

## Extension rule

When adding a feature, answer these questions before editing:

1. What lifetime owns its state: persistent, run, room, player, transient, presentation, or network?
2. Which existing subsystem owns mutation?
3. If no owner is obvious, is the ambiguity itself an ownership defect that should be fixed first?
4. What is the named request/transaction boundary?
5. What is authoritative state versus derived feedback/presentation?
6. Which existing same-frame precedence must remain unchanged?
7. Does the feature affect protocol-visible layout or browser-parity behavior?

A feature should not introduce a new broad mutable path simply because the direct assignment is convenient.

## Migration sequence from current main

1. **Local-settings ownership**
   - add explicit settings replacement/update API;
   - migrate persistence/platform settings writes away from `networkMutableState()`;
   - preserve byte-for-byte save behavior.

2. **State lifetime/reset characterization**
   - add focused tests around reset/restart/room-transition preservation;
   - document actual behavior before moving reset code.

3. **Classify remaining `networkMutableState()` production callers**
   - snapshot application;
   - multiplayer lifecycle;
   - Android bridge lifecycle;
   - remove one category at a time.

4. **Energy transaction boundary**
   - characterize precedence with tests first;
   - separate authoritative result from HUD/audio publication without changing values.

5. **Room/deposit transaction boundary**
   - make projectile deposit and room progression ownership explicit;
   - preserve update order.

6. **Per-player runtime boundary**
   - only after isolation tests cover remote actions/shared-world mutation;
   - migrate peer context swapping toward explicit per-player simulation context.

7. **Simulation → presentation derivation**
   - keep phone/camera/HUD/audio adapters read-oriented;
   - share local/remote presentation evaluators where behavior is intended to match.

## Validation contract for every ownership PR

Every PR must report:

- exact base commit;
- exact files changed;
- ownership contract being narrowed;
- observable behavior intentionally preserved;
- tests/builds executed;
- tests unavailable in the environment;
- remaining broad mutation paths;
- next dependency.

Do not combine unrelated ownership migrations in one PR. A small contract with strong evidence is preferable to a large cleanup diff.
