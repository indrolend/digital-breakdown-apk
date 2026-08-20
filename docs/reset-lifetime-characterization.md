# Reset and Lifetime Characterization

Base observed: `main@14ba2e530ff6f7d66dde6ef01a6fb75a7688b674`

Status: observed current behavior. This document is descriptive, not permission to change reset semantics.

## Why this exists

Reset code is a cross-cutting ownership boundary. A mechanically attractive rewrite can preserve compilation while silently changing progression, settings, multiplayer membership, presentation state, audio, or room behavior.

Before reset code is decomposed, current behavior must be treated as a contract and covered by focused tests.

## `Game::reset()`

Observed implementation:

```text
capture permanent progression
capture local settings
construct fresh GameState
restore permanent progression
restore local settings
reset local menu-navigation transients
initialize secret-TV tolerance
reset room
mark game started/alive/unpaused
emit invitation audio
refresh phone display
```

### Preserved across `reset()`

- `ProgressionState::permanent`
- persistent `LocalSettingsState` values

### Explicitly not preserved inside `LocalSettingsState`

These fields are treated as session/UI transient state even though they live inside the settings structure:

- `menuPage` → `Main`
- `menuScroll` → `0`
- `menuHistoryDepth` → `0`
- `rebindingAction` → `-1`

This is an important structural smell: one struct contains both persistent configuration and transient menu navigation. Do not split it until preservation behavior is tested.

### Reconstructed/reset by fresh `GameState`

Everything not explicitly restored is replaced by default-constructed state before `resetRoom()` performs room initialization. This currently includes player runtime, run progression, room state, presentation state, audio state, multiplayer runtime, transient gameplay state, and debug state unless subsequently rebuilt.

The broad fresh-state reset is current behavior, not evidence that all those concepts have the same lifetime.

## `Game::restart()`

### Offline / host behavior

`restart()` calls `reset()` and then starts the restart intro presentation.

### Network guest behavior

A guest does not restart itself:

```text
multiplayer enabled
+ not authoritative host
→ restart() returns without mutation
```

Guest run/session authority therefore remains upstream with the host.

### Network host behavior

Before reset, the host captures active peer membership. After reset it:

1. reconfigures host network state;
2. restores each previously active peer through `setNetworkPeerActive()`;
3. starts restart cinematic state.

This means active peer membership is semantically preserved across host restart even though `Game::reset()` itself fresh-constructs `MultiplayerRuntimeState`.

The preservation belongs to the restart/session orchestration contract, not to generic `GameState` persistence.

## `Game::prepareStartScreen()`

Observed layering:

```text
reset()
→ started=false
→ dead=false
→ uiPaused=false
→ HUD gameOver=false
→ clear input
→ replace AudioState with default
→ update phone display
```

Notably, `reset()` emits an invitation audio event, but `prepareStartScreen()` subsequently replaces `AudioState`. That ordering is observable state behavior and should not be casually reordered.

## `Game::prepareAttractScreen()`

Observed layering:

```text
reset()
→ derive showcase random seed
→ replace roomSeed / flower random state
→ resetRoom() again using showcase seed
→ attractMode=true
→ started/alive/unpaused
→ clear cinematic state
→ battery=100
→ randomize showcase player/camera placement
→ clear input
→ update phone display
```

The second `resetRoom()` is intentional with respect to the newly selected showcase seed. Folding it into the initial `reset()` would change attract-room generation.

## Debug entrypoints

Debug/lab entrypoints call `reset()` and then intentionally overwrite slices of state to construct fixtures. These are fixture builders, not canonical examples of ordinary run/room lifecycle ownership.

Examples observed on current main:

- secret-TV test
- traversal lab
- room inspector

Future test/debug access should become explicit rather than encouraging arbitrary production mutation.

## Contract tests required before reset refactoring

### Reset preservation

Given non-default persistent progression and settings, after `reset()` assert:

- permanent tokens/levels/revision preserved exactly;
- persistent settings preserved exactly;
- menu navigation/transient settings reset to their current defaults;
- run progression returns to default;
- player runtime returns to expected fresh-run values;
- multiplayer runtime is not implicitly preserved by generic reset.

### Restart guest authority

Given configured guest state, `restart()` must not mutate the guest-owned run/session state.

### Restart host peer membership

Given configured host state with active peers, after `restart()` assert:

- host authority restored;
- same active peer IDs restored;
- run/gameplay state restarted according to current behavior.

### Start-screen audio precedence

After `prepareStartScreen()`, assert the final `AudioState` matches current start-screen behavior rather than merely asserting that `reset()` emitted a cue earlier in the call chain.

### Attract-room reseed

Assert attract setup rebuilds the room using the showcase seed chosen after the initial reset.

## Ownership implication

Current reset behavior confirms that lifetime does not map one-to-one to struct boundaries:

- `LocalSettingsState` mixes persistent settings and transient menu state;
- multiplayer peer membership survives host restart through orchestration rather than raw state preservation;
- presentation/audio state may be initialized by one lifecycle function and intentionally replaced by a higher-level lifecycle function.

Therefore the next refactor should add characterization tests first. Only then should state structures or reset responsibilities be split.

## Rule

Do not replace current reset logic with a generalized reset framework.

First make each observed preservation/reset rule executable as a test. Then extract one lifetime owner at a time while keeping those tests green.
