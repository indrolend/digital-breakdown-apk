# Room environment grammar foundation

## Ownership

`early_browser_visuals::roomPlan(roomSeed, roomIndex)` is the deterministic,
pure plan authority. `Game::buildRoomColliders()` converts its bounded obstacle
specifications into gameplay `RoomCollider` state. Desktop and Android render
those same colliders; they do not generate independent gameplay geometry.
Authoritative multiplayer snapshots continue to carry the room seed/index and
the resulting collider array.

The repeating 30 by 42 room shell, ground, doorway, capture grid, enemy spawn
rules, and room transition remain owned by the native simulation. This
milestone does not alter their authority or the network protocol.

## Implemented grammar

A room plan now names four separate dimensions:

- `RoomSetting`: Field, City, Sterile
- `RoomForm`: Open, Corridor, Courtyard
- `RoomScale`: Compact, Standard, Large, Arena
- `RoomCondition`: Normal, Recovery

Field/Open, City/Corridor, and Sterile/Corridor preserve the prior layouts.
City/Courtyard is the first new form. Its scale changes the bounded structure
radius and footprint while the fixed shell depth remains unchanged. Arena is
reserved vocabulary and is not selected yet.

Rooms also expose a bounded deterministic presentation-prop layer built from a
small reusable primitive kit: stepped-roof house silhouettes, trunk/crown
trees, lawn fragments, and sterile marker pillars. These props are placed
outside the protected required route. Recognizable solid roles contribute
bounded native colliders and therefore remain honest under player contact;
ground-coincident lawn fragments remain presentation-only. Desktop and Android
derive identical prop specifications and assemble them from their existing
reusable box geometry, while multiplayer snapshots carry the resulting native
colliders. The kit is architectural vocabulary, not a
commitment to houses as the central theme; later settings can recombine or
transform the same primitive roles.

Field currently deploys only ground-coincident lawn fragments. Tree geometry
remains available in the kit but is withheld from generation until placement
validation accounts for nearby ledge-selection volumes; an early solid-tree
trial correctly failed the existing lunge-to-ledge regression fixture.

The complete prop set is now accepted or rejected by one deterministic pure
validation function shared by simulation and both renderers. It checks room
bounds, protected traversal surfaces, clearance from gameplay obstacles,
prop-to-prop separation, and the fixed collider budget. A rejected set produces
neither visible props nor colliders, preventing partial render/simulation
disagreement. The regression sweep exercises this contract across 4,096
seed/room pairs in Release builds.

Every generated plan contains a small required traversal route from entry,
through the central circulation space, to the capture/exit approach. Generation
tests validate that route against every obstacle with the phone's canonical
comfortable clearance radius. The native collider builder rejects optional
obstacles if a future plan fails this contract, leaving the stable room shell,
objectives, and exit reachable.

The route is now represented as a bounded action-labelled traversal graph.
Surfaces are graph nodes; edges identify the intended verb (`Walk`, `Jump`,
`DoubleJump`, `Lunge`, `JumpLunge`, `Drop`, or `LedgeRecover`) and an intended
difficulty band (`Automatic` through `Expert`). Existing rooms currently use
only required `Walk` edges, so this representation change does not silently
turn scenery into mandatory parkour.

## Automated traversal calibration

`TraversalCalibrationTest` drives the real `Game` input/update path against a
controlled two-platform fixture. It does not set jump velocity or teleport the
player during an attempt. Its initial profile holds ordinary forward movement,
varies takeoff position across approximately 0.84 world units, applies plus or
minus two degrees of steering error, and records landing success.

The initial 21-trial sweep observed:

- 1.50-unit ground-jump gap: 21/21
- 2.00-unit ground-jump gap: 21/21
- 2.50-unit ground-jump gap: 10/21
- 1.50-unit gap with a 0.30 rise: 18/21
- 1.50-unit gap with a 0.50 rise: 21/21
- 2.00-unit gap with a 1.50-wide landing: 21/21

These measurements are regression fixtures for one controller profile, not
universal human-accessibility limits. They establish that 2.50 units must not
be called comfortable based only on analytic physics. Human calibration and
additional approach-speed, landing-width, height, camera, and action profiles
must precede use as generation policy.

The higher ascent outperforming the lower ascent is an observed interaction of
the actual arc, platform face, support, and takeoff profile. Difficulty must not
be inferred by independently sorting gap or height; useful generation bands
need measurements of complete edge configurations.

## Observed mappings

- Room seed and index already determine the complete environment plan.
- Room advancement increments the index and advances the seed deterministically.
- The host owns transitions and collider state; snapshots transfer that state.
- Gravity, ground jump, double jump, air-lunge distance, and phone clearance are
  native gameplay constants. They now share a canonical traversal capability
  contract with generation validation.
- Desktop and Android both render `GameState::roomColliders` and derive only
  presentation materials/grass/sidewalks from the deterministic plan.
- The current room has a permanent floor and ground-level doorway/objective
  approach. It has no room-condition-owned void/fall/respawn contract.

## Derived decisions

- `Scale` currently means usable layout footprint/density, not room tile depth.
  This avoids changing repeating topology, camera seams, and door ownership.
- The comfortable traversal margin is 72 percent of the analytic maximum for
  jump height and air-lunge distance. This is a conservative design margin, not
  a measured human-input percentile.
- City/Courtyard was chosen ahead of Rooftops/NoFloor because it proves
  independent composition and circulation without inventing fall behavior or
  moving required objectives.

## Not implemented

Run-level passages, setting transitions, vertical required routes, no-floor
rooms, atmosphere interpolation, elevated enemy/objective placement, shareable
run seeds, dynamic room dimensions, and a playable human calibration course
remain future work. The automated controller currently measures only level
ground jumps; it does not yet estimate double-jump, lunge, ledge recovery, route
discovery, camera readability, combat interference, or fun. Historical browser
artifacts remain reference-only and provide no gameplay authority here.
