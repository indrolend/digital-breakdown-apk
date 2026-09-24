# Dirt Landing Material Response Convergence

## Purpose
Make the player's existing authoritative landing contact visibly affect dirt, with weather determining the material response.

## Change
The existing landing contact position and impact now feed the existing dirt presentation when the contact falls inside a dirt patch. Dry dirt lifts a tiny bounded dust kick; saturated dirt suppresses airborne dust and produces a low dark kick instead. A small pure `dirtContactResponse` maps existing contact strength and weather wetness into those two presentation amounts.

Authority remains: physical landing contact -> existing dirt footprint/material + weather wetness -> bounded presentation.

## Reused authorities
Player `landingContactPosition` / `landingImpact`, procedural dirt footprint, weather `wetness` / `exposed`, existing particle graphics setting, and existing `drawParticleCube` presentation path.

## Deliberately not added
No landing event queue, decal or track state, mud simulation, surface manager, texture, physics query, AI state, dynamic light, allocation, or unbounded work. The response is decorative and remains behind the existing particle graphics option; vegetation still carries landing contact on reduced graphics.

## Verification
- `dirt_footprint_test`: PASS, including dry/wet/half-wet contact-response invariants.
- `early_browser_visuals_test`: PASS.
- Strict C++20 `Game.cpp` compile with `-Wall -Wextra -Wpedantic -Werror`: PASS.
- Authored delta `git diff --no-index --check`: PASS.

## Known limitation
Windows/OpenGL renderer execution is unavailable here, so final dirt-contact appearance is not visually verified on the shipping target.
