# Digital Breakdown Ownership Contracts

Current hardening line: `agent/ownership-contracts-settings`
Base lineage: `main@14ba2e530ff6f7d66dde6ef01a6fb75a7688b674`

This is the current architecture contract for behavior-preserving work. Historical reasoning lives in commits, tests, and archival issue #50; this file represents the coherent current state.

## Governing rule

```text
one mutable value
→ one canonical owner
→ named mutation path
→ explicit semantic result/event when another system must react
```

Broad mutable `GameState&` access is transitional/internal infrastructure, not an ownership model.

## Runtime direction

```text
PLATFORM INPUT
keyboard / controller / touch / network
        ↓
PlayerCommand + per-player capabilities
        ↓
AUTHORITATIVE SIMULATION
session / room / player / targets / combat / vacuum /
projectiles / energy-survival / progression
        ↓
GameState
        ↓
DERIVED OUTPUT
presentation / HUD / audio events / network snapshots
        ↓
PLATFORM ADAPTERS
Desktop / Android
```

Presentation and platform adapters may describe or request authoritative state changes; they must not become alternate gameplay authorities.

## Lifetime owners

### Persistent

- `ProgressionState::permanent`;
- persisted audio/graphics/input preferences;
- save-format/revision data.

`Game::applyLocalPreferences()` updates persisted preferences without replacing menu/session state. `Game::setMobileFraming()` is separate because mobile framing/action assist affects player behavior.

### Run

- `ProgressionState::run`;
- run rules/modifiers;
- run-wide progression/survival timers.

### Room

- targets/respawn requests;
- capture points and deposited progress;
- colliders/topology;
- room heat/elapsed/captures;
- room-clear/door-transition state;
- room-local secret behavior.

### Player runtime

- movement/traversal;
- battery/supplemental energy;
- stored souls/pending shots;
- vacuum/melee runtime;
- downed/revive/grab/communication state.

Remote authoritative simulation still swaps bounded player contexts internally. Keep peer-isolation tests green until that mechanism earns a narrower replacement.

### Presentation

- phone pose/transform;
- camera presentation;
- human/soul visual state;
- particles;
- HUD formatting/tickers;
- audio event publication;
- debug visualization.

Presentation may react to an authoritative result but must not decide it.

### Network

- room/connection status;
- local/peer identity and activity;
- input/snapshot sequences;
- prediction correction;
- per-player command capability bits.

## Named boundaries already established

### Semantic input

`PlayerCommand` is the semantic input boundary after platform merging. One-shot remote jump/melee/shoot/camera-toggle edges latch until host simulation consumes them; later packets in the same host frame cannot erase them.

### Per-player mobile assist

Protocol v8 carries `PlayerCommand::capabilities`. `CommandMobileFraming` is player-specific. Android hosting must not silently apply Android action assist to desktop guests.

### Local preferences

`Game::applyLocalPreferences()` owns persisted preference writes. It does not replace menu page/history/scroll, rebinding-session state, or `mobileFraming`.

### Authoritative snapshots

Platform adapters apply snapshots through:

```cpp
dbnet::applyWorld(Game&, const WorldSnapshot&, localPlayerId)
```

`MultiplayerProtocol.cpp` is the explicit internal snapshot mutation owner. Desktop multiplayer and the Android bridge no longer receive generic mutable `GameState` authority for world replacement.

### Capture inventory

`captureSoul()` owns inventory-awarding soul capture. Human/soul identity remains in the existing fixed target pool; do not create parallel entity identity merely to make the lifecycle look cleaner.

### Cleared-room advancement

`updateRoomTopology()` detects a legal cleared-room crossing. `advanceClearedRoom()` owns the transition policy:

- door-transition publication;
- room index/seed evolution;
- run-rule advancement;
- +18 next-room battery reward;
- room clear/deposit reset;
- room-scoped solo reboot reset;
- stored-soul clear;
- room counters reset;
- vacuum/melee/discharge reset;
- capture/bullet/pending-shot/flower/respawn reconstruction;
- room geometry/target rebuild;
- upgrade-menu activation, pause, and input clear.

The 256-room probe locks this behavior.

### Energy / survival

Authoritative precedence is now proven through real gameplay paths:

```text
request
→ hit mitigation
→ stored-soul efficiency
→ supplemental absorption
→ main battery
→ last stand
→ multiplayer downed OR solo soul reboot OR run death
```

HUD ticker/battery HUD/audio are derived/event publication, not alternate energy authority. See `docs/energy-survival-transaction-characterization.md`.

### Projectile deposit / room clear

The existing 256-room `RoomProgressionProbe` now behavior-locks the externally meaningful transaction outputs:

- every valid projectile deposit fills exactly one goal;
- each goal awards exactly one permanent token and one progression revision;
- each deposit emits exactly one capture-slot cue;
- the final required deposit transitions the room clear state and emits exactly one `PaymentSuccess` cue;
- an additional frame cannot duplicate token, revision, capture cue, or room-clear cue.

Current implementation remains split across adjacent `updateBullets()` and `updateCaptures()` calls. A future named deposit result/transaction may consolidate ownership only if it preserves this same-frame contract.

## Ownership ledger

| State / operation | Canonical owner | Current debt |
|---|---|---|
| player movement / ledges | player movement/traversal | remote simulation still swaps player contexts |
| battery + supplemental energy | energy/survival | mutation and feedback publication still live in one function |
| stored souls | inventory/capture/discharge | shared by capture and projectile discharge |
| target identity/lifecycle | target lifecycle | living/soul/visual fields intentionally share one bounded pool |
| target damage/armor | combat | feedback remains directly published from transactions |
| capture points/deposits | room progression/projectile deposit | implementation split between bullet collision and room-clear evaluation |
| cleared-room transition | room progression | named and behavior-locked |
| bullets/pending shots | projectile/discharge | touches inventory/combat/room progression |
| permanent progression | progression | derived reads remain distributed |
| run progression/rules | progression/run | modifiers read by several systems |
| local preferences | persistence/local preference | menu/session fields still share the aggregate struct |
| mobile assist | per-player capability | field physically remains in `LocalSettingsState` |
| camera/phone pose | presentation | local/peer derivation must stay consistent |
| HUD/audio | derived feedback | several authoritative transactions publish directly |
| multiplayer snapshot state | protocol/network | protocol implementation retains one explicit internal mutable handle |
| desktop menu/session + fixtures | desktop UI/test harness | broad mutable access remains concentrated in `main.cpp` |

## Broad mutable-state ratchet

`Game::networkMutableState()` must not spread or silently regrow.

`tools/check_ownership_boundaries.py` scans desktop, native network, and Android native production code. The current exact debt budget is:

```text
native-desktop/main.cpp                                27
native-network/MultiplayerProtocol.cpp                  1
native-android/app/src/main/cpp/game/Game.hpp           1
```

The check fails if:

- a new production file acquires the token;
- an existing owner exceeds its budget;
- an existing owner shrinks without lowering the budget in the same change.

That last rule makes cleanup monotonic: once debt is removed, the old allowance cannot quietly return.

`main.cpp` debt is not homogeneous. It mixes real menu/session mutation with deliberate capture/demo/stress/parity fixtures. Classify those responsibilities before replacing anything; do not hide them behind a differently named broad mutable handle.

## Update-order contract

Refactors preserve `Game::update()` order unless a gameplay change explicitly intends otherwise. Sensitive same-frame relationships include:

- damage versus energy outcomes;
- projectile deposit versus room-clear evaluation;
- capture versus inventory award;
- revival versus bleedout;
- melee contact versus target movement;
- upgrade acquisition versus derived-stat use.

Moving code into a named function does not grant permission to reorder it.

## Completed hardening on this line

- reset/restart/room lifetime characterization;
- exact mutable-state production ratchet;
- remote one-shot edge latching;
- per-player mobile assist + protocol v8;
- persisted local-preference boundary;
- authoritative snapshot boundary;
- cleared-room advancement transaction;
- 256-room progression behavior lock;
- focused energy/survival precedence coverage;
- deposit/token/revision/cue idempotence coverage.

## Next evidence-first choices

Do not reopen completed seams unless new evidence perturbs their contract.

Highest-value remaining choices are:

1. classify the 27 desktop mutable calls into real UI/session authority versus deliberate fixture authority, then shrink one category without replacing it with another broad handle;
2. if an actual caller/ownership problem justifies it, extract an energy result that separates authoritative mutation from derived feedback while preserving proven precedence;
3. if a concrete maintenance problem justifies it, consolidate projectile deposit + room-clear mutation behind a named transaction while preserving the 256-room output contract;
4. keep whole-player remote context swapping until peer-isolation evidence identifies a specific defect or simplification target.

A green characterization test is evidence, not an instruction to refactor immediately.
