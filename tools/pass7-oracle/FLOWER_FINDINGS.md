# Pass 7 flower and supplemental-power dependency map

Authoritative source: `reference/browser-pass7/index_module.mjs`.

## State and constants

- One stack counter currently exists: `activePowerupStacks.flower`.
- Supplemental power starts inactive at `0/85`.
- The first stack keeps maximum stock at `85`; each later stack adds `32`.
- A pickup adds one stack and `46` stock, clamped to the current maximum.
- Brutes roll a `0.26` flower-drop chance when their shell first becomes
  slurpable. This is not rolled on capture or final removal.

## Spending and feeding

- Every battery cost is first reduced by stored-soul efficiency:
  `cost / (1 + storedSouls * 0.16)`.
- Supplemental power absorbs the reduced cost before main battery is touched.
- Empty supplemental power clears all active powerup stacks and resets its
  maximum to `85`.
- A successful shell hit feeds `2.8` supplemental power when a stack is active.
- A completed ingestion feeds `9.0` supplemental power after the main battery
  receives its `18.0` capture gain.
- Because `spendBattery` owns this ordering, implementing flowers as an isolated
  pickup without routing every battery expense through the shared spending path
  would produce incorrect death, combat, movement, and HUD behavior.

## Drop and pickup lifecycle

- Drops are separated from their source and existing drops using a deterministic
  list of angular offsets, but their initial Y rotation and drop roll use
  `Math.random()`.
- Base spawn Y is at least `0.38`; brute shell conversion requests
  `HUMAN_VISUAL_FOOT_Y + 0.42`.
- The visual bobs by `sin(age * 3.2) * 0.16` and rotates at `1.35` radians/sec.
- Pickup uses a full 3D radius of `1.05`, including the current bobbed Y.
- Each drop has continuation-room mirror models. Only the canonical drop owns
  gameplay collision and pickup.

## Update and reset order

- Frame order is movement, pending shots, human walking, vacuum, battery,
  queued capture commits, population, flowers, room loop, camera, visuals.
- Flower feed from vacuum capture therefore occurs after that frame's battery
  update, while flower pickup occurs afterward during `updateFlowerPowerups`.
- Startup/death and room/reset paths must be audited separately: the reference
  explicitly clears active stacks and spawned flower objects in run reset paths.

## Current native gap

The shared native state currently has main battery only. It does not represent
flower drops, powerup stacks, supplemental value/max/active state, or their HUD.
The native renderers likewise have no flower draw path. This is a gameplay and
state-machine port, not simply an asset substitution.
