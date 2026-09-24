# Physical Footprint -> Vegetation Convergence

## Purpose
Make animal body scale read as physical footprint, not only stronger grass compression, while preserving the existing bounded vegetation presentation path.

## Change
`grassBodyContact` now derives a tightly bounded contact radius from the existing body scale alongside its existing strength. The renderer passes that radius through `GrassReactionInputs`, and the existing `grassTip` body-disturbance relationship uses it as the falloff radius. Brutes therefore disturb a modestly wider patch of grass while ordinary animals retain the prior 1.15 m footprint.

## Reused authorities
- `PhysicalEnemyBody` planted-foot positions and support weights
- actual target velocity
- existing target body scale
- existing two-strongest grass-contact selection
- existing `grassTip` deformation and graphics-preset blade density

## Deliberately not added
No mass model, vegetation state, trail/history buffer, per-blade enemy query, manager, allocation, particle, texture, light, draw call, or persistent deformation.

## Verification
- `early_browser_visuals_test` PASS, including an edge-of-footprint invariant where a brute affects grass outside the normal-body radius.
- `physical_enemy_body_test` PASS.
- `enemy_visual_support_transfer_test` PASS.
- `enemy_visual_physical_posture_test` PASS.
- `Game.cpp` strict C++20 compile with `-Wall -Wextra -Wpedantic -Werror` PASS.
- authored delta whitespace check PASS.

## Known limitation
Windows/OpenGL renderer presentation was not directly run in this Linux environment.
