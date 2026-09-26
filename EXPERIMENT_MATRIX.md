# Digital Breakdown — Extreme Enemy Lab

Source authority: `indrolend/digital-breakdown-apk`, remote branch `codex/specification-convergence-pass3`, commit `1a17e257b8a95cec68eb8ef5ec0d43128cf1c489` (latest relevant remote source when this lab was created).

## How to select a variant

Build with `tools\dbdev.ps1 desktop-build`, then launch the executable with:

```text
DigitalBreakdown.exe --enemy-variant rabid-animator
DigitalBreakdown.exe --enemy-variant euphoria-lite
DigitalBreakdown.exe --enemy-variant traversal-predator
DigitalBreakdown.exe --enemy-variant feral-hybrid
DigitalBreakdown.exe --enemy-variant support-driven
```

The default is `rabid-animator`. The selection survives in-game restart/reset. Multiplayer deliberately retains its shipped deterministic cadence and does not apply solo lab speed changes.

## Controlled experiment

All variants share the same authoritative perception, evidence memory, cognition state, pursuit belief, attack eligibility, collision queries, support solver, and physical body. The experiment changes four independent profiles rather than branching the enemy brain.

| Variant | Ordinary gait presentation | AI intention / motor temperament | Traversal skill envelope | Physical consequences | Expected read |
|---|---|---|---|---|---|
| Rabid Animator | Distance-driven authored biped clip. Physical foot projection is isolated from the visible leg gait. | Fast, committed, moderately reckless. | Step, vault, mantle and tree climb capability; conservative routing. | Physical root disturbance is visually restrained; fast recovery. | Clearest COD-zombie-like baseline. |
| Euphoria-Lite | Physical contact gait and planted-foot projection remain visible. | Baseline pursuit speed with softer turn commitment. | Same conservative traversal envelope as the baseline. | Strong impact/imbalance authority and slower recovery. | Heavy, reactive, stumbling. |
| Traversal Predator | Distance-driven authored biped clip; no ordinary physical leg overwrite. | Fastest clean pursuit with strong turn commitment. | Aggressive routing profile with step, vault, mantle, climb and gap-jump capabilities exposed. | Moderate disruption, aggressive recovery. | Relentless, readable terrain hunter. |
| Feral Hybrid | Physical contact gait with maximum procedural expression. | Fast, highly reckless pursuit. | Full aggressive traversal profile. | Maximum impact and imbalance authority with aggressive recovery. | Most chaotic and animalistic. |
| Support Driven | Physical-contact gait reports the same contacts that earn movement. | Behavior supplies direction, target speed, urgency and facing only. | Full aggressive traversal profile. | Support loss, falling and recovery stay within the contact system. | Authority-inversion diagnostic: the body moves only as weighted support advances. |

## Support-driven authority experiment

`support-driven` is deliberately different from the first four variants. It selects `EnemyLocomotionAuthority::PhysicalSupport` and disables the desired-velocity-to-root acceleration path. A requested direction still chooses reachable swing-foot targets, but it cannot directly translate the root.

The physical body tracks the weighted center of its planted contacts. Root velocity is generated only when an unload/swing/contact/load sequence advances that support center. If no viable step or load transfer occurs, the correct diagnostic result is no ordinary travel (or a genuine loss of support), not concealed kinematic motion.

The automated invariant is:

```text
desired movement + unchanged loaded contacts = zero generated root travel
advanced contact + real load transfer = bounded generated root travel
```

## Authority boundaries

```text
perception + memory -> cognition -> pursuit intention
                                      |
                                      v
                              traversal skill selection
                                      |
                                      v
                                motor competence
                                      |
                     authored gait / contact gait choice
                                      |
                              procedural expression
                                      |
                              physical consequences
```

- AI intention decides *where and why* to move.
- Traversal capability describes *which skills may solve the route*.
- Motor competence controls speed, commitment and recklessness.
- Presentation chooses one ordinary gait authority, avoiding authored gait plus physical leg overwrite at the same time.
- Physics remains the authority for collision, support, imbalance, falling and recovery consequences. Glancing collisions resolve only the closing normal component, preserve tangent motion, and apply the same world impact to body pitch/roll.
- Contact-gait variants project both forward and lateral placement from their real foot contacts; lateral placement is derived in the renderer and does not expand gameplay or network state.

## Visual playtest checklist

Run every variant through the same room/seed and inspect:

1. Forward sprint silhouette: knees bend forward, feet do not read as alternating tip-toes, and translation agrees with cadence.
2. Hard 90/180-degree pursuit turns: intent remains stable while the body reorganizes.
3. Wall and obstacle encounter: the enemy routes around or selects a traversal opportunity without frame-to-frame indecision.
4. Tree pursuit: the enemy approaches, attaches, climbs, and attacks only after reaching vertical overlap.
5. Shoulder/side impact while moving: Euphoria-Lite and Feral Hybrid should visibly yield more than the animation-led variants.
6. Loss of support and recovery: falls must be consequential, followed by a readable return to pursuit.
7. Same seed comparison: perception/cognition decisions should remain comparable; visible/body differences should explain the changed feel.

Automated checks cannot judge knee direction, foot sliding, silhouette quality, transition naturalness, or whether the result feels “rabid.” Those items require visual playtesting.

## Verification record

- Native desktop release build: PASS.
- CTest behavioral suite: PASS, 76/76 from a clean short-path release build.
- Release smoke launch: PASS for `rabid-animator`, `euphoria-lite`, `traversal-predator`, `feral-hybrid`, and the `support-driven` diagnostic.
- Support-driven locomotion authority contract: PASS; intention alone cannot translate the root and contact transfer earns bounded translation.
- New variant selection/profile contract: PASS.
- Collision tangent preservation, world-to-body impact mapping, and lateral foot-plant contracts: PASS.
- Existing perception, cognition, locomotion, recovery, physical-authority, multiplayer determinism, traversal calibration, slope traversal, and Pass 7 parity contracts: PASS.
- The high-level `desktop-test` wrapper encountered Windows path-length bookkeeping failures in third-party miniaudio targets. The established short-drive build avoided that environment limitation and completed the same release build and all 76 tests successfully.
- Visual playtesting of the four primary variants and the support-driven diagnostic: REQUIRED.
