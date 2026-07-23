# Digital Breakdown code map

This document identifies the current gameplay ownership boundaries and the intended low-risk extraction path. It is a navigation aid, not permission to redesign mechanics.

## Authoritative behavior

- Browser parity reference: `reference/browser-pass7/index_module.mjs`
- Shared native gameplay: `native-android/app/src/main/cpp/game/`
- Current orchestration and most gameplay behavior: `native-android/app/src/main/cpp/game/Game.cpp`
- Shared state and public game interface: `native-android/app/src/main/cpp/game/Game.hpp`

## Target lifecycle

The same fixed-pool `TargetState` object moves through these semantic roles:

```text
inactive -> active human -> loose soul -> captured/inactive -> respawned human
```

Do not split this into dynamically allocated human and soul entities.

### Current ownership

- Human spawn and respawn: `Game::respawnTarget`, `Game::updateRoomPopulation`
- Human movement and attack simulation: `Game::updateTargets`
- Shell damage and human-to-soul transition: `Game::damageSoulShell`
- Vacuum attraction and ingestion: `Game::updateVacuum`
- Capture transaction: `Game::queueSoulCapture`, `Game::processQueuedSoulCaptures`, `Game::captureSoul`
- Projectile collision: `Game::updateBullets`
- Melee collision: `Game::applyMeleeHits`
- Soul lattice presentation: `Game::updateSoulLattices`

## Stable invariants

- `TargetState::pos` is canonical simulation position.
- Visual hover, spin, squash, morph, and tether deformation are presentation-only.
- `captureSoul()` is the only operation that awards a stored soul and queues human replacement.
- `networkOwnerPlayerId` owns temporary vacuum interaction, not target identity.
- Recoiling souls are loose souls but are temporarily ineligible for vacuum reacquisition.
- Combat systems operate on active humans, not loose souls.

## Intended module boundaries

The migration should be incremental. A module is added only when its behavior can be extracted without changing update order or protocol layout.

```text
native-android/app/src/main/cpp/game/
  Game.cpp                         orchestration and legacy implementations
  Game.hpp                         shared state and public interface
  gameplay/
    TargetRoles.hpp                derived semantic predicates
    SoulMotion.hpp                 small dt-based loose-soul simulation helper
    TargetSimulation.*             eventual human/target update extraction
    VacuumSystem.*                 eventual vacuum extraction
    MeleeSystem.*                  eventual melee extraction
    ProjectileSystem.*             eventual projectile extraction
    CaptureSystem.*                eventual capture transaction extraction
  presentation/
    CrosshairSystem.*              eventual crosshair/aim presentation extraction
    SoulVisuals.*                  eventual soul-only presentation extraction
  world/
    RoomCoordinates.*              eventual room wrapping/tile coordinate helpers
    RoomPopulation.*               eventual spawn/respawn extraction
```

## First extraction sequence

1. Introduce and adopt `gameplay/TargetRoles.hpp`.
2. Introduce and adopt `gameplay/SoulMotion.hpp`.
3. Add focused tests for role classification and Free/Recoiling motion.
4. Extract vacuum behavior only after the first two steps pass parity and protocol tests.
5. Extract combat systems separately; do not combine melee and projectile rewrites.
6. Keep networking and capture transaction structure stable until focused tests exist.

## Focused verification

Run one canonical command before considering a gameplay edit complete:

```bash
bash scripts/verify-gameplay.sh
```

On Windows PowerShell:

```powershell
./scripts/verify-gameplay.ps1
```

The focused suite builds and runs:

- `GameplayRoleAndSoulMotionTest`
- `Pass7ParityTest`
- `MultiplayerProtocolTest`
- `git diff --check`

`.github/workflows/gameplay-checks.yml` runs the same contract for gameplay-related pushes and pull requests.

## Review checklist

For each extraction:

- source behavior was inspected before editing;
- update order is unchanged;
- constants are unchanged unless explicitly requested;
- no protocol-visible state was added or reordered;
- no visual offset became simulation state;
- native build and available parity/protocol tests passed;
- `git diff --check` passed;
- the commit contains one coherent extraction or behavior fix.
