# Weather / Contact Vegetation Convergence

## Purpose
Make the existing physical animal/grass contact response inherit the existing wetness authority instead of behaving laterally identical in dry and saturated vegetation.

## Change
`GrassReactionInputs::wetness` now slightly damps lateral/radial body displacement while retaining the existing stronger downward compression. Wet grass therefore reads heavier and more yielding under animal weight without new state: bodies press it down more but sweep it sideways less.

Authority remains: weather wetness + PhysicalEnemyBody planted support/velocity/scale -> existing grass presentation.

## Deliberately not added
No vegetation state, recovery simulation, manager, material lookup, per-blade body query, physics query, allocation, particle, texture, light, draw call, or AI behavior.

## Verification
Focused early-browser visual contracts cover reduced wet lateral travel plus increased wet compression. Strict Game.cpp compilation and authored-delta whitespace checks are run for the packaged snapshot.

## Limitation
Windows/OpenGL presentation is not visually verified in this environment.
