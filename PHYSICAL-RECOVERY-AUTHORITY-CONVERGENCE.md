# Physical Recovery Authority Convergence

Purpose: remove a competing physical-posture authority that could leave fallen enemies permanently inert after disruption.

Observed root cause:
- `PhysicalEnemyBody` kept its fall spring targeting the prone pose while a second get-up torque simultaneously targeted upright after the recovery delay.
- Those opposing torques converged near pitch 0.34 / roll 0.22 instead of crossing the existing 0.20 recovery threshold, so `fallen` could remain true indefinitely. A direct 10-second probe reproduced the permanent tilted equilibrium.

Changed:
- falling and getting up now use the same existing posture spring;
- after 1.25 seconds down and once horizontal speed is settled, that spring changes its target from the prone pose back to the normal support-derived upright pose;
- the redundant recovery torque was removed;
- locomotor authority still returns only after pitch and roll are both below the existing 0.20 threshold.

Reused authorities: `PhysicalEnemyBody` fall state, recovery timer, support posture, actual body speed, existing pitch/roll spring, existing locomotion gate.

Deliberately not added: recovery state machine, animation state, teleport/snap upright, AI override, new timer, manager, allocation, or renderer behavior.

Verification:
- focused physical-body recovery invariant: a fully prone supported body recovers within 360 frames;
- `physical_enemy_body_test` passes, including 32-enemy / 60-simulated-second CPU probe (~0.196 us per enemy-frame in this environment);
- `enemy_physical_authority_test` passes;
- `enemy_motor_physical_recovery_test` passes;
- strict C++20 `Game.cpp` compile with `-Wall -Wextra -Wpedantic -Werror` passes;
- `git diff --check` passes.

Known limitation: full Windows/OpenGL runtime was not available here. The reported startup dark-film presentation remains a separate runtime-playtest issue and was not guessed at or masked in this pass.
