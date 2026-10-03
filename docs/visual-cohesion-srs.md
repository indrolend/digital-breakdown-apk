# Data Visual Cohesion and Identity SRS

Status: Draft implementation contract  
Date: October 3, 2026  
Scope: Player-facing visual language, animation, interface composition, transitions, audio/haptic coordination, and visual evidence

## 1. Purpose

This specification defines how Data should become visually cohesive, characteristic, legible, and emotionally expressive without replacing the game's existing runtime or attaching an unrelated interface layer to it.

The governing premise is:

> Data's world, entities, physical phone, interface, progression, and transitions are different projections of one living signal-processing system.

The work should converge existing strengths. It should not make every surface look identical, introduce a generic game-engine framework, or add decoration merely to make screens busier.

## 2. Experience objective

Data should feel:

- tactile, strange, intimate, and reactive;
- calm and legible during ordinary decisions;
- increasingly unstable and expressive under danger;
- distinctive because systems visibly behave, not because effects are layered over them;
- continuous when moving between play, menus, progression, failure, and restart;
- reliable under keyboard, mouse, and controller input;
- authored rather than assembled from unrelated interface conventions.

A player should be able to understand important state changes from coordinated world behavior, posture, device response, sound, and motion before reading explanatory text.

## 3. Non-goals

This work shall not:

- replace gameplay authority with renderer-side inference;
- introduce a new entity-component system or general-purpose animation engine;
- redesign all gameplay systems merely to support a visual treatment;
- convert every game state into a screen-space HUD element;
- make animation interfere with control reliability;
- make every menu category a different arbitrary color;
- use constant motion as a substitute for hierarchy;
- turn developer diagnostics into the player-facing visual language;
- imitate another game's typography, branding, or death screen;
- discard effective current primitives solely to make the architecture uniform.

## 4. Unified fiction

The visual language should treat the game as one system:

- The world is mutable data made physical.
- The phone is an instrument for interpreting, storing, and manipulating that world.
- The HUD is selective telemetry mediated by the same system.
- Souls are captured information with physical mass and behavior.
- Enemies express cognition through bodies, movement, rhythm, and signal disturbance.
- Doors recompile or reorganize world state.
- Upgrade selection edits rules during recompilation.
- Damage disrupts synchronization among body, device, and world.
- Death is loss of synchronization.
- Restart is signal reacquisition.
- The secret television is a foreign, damaged, or intrusive transmission.

These statements establish visual causality. They do not require literal exposition to the player.

## 5. Presentation authority

### 5.1 Authority pipeline

Player-facing presentation should follow this direction:

```text
simulation truth
    -> semantic presentation state
    -> composition and layout
    -> platform renderer
    -> visual, audio, and haptic evidence
```

### 5.2 Simulation authority

Simulation state owns facts such as:

- enemy behavior and physical state;
- soul lifecycle state;
- player health, battery, motion, damage, and death;
- progression and objective state;
- room topology and population;
- active menu page, selection, and value;
- input acceptance and action results.

### 5.3 Presentation authority

Semantic presentation state owns how those facts are expressed, including:

- search posture, commitment, disruption, and individuality;
- soul deformation, tethering, and ingestion response;
- phone pose, deformation, glow, and display mode;
- acquisition, selection, warning, completion, and failure response;
- transition phases and motion vocabulary;
- semantic color roles.

Existing examples include `HumanReactionVisual`, `HumanVisualPose`, `SoulVisualState`, `PhoneVisualState`, `PhoneDisplayState`, menu view models, and scene-lighting response.

### 5.4 Composition authority

Composition decides:

- where information appears;
- which cue dominates;
- which supporting cues are allowed;
- how much information is visible simultaneously;
- what persists between states;
- how one state transforms into the next.

### 5.5 Renderer responsibility

The renderer draws geometry, materials, text, textures, and effects. It should not independently invent semantic meaning when that meaning can be supplied by shared presentation state.

Renderer-local effects may remain when they are purely representational and do not create competing state authority.

## 6. Current reusable primitive inventory

### 6.1 Physical response

Retain and reuse:

- spring-based phone ingestion bulge and settling;
- phone gait, lift, extension, pitch, roll, and yaw;
- phone screen scale, offset, glow, emission, and local light;
- spring-based soul lattice deformation;
- soul pull, latch, seal, pressure, squeeze, pop, sag, and smear;
- soul-to-phone tether formation;
- enemy locomotion cadence, posture, imbalance, hit response, collapse, and shell thinning;
- camera interpolation and cinematic camera phases;
- controller rumble and platform haptics;
- material-aware particle motion, settling, rotation, and decay.

### 6.2 Interface population and reveal

Retain selectively:

- deterministic stencil-glyph reveal;
- per-glyph travel and restrained jitter;
- phone display-mode transition progress;
- screen color, emission, and lighting interpolation;
- short interaction impulses;
- selected-card tilt;
- view-model-driven phone layouts;
- typed join-code population;
- state-hashed phone-display caching.

### 6.3 World transformation

Treat as core identity primitives:

- captured-frame door transition;
- datamosh displacement;
- data-cell breakup and reconstruction;
- the data mosaic palette and grid language;
- gameplay-responsive scene lighting;
- secret-TV flicker and signal degradation;
- shell fragmentation into environment-like matter;
- room-dependent material and particle response.

### 6.4 Gameplay communication

Retain but rationalize:

- action-sensitive reticle appearance;
- reticle spread, convergence, hover, and spin;
- soul-storage mosaic;
- capture-point fill and completion response;
- energy ticker;
- battery and power feedback;
- communication signals;
- critical-hit field response;
- enemy critical markers where indispensable.

### 6.5 Deterministic population

Preserve the existing use of authoritative state to populate:

- room plans from seed and room index;
- environment setting, form, scale, condition, traversal, props, and lighting;
- enemy population and respawn placement;
- enemy individuality and movement response;
- capture goals and completion state;
- flower and power-up placement;
- bounded particle populations;
- soul lattice nodes;
- phone menu rows and values;
- phone display mode;
- environment lighting response.

## 7. Fundamental visual laws

1. **One fact, one dominant expression.** Supporting cues may reinforce it but shall not restate it with equal weight.
2. **Every persistent element earns its space.** Persistent UI must answer a recurring player question.
3. **Focus has one primary cue.** Selection should not simultaneously require a rail, dot, field, bright label, slider accent, and motion.
4. **Color has stable semantic meaning.** It should not be assigned merely to make adjacent pages different.
5. **Motion explains state, causality, or attention.** Motion without information is noise.
6. **Personality comes primarily from behavior.** Prefer posture, timing, response, material, sound, and transition over ornaments.
7. **Different controls look different.** Continuous, binary, navigational, destructive, and irreversible operations require distinct representations.
8. **Exact values appear only when useful.** A value, label, and meter shall not all repeat the same fact without purpose.
9. **Instructions are contextual.** Help appears at the moment of likely need and recedes after comprehension.
10. **Orientation must be earned.** Ordinary menus show no game branding or breadcrumbs. Add location context only after observed ambiguity cannot be solved through page structure or consistent back behavior.
11. **Back is consistent and quiet.** It remains predictable without competing with the page's purpose.
12. **Phone and screen form one object.** Display activity should affect the phone's material, light, motion, sound, or haptics when appropriate.
13. **Readability wins at decision time.** Expressive behavior must settle enough for reliable input.
14. **Specialness is earned.** Flicker, datamosh, palette cycling, strong pulse, and full-screen effects are reserved for meaningful states.
15. **A surface may have a distinct composition, but must speak the same grammar.** It should inherit state from the experience immediately before it.
16. **Transitions explain how states connect.** Avoid unexplained cuts to generic overlays.
17. **Procedural variation cannot alter meaning.** Individual timing may vary while semantic cues remain legible.
18. **Developer language is separate.** Diagnostic density and labels must not leak into ordinary player presentation.
19. **Input truth outranks animation.** Accepted input, focus, and enabled state must remain unambiguous throughout animation.
20. **Every visual change requires evidence.** Review it as a still, as an interaction, and in its actual world context.

## 8. Provisional design decisions

These decisions establish a coherent default and may be revised through playtest evidence:

- Baseline personality: a calm alien instrument that becomes corrupted or unstable under pressure.
- Selection: a restrained acquisition field plus typographic emphasis; no permanent selection rail by default.
- Page identity: ordinary menus contain no game branding or breadcrumbs and at most one primary title.
- Color: semantic and state-based, not category-based.
- Density: minimal by default, with detail disclosed contextually.
- Help: contextual and temporary.
- Controls: conventional and dependable.
- Feedback: distinctive, physical, and behaviorally expressive.

## 9. Semantic visual roles

Exact values may evolve, but the meanings shall remain stable:

| Role | Intended meaning | Current family candidate |
|---|---|---|
| Stable signal | available, connected, interpretable | electric cyan / metallic teal |
| Living or stored data | soul, stored potential, successful transfer | acid chartreuse |
| Hostile cognition | pursuit, threat, enemy commitment | electric magenta |
| Warning or damage | depletion, danger, damaged machinery | copper / warm gold |
| Mutable world data | compilation, transition, mixed state | data mosaic palette |
| Foreign transmission | intrusion, secret, unreliable source | degraded cyan/plum with constrained flicker |
| Disabled or unreachable | unavailable without implying danger | dim metallic teal |
| Destructive or final | irreversible termination | warning family with explicit shape/text support |

Color shall never be the only carrier of meaning.

## 10. Motion vocabulary

New animation should use an existing verb unless a new semantic state genuinely requires another:

- **Acquire:** an element becomes available or receives focus.
- **Tune:** a continuous value or signal is adjusted.
- **Lock:** a target or decision becomes committed.
- **Charge:** potential accumulates toward an action.
- **Store:** information settles into a retained structure.
- **Transfer:** information moves between world entities or device surfaces.
- **Compile:** world rules or structure are reconstructed.
- **Fracture:** a stable material or signal breaks into constituent data.
- **Desynchronize:** coordinated layers drift or fail under damage.
- **Reacquire:** a lost world, device, or player signal returns.

Flicker means damaged, foreign, unstable, or reacquiring signal. Datamosh means transformation between world states. Palette cycling means composite or uncontrolled data. Constant pulse means unresolved energy or attention. These effects should not be used as general decoration.

## 11. Experience continuity

### 11.1 Primary progression path

```text
signal initialization
    -> title
    -> world acquisition
    -> play and phone telemetry
    -> objective completion
    -> door recompilation
    -> upgrade/rule editing
    -> next world state
```

### 11.2 Pause path

```text
play
    -> phone raised and stabilized
    -> world audio and visual attention recede
    -> phone control surface
    -> phone lowers
    -> play resumes continuously
```

Solo and multiplayer pause may differ in authority constraints, but should not appear to belong to different games.

### 11.3 Failure path

```text
fatal event
    -> readable physical consequence
    -> brief time dilation
    -> body collapse
    -> phone separation
    -> signal desynchronization
    -> camera resolves toward fallen phone
    -> broken-screen AGAIN / END
    -> reacquisition or termination
```

## 12. Surface requirements

### 12.1 Title and initialization

- Preserve the world as contextual background where practical.
- Treat the title as signal initialization, not an unrelated logo animation.
- Reduce unconstrained palette cycling if it conflicts with semantic color.
- Transition into play through world acquisition rather than a hard interface disappearance.
- Input dismissal must be immediate and predictable.

### 12.2 Physical phone and menus

- Preserve the phone as the strongest diegetic interface surface.
- Do not show persistent game branding or breadcrumbs in ordinary menus; each page has at most one title.
- Use one dominant focus treatment.
- Represent continuous values as continuous controls and binary values as discrete states.
- Avoid repeating a value through text, percentage, meter, and decoration simultaneously.
- Allow display activity to affect screen light, device material, audio, and haptics without impairing readability.
- Maintain full keyboard and controller parity.

### 12.3 Gameplay telemetry

- Show only information needed for immediate decisions.
- Prefer world-anchored or phone-mediated expression where it remains readable.
- Avoid multiple simultaneous alert systems for the same event.
- Make temporary signals enter and leave cleanly.
- Keep the soul mosaic as a strong candidate for shared identity across HUD, phone, upgrades, and transitions.

### 12.4 Enemies

- Preserve cognition expressed through posture and movement.
- Use screen-space enemy markers only where body and world cues cannot provide timely information.
- Maintain readable anticipation, commitment, disruption, damage, and recovery phases.
- Let individuality vary cadence and asymmetry without changing the meaning of core cues.
- Prefer material and body response over floating labels.

### 12.5 Souls and capture

- Preserve lattice deformation and physical transfer as signature behavior.
- Ensure the phone, soul, tether, sound, and haptic response describe the same phase.
- Avoid effects that conceal the visible physical transfer.
- Reuse store and transfer behavior in progression presentation where appropriate.

### 12.6 Room completion and door transition

- Treat objective completion as a world-state change, not merely a HUD update.
- Preserve captured-frame datamosh and data-cell breakup.
- Let lighting, sound, HUD, and door readiness resolve in a deliberate order.
- The transition must visibly carry the previous room into rule selection.

### 12.7 Upgrade and progression

- Recompose the current generic overlay as rule editing during world recompilation.
- Reuse data cells, stencil acquisition, selection tilt, transfer, lock, and compile behavior.
- Distinguish temporary run changes from permanent progression through material stability and behavior, not labels alone.
- Keep comparison information readable and still while a decision is being made.
- Preserve existing progression authority and input behavior.

### 12.8 Damage and critical state

- Express damage as increasing desynchronization among world, phone, sound, and control feedback.
- Keep the causal threat readable.
- Reserve strong edge fields and signal interference for genuinely critical events.
- Provide reduced-flashing and reduced-motion equivalents.

### 12.9 Death and ending

- Base the sequence on the final physical cause and force direction.
- Use a short, weighty, restrained slow-motion collapse.
- Permit the phone to leave the player's control and remain spatially continuous.
- Resolve camera attention toward the same physical phone used during play.
- Reuse secret-TV damaged-signal and phone-display primitives for the broken screen.
- Present only `AGAIN` and `END` unless testing proves more information is required.
- Interpret `AGAIN` as signal reacquisition and `END` as intentional termination.
- Keep focus and input reliable even while the display flickers.
- Provide an abbreviated version after repeated deaths.
- Provide reduced-camera-motion, reduced-slow-motion, and reduced-flashing alternatives.
- Use "GTA-adjacent" only as a reference for physical weight and dramatic deceleration, never for copied branding or composition.

### 12.10 Developer presentation

- Preserve dense diagnostic views as a separate development layer.
- Diagnostic state may inspect semantic presentation values directly.
- Player UI should not inherit developer density merely because the data is available.
- Deterministic capture fixtures should remain first-class verification tools.

## 13. Visual behavior registry

The implementation should maintain a compact registry in documentation or tests before adding new effects:

| State | Primary expression | Supporting expression | Avoid |
|---|---|---|---|
| Menu selection | acquisition field | typographic emphasis | simultaneous rail, dot, glow, and repeated value |
| Enemy searching | asymmetric body scan | restrained signal response | permanent floating label |
| Enemy committed | forward lean and cadence | brief threat accent | generic constant pulse |
| Enemy disrupted | imbalance and recovery | material/hit response | unrelated screen flash |
| Soul attracted | directional lattice pull | tether emergence | opaque shell hiding deformation |
| Soul latched | seal, sag, and phone compression | audio/haptic lock | detached HUD notification |
| Soul stored | settling mosaic cell | confirmation cue | full-screen celebration |
| Room complete | world-light resolution | door readiness | text-only completion |
| Upgrade focus | rule acquisition | restrained card response | arcade-shop overlay behavior |
| Upgrade committed | data lock and compile | transfer sound/haptic | ambiguous disappearance |
| Critical damage | desynchronization | warning audio/haptic | decorative noise during ordinary play |
| Death | physical collapse | phone signal failure | generic detached game-over panel |
| Restart | signal reacquisition | reconstruction | unexplained hard reset |

## 14. Initial implementation sequence

### Phase A: Baseline and subtraction

1. Capture baseline title, phone pages, gameplay HUD, pause, upgrade, room transition, damage, and death states.
2. Record current input behavior for keyboard and controller.
3. Create an implementation-level visual behavior ledger from the registry above.
4. Remove redundant phone page titles, focus cues, value repetition, and misleading toggle tracks.
5. Preserve behavior and tests while simplifying presentation.

### Phase B: First vertical slice

Implement and validate:

```text
room completion -> door transition -> upgrade selection -> next room
```

This slice should reuse objective lighting, datamosh, data mosaic, stencil acquisition, selected-card response, and existing progression logic. It is the first proof that separate surfaces can share one visual system.

### Phase C: HUD and pause convergence

1. Classify each HUD element by the player question it answers.
2. Remove or demote redundant signals.
3. Connect HUD appearance and disappearance to phone/world behavior.
4. Reconcile solo and multiplayer pause composition without changing multiplayer authority.

Implemented first slice:

- Project persistent solo-play telemetry from authoritative `GameState` into a query-time phone display model.
- Render objective progress, charge, captured souls, tokens, and supplemental power on the physical phone during play.
- Remove their detached HUD duplicates while retaining reflex-critical reticle, threat, interaction, multiplayer, and spectator cues.
- Suppress generic menu navigation instructions; retain only contextual instructions for adjustment, rebinding, cycling, and toggling.
- Hold any phone-inspection gesture or readability enlargement until actual-scale playtesting demonstrates that it is needed.

### Phase D: Enemy and combat convergence

1. Validate cognition cues under real play conditions.
2. Reduce markers where body language is sufficient.
3. Align reticle, critical opportunity, hit response, lighting, audio, and haptics.
4. Preserve deterministic combat behavior.

### Phase E: Death and ending

1. Implement force-informed collapse and brief time dilation.
2. Add phone separation and camera resolution.
3. Reuse damaged-signal primitives on the fallen phone.
4. Implement reliable `AGAIN` / `END` interaction.
5. Add repeated-death and accessibility variants.

### Phase F: Complete playthrough review

Review the entire experience from initialization through progression, death, restart, and termination. Resolve remaining discontinuities rather than adding isolated polish.

## 15. Evidence requirements

Every meaningful visual change should provide:

- a representative still image;
- an interaction sequence or short capture when motion matters;
- an in-world capture at actual gameplay scale;
- keyboard and controller verification;
- before/after comparison;
- relevant automated test results;
- a statement identifying the governing visual law and authoritative source state;
- reduced-motion/flashing observation when applicable.

Evidence should be suitable for inclusion in a pull request and delivery through the established Brokeman mobile-evidence path.

## 16. Acceptance criteria

The visual-cohesion pass is successful when:

1. Title, play, phone, pause, upgrade, damage, death, and restart are recognizable as parts of the same game.
2. A player can identify focus and operate all menus without relying on redundant cues.
3. Continuous and binary settings are visually and behaviorally distinct.
4. Color roles remain semantically stable across surfaces.
5. Important animations explain real state changes.
6. Enemy intent remains readable through behavior under ordinary play conditions.
7. Room completion flows into upgrade selection without an unexplained visual break.
8. Death flows from physical gameplay into the fallen-phone choice without a detached overlay.
9. Keyboard and controller operation remain reliable throughout transitions.
10. Reduced-motion and reduced-flashing presentation preserves necessary information.
11. The authoritative checkout remains unaffected until explicit integration.
12. Automated tests, deterministic captures, and human visual review agree with the intended behavior.

## 17. Decision test

Before accepting a visual change, ask:

1. What authoritative state caused this?
2. What player question does it answer?
3. Is it the dominant expression or an unnecessary duplicate?
4. Which established semantic color and motion verb does it use?
5. How does it inherit from the immediately preceding state?
6. Does it remain readable and controllable in motion?
7. Does it still look like Data without relying on its text label?

If those questions do not have clear answers, the change is not ready.
