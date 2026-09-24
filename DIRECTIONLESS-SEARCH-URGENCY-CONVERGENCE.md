# Directionless Search Urgency Convergence

## Purpose
Make environmental events visibly change an animal's attention without granting hidden spatial knowledge or adding another behavior authority.

## Change
`EnemyPerception` already received directionless `vagueAwareness` from proximity, phone actions, shots, and nearby herd activity, and already owned a deterministic `searchPhase`. Awareness now modestly accelerates that existing search clock while the animal lacks confirmed perception. The resulting urgency is visible through the existing head/gaze presentation; it does not create confidence, coordinates, locomotion intent, or confirmation.

Authority chain remains: environment/action/herd event -> vague awareness -> EnemyPerception search clock -> gaze -> visual presentation. Spatial cues, animal intention, EnemyMotor, PhysicalEnemyBody, and contacts remain authoritative for movement.

## Deliberately not added
No alert manager, AI state, target injection, animation state, random timer, world query, allocation, renderer feature, particle, light, or draw call.

## Verification
- enemy_perception_test (including new awareness/search-urgency no-spatial-belief invariant)
- enemy_perception_environmental_cue_test
- enemy_perception_cue_memory_test
- enemy_perception_room_vigilance_test
- enemy_perception_room_memory_test
- enemy_weather_cue_transmission_test
- herd_physical_contact_cue_test
- enemy_motor_uncertain_investigation_test
- strict C++20 Game.cpp compile with -Wall -Wextra -Wpedantic -Werror
- authored delta whitespace check via git diff --no-index --check

## Known limitation
Windows/OpenGL runtime presentation is not available in this environment, so the gaze-speed change is contract/source verified rather than visually playtested in the shipping renderer.
