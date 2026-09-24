# Physical Weight Transfer Convergence

Built directly on `DigitalBreakdown-PhysicalEnemies-MaterialWeather.zip`.

## Purpose
Make planted support visibly carry enemy mass without introducing an animation-side stabilizer or a second locomotion authority.

## Change
- `PhysicalEnemyBody` now feeds its existing projected-COM/support error into bounded pitch/roll posture targets.
- Single-foot and displaced support therefore produce a small body lean toward the support that is already carrying the body.
- The same support error already used for catch ground reaction remains authoritative; this pass only lets that physical fact become visible through the existing body pose.
- Added a focused physical-authority assertion proving single-foot support develops bounded roll toward the planted foot.

## Deliberately not added
- No animation state machine.
- No inverse-kinematics subsystem.
- No renderer-only fake lean.
- No new per-frame world probes, allocations, particles, lights, or draw calls.
- No cadence or movement-speed inflation.

## Verification
Passing in this environment:
- `early_browser_visuals_test`
- `enemy_perception_test`
- `enemy_physical_authority_test`
- `weighted_enemy_locomotion_test`
- `enemy_center_of_mass_support_test`
- `material_response_test`
- strict C++20 compile of `Game.cpp`
- `git diff --check`

Known inherited failure:
- `physical_enemy_body_test` still aborts at line 275 on the pre-existing `embodiedPose.bodyCompression > 0.0f && embodiedPose.bodyCompression < 0.07f` assertion. This pass does not touch `bodyCompression`.

The full Windows/OpenGL target was not built in this Linux environment.
