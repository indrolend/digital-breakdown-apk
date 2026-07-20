# Digital Breakdown integrated debug suite

## Source baseline

The debug suite is based on the current Pass 7 checkpoint in `agent/pass7-visual-parity` / PR #15. That checkpoint includes the current shared gameplay, TV room, progression, multiplayer protocol v4, desktop renderer, Android renderer, menus, audio events, particles, grabs, revival, and room economy.

`main` is not the feature baseline until PR #15 is merged.

## One-tool contract

The player/developer launches the normal diagnostic build and uses one in-game suite:

- `F1`: full developer panel
- `F2`: compact profiler
- `F3`: pause simulation
- `F4`: advance one simulation frame
- `F5`: safe/breaktest range toggle
- arrows: navigate and edit
- Enter: execute action or scenario
- Backspace: restore the selected value to its baseline

The same suite owns:

1. runtime timings and workload counters;
2. live state inspection;
3. reversible parameter overrides;
4. stress scenarios;
5. debug visualizations;
6. validation and failure snapshots;
7. debug event and benchmark logging.

The external PowerShell recorder remains optional OS-level evidence. It does not define gameplay state and is not a separate gameplay debug interface.

## Architecture

### Shared, platform-neutral

`native-debug/DebugFeatureManifest.hpp`

Stable feature IDs, categories, value kinds, safe ranges, break ranges, and logging/snapshot policy.

`native-debug/DebugRegistry.*`

Binds each feature ID to exactly one getter/setter/action implementation. The desktop menu, Android developer overlay, command parser, logger, snapshot writer, and scenario runner all consume this registry.

`native-debug/RuntimeDiagnostics.*`

Scoped timers, workload counters, frame history, percentiles, peaks, and CSV output.

`native-debug/DebugValidation.*`

Finite-number checks, state invariants, array/index checks, illegal state combinations, soft assertions, and failure snapshots.

`native-debug/DebugScenarios.*`

Deterministic baseline and stress scenarios using explicit room seeds and exact state setup.

### Platform presentation

`native-desktop/DebugOverlay.*`

Keyboard-first classic developer panel and OpenGL overlay.

`native-android/.../DebugOverlay.*`

Optional developer-build touch presentation. It uses the same registry and scenarios; it does not duplicate gameplay controls.

## Build modes

- **Release**: debug suite excluded.
- **Diagnostic**: profiler, validation, snapshots, and logs; state mutation disabled.
- **Developer**: complete panel, state mutation, visualizers, scenarios, and breaktest ranges.

Suggested CMake definitions:

```cmake
DIGITAL_BREAKDOWN_DIAGNOSTICS
DIGITAL_BREAKDOWN_DEBUG_SUITE
```

## Registration rule for future features

A gameplay feature is not considered development-complete until it provides, where applicable:

1. a stable `FeatureId`;
2. a registry descriptor and getter;
3. a setter or action when safe to manipulate;
4. validation rules;
5. snapshot serialization;
6. workload/timing counters for expensive behavior;
7. at least one deterministic scenario or parity assertion when behavior is complex;
8. multiplayer ownership/synchronization visibility when networked.

This prevents new systems from becoming opaque after they are added.

## Current feature groups

### Player

Battery, stored souls and brute slots, alive/downed/revival state, grab state, movement state, ledge state, secret-room state, position, velocity, and debug overrides.

### Energy and progression

Supplemental energy, flower stacks, combo state, permanent tokens, permanent levels, temporary levels, accuracy, room heat, relay primer, impact guard, last stand, lunge rebound, and regeneration modifiers.

### Vacuum and souls

Vacuum power, field strength, lock strength, selected target, all soul states, capture/ingest state, screen latch, lattice nodes, tethers, visual pull, and network ownership.

### Rooms and goals

Room index, seed, topology, clear state, nine capture points, door transition state, room elapsed time, captures, and room-specific state.

### Enemies and combat

Alive/brute counts, health, armor, attack state, grab state, cooldowns, target player, walk targets, melee state, bullets, pending shot, damage calculations, and respawn requests.

### TV room

Availability, active visit state, signal, damage, tolerance, broken state, donation cooldown, and reward/donation actions.

### Rendering

Graphics preset, shadows, portal window, particles, FPS display, humans, animation, skinning, soul lattice, TV, HUD, draw calls, triangles, visible/animated/skinned targets, and active particles.

### Multiplayer

Connection state, local role, player ownership, snapshot rate, world-update rate, bytes, queue depth, protocol version, desync/state hash, and simulated latency/loss in developer builds.

## Initial scenarios

- baseline room;
- maximum living enemies;
- maximum attacking enemies;
- all enemies slurpable;
- all souls attracted;
- all souls latched/ingesting;
- maximum lattice activity;
- maximum bullets;
- maximum particles;
- TV room idle;
- TV room active and near failure;
- door transition under load;
- player grabbed during transition;
- player downed with stored souls;
- multiplayer host with simulated remote players;
- everything at safe maximum;
- progressive breaktest sweep.

## Logging contract

Each session uses one directory:

```text
playtest-logs/<timestamp>/
  process-metrics.csv
  runtime-diagnostics.csv
  debug-events.csv
  stress-tests.csv
  failure-snapshots.jsonl
  notes.txt
  game-stdout.log
  game-stderr.log
```

Every mutation logs feature ID, old value, new value, source, frame, room, seed, and elapsed time. Every scenario logs its exact setup and result. Failures retain the preceding frame-history ring buffer.
