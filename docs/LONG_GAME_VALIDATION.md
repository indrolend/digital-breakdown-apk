# Long-game playability validation

The registered native suite includes two deliberately simple longevity checks.

`DeterministicInputSoak` runs 180,000 fixed simulation steps: fifty minutes of
continuous randomized movement, camera input, sprint, jump, vacuum, melee and
shoot actions. It validates finite and bounded player, camera, battery, energy,
target, projectile, particle, collider and ownership state through repeated
death and restart cycles.

`RoomProgressionProbe` clears and advances 256 consecutive rooms. It validates
room seed evolution, environment plans, required-route and prop-layout
contracts, exact collider counts, finite positive collider geometry, clear
player/enemy starting positions, goal rewards, the level-ten secret event,
door transitions, upgrade interludes, rule saturation, target-count bounds and
fresh-room reset behavior.

These tests establish deterministic longevity and obvious state validity. They
do not claim that a random controller can complete combat, that every room is
fun, or that a physical Android device can sustain the run at target frame
rate. Those remain human and device playtest responsibilities.
