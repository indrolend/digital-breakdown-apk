# Digital Breakdown Ownership Contracts

Base audited: `main@14ba2e530ff6f7d66dde6ef01a6fb75a7688b674`

Current hardening line: `agent/ownership-contracts-settings`

## Purpose

This document is the architecture contract for behavior-preserving cleanup. The goal is not to redesign the game or introduce a framework. The goal is to make future changes mechanically obvious: each piece of state has one canonical owner, mutation crosses named boundaries, update order remains explicit, and platform/render/network code cannot silently become gameplay authority.

The runtime remains a direct bounded C++ simulation with fixed-capacity state, stable target indices, shared desktop/Android gameplay, and host-authoritative multiplayer.

## Governing rule

For every mutable gameplay value:

```text
one state value
→ one canonical owner
→ named mutation path
→ explicit result/event when other systems must react
```

Broad `GameState&` access is transitional/internal infrastructure, not an ownership model.

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

Canonical owner: progression and local-preference persistence boundaries.

- `ProgressionState::permanent`
- persisted audio/graphics/input preference fields
- save revision / format data

`Game::applyLocalPreferences()` may update persisted local preferences without replacing menu/session state. `Game::setMobileFraming()` is separate because mobile framing/action assist participates in player behavior and multiplayer capability negotiation.

Persistent state may survive process restart and must not be reset by room or run transitions.

### Run

Canonical owner: run progression/session orchestration.

- `ProgressionState::run`
- run rules and run-wide modifiers
- run survival allowances confirmed by gameplay behavior

Reset only when the run contract says a new run begins.

### Room

Canonical owner: room/encounter lifecycle.

- targets and respawn requests
- capture points
- room colliders / topology
- room heat / elapsed time / captures
- room-clear and door-transition state
- room-local secret/special behavior

Crossing the forward door after clearing a room is detected by `updateRoomTopology()` and committed by the named `advanceClearedRoom()` transaction. Detection and transition policy are intentionally separate.

### Player runtime

Canonical owner: player simulation plus named resource/ability owners.

- position / velocity / grounding / jumps / ledge state
- battery / supplemental energy
- stored souls and pending shots
- vacuum runtime
- melee runtime
- downed / revive / grab / communication state

Remote authoritative simulation still uses player-context swapping internally. Isolation tests must remain green until that mechanism is replaced by a narrower per-player runtime context.

### Transient simulation

Canonical owner: the subsystem performing the current transaction.

- input edges
- queued capture commit
- temporary target selections
- collision query results
- pending damage/hit decisions

Remote one-shot command edges are latched until host simulation consumes them. A later network command arriving in the same host frame must not erase an unconsumed jump/melee/shoot/camera-toggle edge.

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
- per-player command capability bits

Multiplayer protocol version 8 carries `PlayerCommand::capabilities`. `CommandMobileFraming` is a per-player capability: an Android host must not apply its own mobile action assist to a desktop guest.

Platform networking adapters request authoritative snapshot application through `dbnet::applyWorld(Game&, ...)`. They do not receive generic mutable `GameState` access for snapshot replacement.

## Current named boundaries

### Semantic player input

`PlayerCommand` is the canonical semantic input boundary after platform input merging. Keyboard, controller, touch, network input, prediction, replay, and tests should converge on this shape rather than inventing parallel action semantics.

The command contains movement/look values, action buttons, and per-player capabilities. One-shot edges remain pending until simulation consumes them.

### Local preferences

`Game::applyLocalPreferences()` owns persisted audio/graphics/input preference application. It intentionally does not replace:

- menu page / scroll / history;
- rebinding-session state;
- `mobileFraming`.

`Game::setMobileFraming()` is the explicit local platform/player capability boundary.

### Authoritative snapshots

`dbnet::applyWorld(Game&, const WorldSnapshot&, localPlayerId)` is the platform-facing authoritative snapshot transaction.

The protocol implementation owns the internal mutable-state application. Desktop and Android network adapters read state before/after the transaction for metrics or presentation, but do not acquire generic mutation authority.

### Target identity and lifecycle

`TargetState` remains pooled and fixed-size. Human/soul role is derived from existing target state. `TargetState::pos` remains canonical simulation position. Human-to-soul conversion occurs in place; do not introduce parallel entity identity.

Use focused helpers under `game/gameplay/` for role and lifecycle semantics.

### Capture transaction

`captureSoul()` remains the inventory-awarding capture transaction. Capture commitment and inventory award must remain atomic with respect to frame order.

### Cleared-room advancement

`updateRoomTopology()` owns detection of a legal door crossing.

`advanceClearedRoom()` owns the current cleared-room transition policy, preserving the existing order for:

- door-transition publication;
- room index and deterministic seed evolution;
- run-rule advancement;
- room-clear reset and +18 next-room battery reward;
- solo soul-reboot allowance reset;
- stored-soul inventory clear;
- required/deposited soul reset;
- room heat/elapsed/capture counters;
- vacuum, melee, and discharge runtime clearing;
- capture-point reconstruction;
- bullets, pending shots, flowers, respawn queue, colliders, and target population;
- upgrade-menu activation, pause, and input clear.

The 256-room progression probe behavior-locks this transition. Moving the policy into the named function does not authorize reordering or changing any value.

### Multiplayer authority

Multiplayer remains host authoritative. Guests may predict presentation/locomotion but do not decide authoritative outcomes.

### Renderer ownership

Renderers consume simulation/presentation state. Lighting/shadow ownership is canonical on desktop; renderer state must not feed back into simulation authority.

## Ownership ledger

| State / operation | Canonical owner | Allowed external interaction | Current debt |
|---|---|---|---|
| player movement / ledge state | player movement/traversal | semantic command + collision queries | remote simulation still swaps player contexts internally |
| battery + supplemental energy | energy/survival transaction | spend/damage/reward request | survival branches and feedback remain coupled |
| stored souls | inventory/capture/discharge | capture result / shot request | capture award must remain atomic |
| `targets[]` identity and phase | target lifecycle | combat/vacuum requests | living/soul/presentation fields intentionally share one pooled struct |
| target damage / armor | combat resolution | damage request/result | feedback side effects remain coupled |
| `captures[]` / deposited progress | room progression | projectile deposit transaction | deposit and room-clear timing must remain deterministic |
| cleared-room transition | room progression | `advanceClearedRoom()` | policy is named; values/order remain coupled by design |
| bullets / pending shots | projectile/discharge | shot request/result | touches inventory, combat, room progression and feedback |
| permanent progression | progression | award/purchase API | derived reads remain distributed |
| run progression / rules | progression/run owner | upgrade/synergy result | modifier formulas remain read by several systems |
| persisted local preferences | local preference owner | `applyLocalPreferences()` | menu/session state still shares the aggregate struct |
| mobile framing/action assist | local player capability | `setMobileFraming()` + command capability | field still physically resides in `LocalSettingsState` |
| camera / phone pose / transform | presentation | derived from simulation | local/peer derivation must remain consistent |
| audio events | feedback publisher | append event; adapters consume | energy/combat transactions still publish feedback directly |
| HUD | presentation/feedback | derive/publish | desktop menu code still mutates some HUD/session fields directly |
| multiplayer peers / room/status | multiplayer | named network APIs | protocol owns snapshot mutation internally |

## Broad mutable escape hatch

`Game::networkMutableState()` still returns the entire mutable `GameState`. It must not spread.

`tools/check_ownership_boundaries.py` scans production source under:

- `native-desktop/`;
- `native-network/`;
- `native-android/app/src/main/cpp/`.

The current production allowance is intentionally small and explicit:

- `native-desktop/main.cpp` — transitional desktop UI/session mutation and built-in stress fixtures;
- `native-network/MultiplayerProtocol.cpp` — named authoritative snapshot transaction implementation;
- `Game.hpp` — declaration of the escape hatch itself.

Desktop multiplayer and the Android bridge are no longer allowed generic mutable-state access.

Test fixtures are outside this production ratchet and should be migrated only when doing so improves the test contract rather than hiding useful state setup.

## Transaction contracts still to establish

### Energy / survival

Current `spendBattery()` behavior is characterized but the major survival branches are not yet independently proven by focused tests.

Required order includes:

```text
request
→ hit mitigation
→ stored-soul drain multiplier
→ supplemental absorption
→ main battery subtraction
→ feedback/audio
→ last stand
→ multiplayer downed OR solo soul reboot OR run death
```

Before extracting an `EnergyResult` or separating feedback, add focused tests for supplemental-first behavior, mitigation, last stand, downed precedence, solo reboot, and non-hit exhaustion.

### Projectile deposit

Target shape:

```text
projectile collision
→ DepositResult
→ capture-point mutation
→ room-clear evaluation
→ feedback
```

Preserve same-frame deposit versus room-clear order.

### Per-player simulation runtime

Only after current peer-isolation tests cover the relevant behavior should remote simulation move away from whole-player context swapping toward an explicit per-player runtime context.

### Desktop menu/session mutation

`native-desktop/main.cpp` remains the largest production user of broad mutable state. Its uses mix legitimate UI/session behavior with built-in test/stress setup, so this must be classified before shrinking the allowance. Do not replace it with a differently named broad mutable handle.

## Update-order contract

Refactors must preserve current `Game::update` order unless a gameplay change explicitly intends otherwise. Particularly sensitive precedence:

- damage versus battery gain;
- soul capture versus room-clear evaluation;
- projectile deposit versus room transition;
- revival versus bleedout;
- melee contact versus target movement;
- upgrade acquisition versus derived-stat use.

Moving code to another function or source file does not grant permission to reorder it.

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

## Completed hardening on this line

- reset/restart/room lifetime characterization;
- production mutable-state ratchet;
- remote one-shot command-edge latching;
- per-player mobile framing/action-assist capability and protocol v8;
- persisted local-preference mutation boundary;
- authoritative snapshot application boundary;
- cleared-room advancement transaction extraction;
- 256-room progression behavior lock.

## Next evidence-first sequence

1. Add focused energy/survival precedence tests through the least brittle authoritative hit path.
2. Only then decide whether `spendBattery()` should expose an explicit result transaction.
3. Characterize projectile-deposit ownership and same-frame room-clear behavior before extracting it.
4. Classify desktop `main.cpp` mutable access into UI/session versus test-fixture responsibilities.
5. Keep per-player context swapping until isolation evidence makes a narrower runtime boundary safe.

## Validation contract

Every ownership change must report:

- exact base/head commit;
- ownership contract being narrowed;
- observable behavior intentionally changed or preserved;
- tests/builds executed;
- unavailable checks, if any;
- remaining broad mutation paths;
- next dependency.

A small contract with strong evidence is preferable to a broad cleanup diff.