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

Every generated plan contains a small required traversal route from entry,
through the central circulation space, to the capture/exit approach. Generation
tests validate that route against every obstacle with the phone's canonical
comfortable clearance radius. The native collider builder rejects optional
obstacles if a future plan fails this contract, leaving the stable room shell,
objectives, and exit reachable.

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
run seeds, and dynamic room dimensions remain future work. Historical browser
artifacts remain reference-only and provide no gameplay authority here.
