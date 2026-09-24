# Physical Body Scale -> Vegetation Convergence

## Purpose
Make animal size physically legible in field vegetation without adding mass simulation, vegetation state, or another contact authority.

## Change
The existing `grassBodyContact` presentation now accepts the target's existing body scale. Larger animals amplify the strength of the same planted-foot support contact with a bounded multiplier; contact origin, support transfer, and velocity sweep remain authoritative and unchanged. `DesktopRenderer` passes `target.scale` into that existing relationship.

## Reused authorities
- `PhysicalEnemyBody` planted-foot positions and weights
- actual target velocity
- existing target body scale
- existing two-strongest grass-contact selection
- existing `grassTip` deformation and graphics-preset blade density

## Deliberately not added
No mass state, vegetation manager, contact history, physics query, AI state, allocation, particles, texture, light, draw call, or persistent deformation.

## Verification
See packaged test/compile results from this pass. Desktop OpenGL runtime presentation was not directly verified in this environment.
