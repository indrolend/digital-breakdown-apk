# Zombie V1 convergence evidence

## Source selection

- Repository: `indrolend/digital-breakdown-apk`
- Starting branch/commit: local `codex/extreme-enemy-lab` at `5b27ad1b2cc7188dfe7e11fa58335facb76a9fbc`
- Convergence branch: `convergence/zombie-v1`
- Latest relevant fetched remote ancestor: `origin/codex/specification-convergence-pass3` at `1a17e257b8a95cec68eb8ef5ec0d43128cf1c489`
- Historical simple-pursuit control: `0862a4cacd35d5e568b0618bca4d4c9421cdd4f1`

`5b27ad1` was selected because it contains the latest specification convergence, persistent world-space foot plants, support/load transfer, physical balance/fall/recovery, the support-driven diagnostic, and tangent-preserving collision response. `main`, `0862a4c`, and the named historical physical branches are older or ancestors of this combined lineage.

## Authority trace

1. Target: solo Zombie V1 receives the authoritative local DATA/player position. Existing mode retains `EnemyPerception` belief and nearest-player selection. Multiplayer is unchanged.
2. Steering: `RelentlessZombieMotor` returns normalized horizontal direction to DATA. Existing mode retains recurrent `EnemyMotor` steering.
3. Desired velocity: `Game::updateTargets` combines the selected steering with the existing speed/profile boundary and traversal route.
4. Actual velocity: `PhysicalEnemyBody::updatePhysicalEnemyBody` resolves the request through contact, support load, traction, acceleration limits, and disruption; `Game` consumes its returned velocity.
5. Foot placement: `EnemyLocomotion` owns persistent world-space planted and swing-foot targets.
6. Contact/load: `EnemyLocomotion` produces contact and load; `PhysicalEnemyBody` consumes those facts without inventing a second step authority.
7. Yaw: `EnemyLocomotion` requests supported yaw; `PhysicalEnemyBody` realizes yaw using planted-support turn authority.
8. Falling: `PhysicalEnemyBody` owns loss-of-support and posture fall thresholds.
9. Recovery: `EnemyLocomotion` owns gather-feet/establish-support/rise phases; `PhysicalEnemyBody` accepts recovery only with support evidence.
10. Attack commitment: Zombie V1 supplies constant commitment and `EnemyBehavior` no longer gates attack; existing combat still owns range, vertical overlap, cadence, swing commitment, hit, grab, and damage.
11. Presentation: `HumanVisual` and the renderer derive gait choice, physical pitch/roll, crouch, planted-foot projection, support transfer, and expression from existing state.
12. Intentional deviation in existing mode: recurrent state, temperament, caution, fixation, disruption exposure, lateral probing, social orbit, herd protection, perception uncertainty, hesitation, and speed/attack modulation can all deviate from direct pursuit. Zombie V1 bypasses these as intention authorities only.

## Bypassed versus retained

Bypassed in solo relentless mode:

- perception uncertainty as target authority;
- `EnemyBehavior` travel/attack gating;
- recurrent/animal `EnemyMotor` steering, hesitation, personality, learning, and herd steering;
- the locomotion directional-commitment cache on unobstructed direct pursuit, because live evidence showed it could preserve an obsolete heading after DATA moved.

Retained:

- `EnemyPerception`, `EnemyBehavior`, and `EnemyMotor` source and runtime comparison mode;
- target/player lifecycle and multiplayer behavior;
- collision queries and existing obstacle/traversal route selection;
- `EnemyLocomotion`, foot plants, foot targets, contact/load transfer, corrective steps, and recovery phases;
- `PhysicalEnemyBody`, traction, actual velocity/yaw, disruption, slipping, falling, and recovery;
- combat, animation/presentation, and renderer.

## Runtime experiment

Launch the deterministic one-zombie flat benchmark:

```text
DigitalBreakdown.exe --zombie-v1-benchmark --enemy-motor relentless --enemy-variant feral-hybrid --zombie-debug
```

A/B at launch:

```text
--enemy-motor relentless
--enemy-motor existing
```

Live A/B through the existing developer console (backtick):

```text
motor relentless
motor existing
playtest zombie
zombie debug on
zombie debug off
state
```

Diagnostic colors:

- red: enemy to DATA;
- yellow: requested steering/trajectory;
- blue: actual horizontal velocity;
- green: planted support span;
- cyan: current swing/next-foot target;
- white: projected COM to support center.

The `state` command reports distance, requested/actual speed, left/right load, support error, disruption, traction, and fall/recovery state for zombie zero.

## Automated evidence

`RelentlessZombieMotorTest` verifies direct steering, full speed/attack commitment, no voluntary retreat vector, invalid-target behavior, and immediate identical intention after a conceptual disruption.

`ZombieV1BenchmarkTest` verifies one supported zombie on flat ground trends more than two meters toward stationary DATA with tightly bounded negative-progress frames, retains its target after a lateral physical impulse, and redirects laterally toward moving DATA. Both modes share the same `Game::advancePhysicalBody` call and downstream locomotion/body stack.

Verification commands:

```text
tools\dbdev.ps1 desktop-build
cmake -S Z:\native-desktop -B Z:\build\zombie-v1-checks
cmake --build Z:\build\zombie-v1-checks --config Release -- /m:1
ctest --test-dir Z:\build\zombie-v1-checks -C Release --output-on-failure
DigitalBreakdown.exe --agent-playtest --zombie-v1-benchmark --zombie-debug --enemy-motor relentless --enemy-variant feral-hybrid
```

Results:

- native desktop release build: pass;
- CTest release suite: 78/78 pass;
- stationary DATA: frame 0 distance 10.0; at frame 120 the zombie advanced from z=-5.000 to z=-3.160;
- moving DATA: after DATA moved laterally, the zombie changed x from 0.058 to 0.393 by frame 240; the longer deterministic test requires x advancement greater than 0.45;
- diagnostic framebuffer capture: pass.

The Windows-only Pass 7 monolithic fixture now requests a 4 MiB test stack. It deliberately keeps many complete `Game` values alive simultaneously and was already close to the default 1 MiB limit; this is test-harness capacity, not gameplay state or shipping runtime behavior.

## Decision gate

The deterministic evidence supports the causal portion of Outcome A: a simple persistent intention reaches the unchanged modern physical stack, closes on stationary DATA, redirects toward moving DATA, and survives a physical shove without losing its objective.

It does **not** yet prove that the result is visually convincing. The environment allowed frame capture and frame-stepped positional playtesting, but not a human-quality continuous-motion judgment. Foot sliding, cadence, knee geometry, support-transfer readability, knockdown silhouette, and rise quality remain visual playtest questions. Therefore no evidence currently supports replacing `PhysicalEnemyBody`, and Outcome C is not supported.

The next smallest experiment is a recorded fixed-camera comparison of relentless versus existing intention using the same seed/body, with one stationary pursuit, one lateral DATA movement, one shove, and one knockdown. If motion looks wrong while red/yellow intention remains correct, classify and change only locomotion/support/pose—not AI.
