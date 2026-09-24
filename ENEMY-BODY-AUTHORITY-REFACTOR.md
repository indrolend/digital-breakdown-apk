# Enemy body authority refactor

This pass follows a few recurring patterns seen in mature game/physics codebases:

- planning asks for motion; a single locomotion/physics authority decides what motion is physically possible;
- contact sensing comes from the world, while planted-support state belongs to the body controller;
- turning and acceleration require ground support rather than being assigned directly by AI;
- presentation reads the resulting physical state rather than inventing a separate stabilizer.

## Fundamental change

The enemy's horizontal center of mass is now projected from actual root position plus current body pitch/roll and a geometry-derived COM height. Lean therefore moves the projected COM. If gravity's projection moves outside planted support far enough, the body falls.

Previously, bodyPosition itself was used as the horizontal COM even while the visual body leaned. That allowed large root rotations without the support consequence a real body would have.

## Single ground-reaction authority

Desired velocity and desired yaw are requests.

Horizontal acceleration and new yaw torque are generated only when planted support exists. Catching an escaping COM and following a movement request share the same bounded ground-reaction budget; there is no second hidden balance force.

## Foot ownership

Game.cpp still performs world queries because it owns room geometry. It now supplies sensed foot positions and contact amounts only.

PhysicalEnemyBody owns:
- foot plant acquisition/release;
- plant weights;
- support center;
- COM support;
- acceleration;
- yaw torque;
- falling/recovery.

That removes a split authority where Game.cpp previously mutated foot-plant state before calling the body controller.

## Geometry, not another behavior knob

centerOfMassHeight is derived by Game.cpp from the existing humanoid visual height and target scale. It is geometry input, not a new personality or difficulty parameter.

## Verification

- Game.cpp compiles with C++20 and -Wall -Wextra -Wpedantic -Werror.
- enemy_center_of_mass_support_test passes.
- weighted_enemy_locomotion_test passes.
- enemy_physical_authority_test passes.
- The new authority test verifies:
  - a large lean moves projected COM far enough to fall;
  - an unsupported body cannot generate new turning torque;
  - sensed grounded foot contacts become planted support inside PhysicalEnemyBody.
- physical_enemy_body_test still reaches the pre-existing HumanVisual bodyCompression assertion failure; this pass does not modify that unrelated visual assertion.
