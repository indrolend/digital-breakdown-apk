# Traversal geometry authority

This audit records the native authority chain before generated rock support is
extended. `RoomEnvironmentPlan` decides composition and intent;
`Game::buildRoomColliders()` decides ordinary room physics. Rendering is not a
gameplay authority.

| Object family | Generation authority | Rendered geometry | Collision / support | Ledge / special movement | Mismatch and decision |
|---|---|---|---|---|---|
| Room shell and box obstacle | `roomPlan()` / `obstacle()` | The room renderers draw the same `ObstacleSpec` as a box | `RoomCollider` copies the box bounds and `topY`; support and obstruction use it | Four rectangular faces at `topY` | Truthful for its deliberately rectilinear form. **KEEP**. |
| Traversal perch / platform | `TraversalGraph` plus `physicalTraversalObstacle()` | Both renderers draw the derived obstacle box | `buildRoomColliders()` consumes the same derived box | Four rectangular faces at the same `topY` | Physicalized optional surfaces are truthful boxes. The graph is planning/capability authority, not collision. **KEEP**. |
| Slope | `SlopeSupport` | `makeSlopeWedgeMesh()` derives a closed wedge from the support definition in both renderers | `sampleSlopeSupport()` supplies bounded height, normal, and walkability | Not exposed to the rectangular ledge system | Best current shared render/physics authority. **KEEP / MODEL**. |
| Faceted rock | `EnvironmentPropSpec` plus deterministic `faceted_rock::makeMesh()` | Eighteen broad triangles generated from base, shoulder, and crown vertices | `environmentPropCollider()` replaces the mesh with one full-footprint box and one `topY` | The invisible box contributes four rectangular ledges | Angled visible faces cannot support; the box can support empty space above the silhouette. **PROVE facet-derived support in a fixture; leave generated rooms unchanged**. |
| House | `EnvironmentPropSpec` | Renderer-local compound body, two roof tiers, and facade inset | One unrotated full-size `RoomCollider` | One coarse rectangular top and four faces | Roof tiers are visible-only and yaw is not represented physically. **DEFER shared compound specification**. |
| Ruin | `EnvironmentPropSpec` | Renderer-local main mass plus offset upper remnant | One unrotated full-size `RoomCollider` | One coarse rectangular top and four faces | The offset upper ledge is undiscoverable; the collider also fills absent volume. **DEFER shared compound specification**. |
| Tree trunk | `EnvironmentPropSpec`; trunk proportions are reconstructed separately by each renderer | Desktop uses three tapered box segments; Android uses one box | One `TreeTrunk` AABB, narrower than the full prop, plus synthetic `climbTopY` | Dedicated attach, climb, descend, and jump-off logic | Intentional authored verb compensates for non-shared trunk geometry. **KEEP behavior; later share trunk specification**. |
| Tree crown / foliage | Shared cluster layout on desktop; simplified single crown on Android | Multiple desktop crown boxes versus one Android crown box | No crown support or obstruction | Tree climb clamps to `climbTopY` without transitioning to crown support | Visible crown/top is not physical and renderer parity is incomplete. **DEFER crown support and transition design**. |
| Marker pillar | `EnvironmentPropSpec` | Body box plus renderer-local oversized cap | One body-sized `RoomCollider` | Body `topY` only | Cap overhang and its top are visible-only. **DEFER shared compound specification**. |

## Authority diagnosis

Genuine authorities are `RoomEnvironmentPlan` for deterministic composition,
`TraversalCapabilities` for the canonical movement envelope,
`RoomCollider` for deliberate boxes, and `SlopeSupport` for bounded planar
support. `FacetedRock` is currently only a render authority. House, ruin, tree
and marker renderers independently reconstruct compound forms, so their coarse
colliders are compensating approximations rather than descriptions of the
visible geometry.

`Game::tryBeginLedgeHang()` discovers only the four faces of each
`RoomCollider`, filtered by one `topY`. Shimmy and mantle remain bound to that
collider. This preserves authored jump-to-hang and descending-lunge-to-hang
behavior, including tangent momentum, but cannot discover an exposed generated
edge.

The smallest future ledge change is not an object permission flag. It is a
bounded exposed-edge query that returns an edge segment, outward normal,
adjacent support height, and clearance. Existing hang, shimmy, mantle and lunge
transition code can consume that result. This proof does not implement it.

## Bounded proof boundary

The controlled fixture will contain an unchanged box, the existing unchanged
slope, and one deterministic current faceted rock. Only upward, walkable rock
triangles may answer the support query. Side/steep triangles remain non-support
and obstruction stays bounded to the rock footprint. Normal room generation,
generated rock collision, ledge discovery, network protocol and traversal verbs
remain unchanged.
