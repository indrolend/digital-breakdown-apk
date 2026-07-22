# Pass 7 vacuum and crosshair oracle findings

Authoritative runtime SHA-256:
`5dd181b2590887e63f5c517cd69dfad7cfe9a0145d321db9e4356dd9960f0b2b`

The deterministic suite runs at exactly 60 FPS. Two independent page loads produced the same
832,068-byte trace with SHA-256:
`a80000f43d1b3f076e01c4c504c8c590dc9d26b3c4e5d965b9ca3fdcc9514742`.

## State ownership

- Vacuum input/field stays active when aim is lost.
- A free soul outside the offer cone remains `FREE` with target `-1`.
- A pre-latch `ATTRACTED` soul becomes `FREE` after aim loss, while vacuum remains active.
- Returning aim reacquires that soul without restarting or recharging vacuum.
- A `LATCHED` or `INGESTING` soul remains offered and owned after aim loss; ingestion continues.

## Update-order evidence

The browser updates vacuum before updating the camera. During the rapid-turn fixture, the rendered
snapshot at 183.33 ms already places the soul outside the new camera offer cone, but vacuum still owns
target `0` in state `ATTRACTED`. At 200.00 ms, the next vacuum update observes aim loss and performs
`ATTRACTED -> FREE`, setting target to `-1`.

The current native order updates the camera before vacuum, so it cannot reproduce this one-frame
retention without an explicit previous-frame camera basis.

## Crosshair/offer-ray alignment

A soul placed six world units down the browser camera world direction projects to NDC `(0, 0)` and
passes the attraction offer at all tested pitches: `-1.2`, `-0.75`, `-0.35`, `0`, `0.35`, `0.75`, and
`1.2` radians. Maximum measured ray distance was below `0.0000003` world units.

The current native third-person renderer looks along `normalize(13 * camera.forward - (0, 0.65, 0))`
while attraction tests use `camera.forward`. The derived angular disagreement is approximately
`1.0-2.9` degrees across the same pitch range, including `2.86` degrees at zero pitch. Therefore a
native cube can be under the rendered crosshair while failing the native offer-cone calculation.

## Crosshair behavior

The browser crosshair owns a persistent rotor angle. During the controlled charge it progressed from
`1.77` to `252.29` degrees over 30 frames as power and lock increased. It continued through aim loss,
wrapping through 360 degrees. Arm spread is separately animated from rotor angle.
