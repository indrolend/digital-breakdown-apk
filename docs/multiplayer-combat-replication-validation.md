# Multiplayer combat replication validation

Validated on branch `agent/windows-multiplayer-visual-pause-fix` using the local
Cloudflare Worker emulator. No Worker deployment, release, merge, or Android
multiplayer expansion was performed.

## Candidate

- Protocol: 7
- Gameplay version: 5
- Snapshot size: 8036 bytes
- Transport: existing Cloudflare WebSocket path
- Authority: host resolves player actions and enemy damage; guests consume
  durable snapshots and host-authored presentation events

## Validation

All commands exited successfully:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\verify-gameplay.ps1
cmake --build .\build\windows-release --config Release --parallel
ctest --test-dir .\build\windows-release -C Release --output-on-failure

Push-Location .\multiplayer-server
npm.cmd run protocol:check
npm.cmd test
Pop-Location
```

Results:

- Gameplay verification: passed
- Windows Release build: passed
- CTest: 9/9 passed
- Worker protocol consistency: protocol 7, gameplay 5
- Worker tests: 7/7 passed

The combat parity harness passed its original movement, jump, visual-state,
pause/resume, and host-departure checks plus the scripted guest melee path:

| Profile | Latency | Jitter | Deterministic loss | Result | Hash matches / transient mismatches |
| --- | ---: | ---: | --- | --- | ---: |
| Baseline | 0 ms | 0 ms | none | passed | 40 / 11 |
| Good | 40 ms | 10 ms | none | passed | 34 / 64 |
| Moderate | 100 ms | 25 ms | every 100th input and snapshot | passed | 38 / 75 |
| Poor recoverable | 180 ms | 50 ms | every 33rd input and snapshot | passed | 38 / 82 |

Every profile produced:

```text
MULTIPLAYER_COMBAT_PARITY_OK room=<code> action=confirmed enemy=0
```

The structured metrics checkpoint recorded one predicted and one confirmed
action in every profile, with no corrected or cancelled action in this fixture.
It also recorded six authoritative events with no duplicate or stale event
accepted. Temporary hash disagreement increased with impairment, while repeated
matching hashes and the harness convergence checks proved recovery.

The reported maximum position correction (approximately 14 units) includes the
test-only deterministic teleport that places the guest beside the combat
fixture. It must not be interpreted as a normal-play reconciliation bound.
Maximum action-phase correction remained at or below 0.8333.

## Invariants covered

- Input, snapshot, and gameplay-event packets carry session/run/room identity.
- Incompatible and older world contexts are rejected before world mutation.
- Room-generation rollover resets snapshot interpolation and event tracking.
- Semantic locomotion, action, phase, sequence, target, and progress are
  serialized rather than renderer transforms or bone state.
- Guest action startup is predicted, but enemy damage is host-only.
- Host-authored action, contact, hit, shell-break, and soul-emergence events are
  presentation signals; durable outcomes remain in snapshots.
- Duplicate events are suppressed and old-world events are rejected.
- Missing events cannot permanently diverge gameplay state.

## Known limitations

- This milestone validates melee/shell-break replication only. Vacuum capture,
  discharge projectiles, room progression, reconnect, late join, and host
  migration remain follow-on work.
- The deterministic impairment layer drops input and snapshot packets, not
  gameplay-event packets. Duplicate and stale-event behavior is covered by
  focused native protocol tests.
- Android multiplayer behavior was not expanded; Android changes only preserve
  shared protocol/build compatibility.
