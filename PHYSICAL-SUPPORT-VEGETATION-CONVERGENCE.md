# Physical Support Vegetation Convergence

## Purpose
Make vegetation read enemy stance and weight transfer from existing physical-body facts rather than snapping its disturbance to whichever foot happens to carry the larger load.

## Changed
- `grassBodyContact` now places each animal's bounded vegetation disturbance at the load-weighted center of its two authoritative planted-foot contacts.
- As `PhysicalEnemyBody` transfers weight between feet, the existing grass reaction follows that support transfer continuously.
- Existing velocity-driven directional sweep, two-animal-per-tile bound, wetness response, graphics-preset density, and renderer draw structure are unchanged.

## Reused authorities
`PhysicalEnemyBody` planted foot world positions and support weights -> `grassBodyContact` -> existing `grassTip` deformation.

## Deliberately not added
No gait state, vegetation manager, contact history, footstep event, extra enemy scan, per-blade body query, allocation, particle, texture, light, or draw call.

## Verification
- `early_browser_visuals_test`: PASS, including weighted support-center and left/right load-transfer invariants.
- `physical_enemy_body_test`: PASS; benchmark 32 enemies / 60 simulated seconds: 1.055 us per enemy-frame in this environment.
- `enemy_visual_support_transfer_test`: PASS.
- `enemy_visual_physical_posture_test`: PASS.
- Strict C++20 `Game.cpp` compile with `-Wall -Wextra -Wpedantic -Werror`: PASS.
- Reconstructed previous `HEAD.zip` as a temporary Git baseline; `git diff --check`: PASS.

## Known limitations
Windows/OpenGL presentation was not run here, so the visual feel of the support-center interpolation still needs direct desktop playtesting. No claim is made that the complete Windows/OpenGL target builds in this environment.
