# Wet Landing / Vegetation Convergence

## Purpose
Make the player's existing landing-contact vegetation response obey the same wet-contact material behavior already used by physical animal bodies.

## Change
The existing grass wetness authority now damps lateral displacement from landing contact as well as animal contact, while preserving stronger downward compression. A landing in saturated grass therefore reads heavier and less springy than the same landing in dry grass. The shared response scalar removes a small inconsistency between two presentations of physical contact.

Authority remains: weather wetness + authoritative landing/body contact -> existing grass presentation.

## Deliberately not added
No vegetation state, landing manager, material subsystem, recovery simulation, physics query, allocation, particle, texture, light, draw call, or AI behavior.

## Verification
Focused visual contracts cover wet landing lateral damping and increased compression alongside existing wet body-contact contracts. Strict Game.cpp compilation and authored-delta whitespace checks are run for the packaged snapshot.

## Limitation
Windows/OpenGL presentation is not visually verified in this environment.
