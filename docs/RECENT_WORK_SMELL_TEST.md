# Recent release-polish smell test

This audit distinguishes shipped behavior, bounded proofs, and unfinished
architecture on `release-polish-semantic-materials`. A passing fixture is not
treated as proof that every generated instance behaves correctly.

| Claim | What the source actually establishes | Confidence | Decision |
|---|---|---:|---|
| Original DATA device | One authored GLB is shared by desktop and Android presentation; gameplay dimensions remain separately authored. | High | KEEP; validate the transformed visible body against contact in action poses. |
| Semantic material response | Damage emitters carry shared semantic material information into both renderers. | High | KEEP; avoid renderer-local meanings. |
| Human substrate progression | Existing damage state drives increasingly environmental color and terminal reclamation. | High | KEEP. |
| Faceted natural masses | A bounded deterministic generator supplies the visible rock facets in eligible Field and Coastal placements. | High | KEEP. |
| Slope traversal | One bounded slope definition supplies both visible wedge and gameplay support; controller fixtures cover ordinary actions. | High for the fixture | KEEP as the reference authority pattern. |
| Rock support | Gameplay projects the generated rock's upward-facing facets into X/Z and samples their height. | Medium | REVISE; this is a height-field interpretation, not full solid contact. |
| Rock obstruction | A separate footprint/envelope test restores the prior X/Z position and cancels horizontal velocity. | Low | REPLACE with one coherent body-versus-solid contact contract. |
| Compound ruins | Shared authored component boxes prove render/collision reuse in a fixture. | Medium | KEEP as a proof; normal-room coverage remains limited. |
| Static support authority | Ground, boxes, slopes, and rock facets share a support query. | Medium | STRENGTHEN; support selection currently favors the highest candidate and does not by itself establish reachable contact. |
| Ledge authority | Ledge discovery still primarily consumes rectangular collider faces and one `topY`. | Low for non-box forms | DEFER broad rewrite; next edge work should consume exposed support boundaries and clearance. |
| Tree traversal | Dedicated trunk climbing is intentional and functional, but crown presentation is not ordinary support geometry. | Medium | KEEP verb; later connect trunk, crown support, and ledges truthfully. |
| Controller surface sweep | Deterministic tests exercise many rock profiles, actions, and camera states. | Medium-low | STRENGTHEN; current oracle shares the same projection assumptions and does not measure the rendered phone hull. |
| Desktop build provenance | Identity was generated only at CMake configure time, allowing rebuilt executables to report stale source. | Failed before this audit | FIX FIRST; refresh identity every build and expose dirty state. |

## Architectural diagnosis

The slope is the cleanest current model: one specification produces both the
visible plane and its support query. Boxes are also coherent because their
visible and physical abstractions agree.

Rock behavior is transitional. The same deterministic mesh is available to
rendering and support, but three different approximations still govern play:

1. projected upward triangles for standing;
2. a footprint/envelope for side obstruction;
3. a retained axis-aligned box used by some generic consumers, including parts
   of camera collision.

Those approximations can disagree at facet boundaries and during vertical
motion. Restoring the previous horizontal position hides penetration after it
has occurred; it does not compute a contact point or separation direction.

The highest-leverage geometry step is therefore not more shapes. It is a small,
shared contact contract for a player body against the existing deterministic
rock solid, with independent black-box tests that compare the resulting player
state to rendered triangles. General imported-mesh physics is not required.

## Evidence gaps

- Tests sample the same 2.5D rock projection used by gameplay, so correlated
  errors can pass both implementation and oracle.
- Most assertions inspect `PlayerState::pos`; action/gait presentation can move
  and rotate the visible phone independently.
- Randomized fixture mutation must rebuild every retained physical derivative;
  otherwise the mesh, envelope, and collider no longer describe one object.
- Camera collision must consume the same chosen physical representation once
  the rock contact contract is consolidated.

## Next bounded proof

For every deterministic rock profile and yaw used by the controller suite:

1. transform the phone's actual conservative body hull through representative
   grounded, jump, landing, lunge, melee, vacuum, and shooting poses;
2. test it independently against the rock triangles, including side faces;
3. assert finite state, support only on reachable upward facets, separation from
   steep faces, clean fall-off, and no empty-space box-top standing;
4. replace previous-position rollback only after the failing cases are captured.

This proof should precede new procedural obstacles, generalized ledges, or tree
top support.
