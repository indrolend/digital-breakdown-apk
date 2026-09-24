# Landing Contact Cue Convergence

## Purpose
Make airborne movement and landing obey one physical perceptual story: airborne motion does not transmit through the floor, but actual landing contact can create a short, bounded environmental cue.

## Changed
- `PlayerState` now retains only the latest landing impact speed as short-lived physical contact state.
- Both existing player support-resolution landing paths record that impact before vertical velocity is cleared.
- The value decays rapidly and feeds `EnemyPerception` through `landingAwarenessStrength`.
- Landing transmission reuses the already-authoritative room floor material and weather attenuation.
- The landing point can become an uncertain environmental cue for investigation; it does not create authoritative player tracking.

## Reused authorities
Player support/grounded resolution, floor `MaterialResponse`, weather cue transmission, `EnemyPerception`, existing uncertain environmental-cue investigation.

## Deliberately not added
No footstep/landing manager, event queue, audio propagation graph, AI state, renderer effect, particles, dynamic light, allocation, or world query.

## Verification
- Strict C++20 `Game.cpp`: `-Wall -Wextra -Wpedantic -Werror` passed.
- `enemy_landing_cue_test` passed.
- `enemy_perception_environmental_cue_test` passed.
- `enemy_perception_test` passed.
- `material_response_test` passed.
- `enemy_weather_cue_transmission_test` passed.
- Archive has no Git metadata; whitespace validation is performed on the generated authored delta rather than claiming a literal repository `git diff --check`.

## Known limitation
Windows/OpenGL runtime presentation and feel cannot be verified in this environment.
