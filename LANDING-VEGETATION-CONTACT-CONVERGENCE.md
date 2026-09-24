# Landing Vegetation Contact Convergence

## Changed
The authoritative player landing contact now has a brief visible consequence in existing field grass. `landingContactPosition` and decaying `landingImpact` feed `GrassReactionInputs`; nearby blades receive a bounded radial press/bend, with wet grass compressing slightly more. This makes a hard touchdown readable in the same environment that animals can already hear it from.

## Reused authorities
Player support resolution -> `PlayerState::landingContactPosition` / `landingImpact` -> existing renderer grass reaction -> `grassTip`. No gameplay or locomotion authority changed.

## Deliberately not added
No vegetation state, landing manager, decal, particle burst, physics query, texture, dynamic light, allocation, draw call, or persistent deformation. The reaction is a constant-time calculation on already-rendered blades and disappears with the existing landing-impact decay. Legacy keeps the same lower blade density while retaining the contact response.

## Verification
- `early_browser_visuals_test`: pass, including near-impact deformation and zero distant deformation invariants.
- `enemy_landing_cue_test`: pass.
- `enemy_perception_environmental_cue_test`: pass.
- `material_response_test`: pass.
- strict C++20 `Game.cpp` compilation with `-Wall -Wextra -Wpedantic -Werror`: pass.
- reconstructed baseline `git diff --check`: pass.

## Known limitation
Desktop OpenGL renderer compilation/presentation cannot be verified in this Linux environment because OpenGL development headers are absent. Windows/OpenGL visual feel therefore remains unverified.
