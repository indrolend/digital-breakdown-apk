# Multi-Body Vegetation Convergence

## Changed
Field grass previously reacted to only the single strongest enemy foot contact in a room tile, so nearby animals could visually erase one another's physical presence. The renderer now retains the two strongest existing `GrassBodyContact` projections and feeds both into the same bounded grass-tip deformation.

## Reused authorities
`PhysicalEnemyBody` planted feet -> renderer-only physical foot projection -> `grassBodyContact` -> `grassTip`. No gameplay or AI authority changed.

## Deliberately not added
No vegetation manager, persistent deformation field, per-blade enemy scan, physics collider, particle system, allocation, texture, or draw call. Cost remains one bounded enemy scan per visible grass tile plus at most two constant-time contact responses per blade. Legacy keeps the same blade-count reduction and the same interaction identity.

## Verification
- `early_browser_visuals_test`: pass, including a new invariant proving a second physical body contributes visible vegetation depression.
- strict C++20 `Game.cpp` compilation with `-Wall -Wextra -Wpedantic -Werror`: pass.
- whitespace check of authored source delta: pass.

## Limitation
Desktop OpenGL runtime presentation is not available in this Linux environment, so overlapping-animal grass feel still needs shipping-build visual playtesting.
