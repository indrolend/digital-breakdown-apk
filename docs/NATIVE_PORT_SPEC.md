# Digital Breakdown Native Port Spec

This document defines the first native-port target for Digital Breakdown. It is intentionally scoped to the main/basic runtime environment only. The chicken/ant populated environment, model-heavy city layer, and full PC asset payload are not part of the first native target.

## 0. Current source of truth

The current repo contains a working Capacitor/WebView Android APK using `www/android-entry.mjs` bundled through esbuild into `www/android-bundle.js`.

Known-good Android/WebView baseline:

```text
[STYLO-V2] boot
[STYLO-V2] room reset
[STYLO-V2] READY
[STYLO-V2] RUNNING
```

The native port must not overwrite or replace the known-good WebView runtime at the start. The WebView runtime remains the behavior reference and fallback implementation.

## 1. First native-port scope

### Included

The first native version targets one simple level/runtime form:

- phone player
- main/basic room or arena
- floor/walls/bounds
- primitive targets
- primitive soul/capture interaction
- battery system
- movement, sprint, jump
- vacuum/capture behavior
- stored souls
- capture/deposit points
- room clear/reset/progression counter
- simple third-person camera
- debug overlay/logging
- Stylo 4 low profile

### Excluded for first native target

These are deferred:

- chicken populated environment
- ant model pipeline
- chicken avatar animation pipeline
- park bench FBX/GLB import
- heavy PC `embedded-assets.js`
- FBX loading
- full PC audio preload
- datamosh/portal visual effect
- PC graphics settings panel
- DOM HUD/control panels
- dynamic shadows
- postprocessing
- mesh collision
- ragdoll/soft body/full physics engine
- multiplayer networking

The first native target is not the full PC browser game. It is the native equivalent of the basic Stylo runtime.

## 2. Porting principle

Do not translate JavaScript line-by-line.

Translate the computational contract:

```text
JS state/rules/constants
-> native C++ structs/functions
-> Android-native input/audio/render/platform layer
```

The browser game is mostly custom vanilla Three.js. That makes manual architectural porting feasible because the gameplay rules are visible and not hidden inside Unity/Unreal/editor-specific state.

The native target should port:

- constants
- state machines
- movement math
- vacuum/capture math
- battery math
- room rules
- camera smoothing
- interaction timing

The native target should replace:

- Three.js scene graph
- Three.js geometry/material classes
- browser DOM UI
- Web Audio helpers
- WebView asset loading
- embedded JS asset bundles

## 3. What Three.js/browser currently hides

The native runtime must explicitly replace the following services that the browser/Three.js currently provide.

| Browser / Three.js service | Native replacement |
|---|---|
| `requestAnimationFrame` | Android render loop + fixed/capped simulation tick |
| `THREE.Clock.getDelta()` | native timer / monotonic clock |
| `THREE.Vector3` | `Vec3` struct |
| `THREE.Matrix4` / transform updates | native transform + matrix utilities |
| `THREE.Group` | lightweight node/transform or direct render-instance list |
| `THREE.Mesh` | native render object / mesh handle |
| `BoxGeometry` | hardcoded cube/prism primitive mesh |
| `CylinderGeometry` | generated low-segment ring/cylinder primitive |
| `SphereGeometry` | optional low-poly sphere/icosphere primitive |
| `MeshBasicMaterial` | flat-color shader/material |
| `MeshStandardMaterial` | deferred; first pass should not need PBR |
| `InstancedMesh` | native instance buffer or fixed render-instance pool |
| `PerspectiveCamera` | native camera state + projection/view matrices |
| WebGLRenderer | OpenGL ES renderer |
| DOM keyboard/pointer/touch | Android input mapped to `InputIntent` |
| Web Audio / HTML audio | event-driven native audio layer, Oboe/miniaudio later |
| DOM HUD | native debug overlay or logcat-first debug |
| JS arrays/objects | fixed arrays/structs/pools |
| JS garbage collection | explicit object lifetime and pools |
| browser asset URLs | Android asset manager / packed assets |
| FBX/GLTF loaders | deferred GLB path; no FBX in first target |

## 4. Native architecture layers

```text
native/
  core/
    math
    input_intent
    sim_constants
    world_state
    simulation
    collision
    fixed_tick

  render_gles/
    renderer
    shader_program
    mesh
    material
    primitive_builder
    camera
    debug_draw

  platform_android/
    activity_bridge
    asset_io
    touch_input
    controller_input
    lifecycle
    logging
    timing

  audio/
    audio_events
    cue_registry
    backend_later

  docs/
    port_notes
```

For Stage 1, only the spec exists. For Stage 2, start with `native/core` before Android/rendering.

## 5. Core simulation data model

The first C++ core should use plain structs and fixed-size arrays. Avoid engine-style inheritance at the start.

```cpp
struct Vec3 {
    float x;
    float y;
    float z;
};

struct InputIntent {
    float moveX;
    float moveZ;
    float lookX;
    float lookY;

    bool jump;
    bool sprint;
    bool vacuum;
    bool attack;
    bool discharge;
    bool switchMode;
    bool toggleCamera;
};

struct PlayerState {
    Vec3 pos;
    Vec3 vel;
    float yaw;
    float targetYaw;
    float battery;
    int souls;
    bool grounded;
    bool alive;
};

enum class TargetKind {
    Basic
};

enum class TargetLifecycle {
    Alive,
    Attracted,
    Latched,
    Captured,
    Stored,
    Dead
};

struct TargetState {
    Vec3 pos;
    Vec3 vel;
    TargetKind kind;
    TargetLifecycle lifecycle;
    float captureProgress;
    float phase;
    bool alive;
};

struct CapturePointState {
    Vec3 pos;
    bool filled;
};

struct RoomState {
    int roomIndex;
    bool clear;
    int requiredCaptures;
};

struct WorldState {
    PlayerState player;
    TargetState targets[5];
    CapturePointState captures[5];
    int targetCount;
    int captureCount;
    RoomState room;
};
```

## 6. Initial constants contract

Use the current Stylo/WebView runtime as the first native baseline, not the PC build.

```cpp
struct SimConstants {
    float roomWidth = 30.0f;
    float roomDepth = 42.0f;
    float wallHeight = 7.2f;

    int activeTargets = 5;
    int maxStoredSouls = 5;
    int capturePoints = 3; // match current Stylo V2 first; can move to 5 later

    float walkSpeed = 7.0f;
    float sprintSpeed = 11.5f;
    float accel = 34.0f;
    float airAccel = 13.0f;
    float friction = 13.0f;
    float gravity = 24.0f;
    float jumpSpeed = 9.2f;

    float batteryMax = 100.0f;
    float batteryIdleRegen = 22.0f;
    float batteryActiveRegen = 3.0f;
    float batteryWalkDrain = 0.45f;
    float batterySprintDrain = 3.0f;
    float batteryAirDrain = 0.9f;
    float batteryVacuumDrain = 1.35f;
    float batteryJumpCost = 4.5f;
    float batteryCaptureGain = 6.0f;

    float vacuumRange = 6.0f;
    float vacuumLatchRadius = 1.0f;
    float vacuumPull = 14.0f;
    float vacuumCaptureTime = 0.55f;
    float vacuumMoveMult = 0.35f;

    float captureRadius = 1.75f;
};
```

PC constants such as `TARGET_COUNT = 32`, `PARTICLE_COUNT = 256`, city/chicken/ant settings, and powerup systems are deferred until after native Stylo V1 reaches parity with the current Stylo WebView runtime.

## 7. Fixed/capped time model

Gameplay must not depend on render frame rate.

Use a fixed simulation tick first:

```cpp
constexpr float FIXED_DT = 1.0f / 30.0f;
```

Main rule:

```text
render FPS can vary;
simulation rules should not vary with render FPS.
```

This protects:

- movement speed
- jump arc
- battery drain/regen
- vacuum pull
- capture progress
- attack cooldowns later
- projectile behavior later
- multiplayer fairness later

## 8. First simulation functions

Stage 2 should implement the following pure C++ functions with no Android and no renderer dependency:

```cpp
void resetWorld(WorldState& world, const SimConstants& c);
void updateWorld(WorldState& world, const InputIntent& input, const SimConstants& c, float dt);
void updatePlayer(WorldState& world, const InputIntent& input, const SimConstants& c, float dt);
void updateTargets(WorldState& world, const InputIntent& input, const SimConstants& c, float dt);
void updateCaptures(WorldState& world, const SimConstants& c, float dt);
void clampToRoom(Vec3& pos, const SimConstants& c);
```

`updateWorld` order:

```text
1. updatePlayer
2. updateTargets
3. updateCaptures
4. update room clear/reset state
```

## 9. Collision rules for first native target

Do not port mesh collision.

Use:

- room bounds clamp
- target/player XZ distance checks
- capture point circle radius
- optional AABB obstacles later
- optional ray/hitscan later

First target collision shapes:

```text
Player: capsule-ish or simple cylinder/sphere radius
Targets: sphere/cylinder radius
Capture point: XZ circle
Room: AABB bounds
```

## 10. Renderer redesign

First native renderer target:

- OpenGL ES
- one flat-color shader
- no textures
- no lighting required at first
- no dynamic shadows
- no postprocessing
- no PBR
- no model loading
- fixed small primitive mesh set

Primitive assets:

```text
phone body: box
phone screen: box/plane
floor: plane
walls: boxes/line boxes
target: two boxes
capture point: low-segment ring/cylinder
soul: small cube
```

The native render layer should receive state from `WorldState`. It should not own gameplay state.

## 11. Asset pipeline redesign

First native target uses hardcoded primitives.

Deferred asset order:

```text
1. hardcoded primitives
2. simple custom mesh struct if needed
3. small GLB models
4. texture atlas
5. KTX2/Basis texture compression
6. optional animation
```

Avoid in first native target:

- FBX
- embedded base64/JS assets
- full PC model set
- heavy music preload
- runtime model conversion on Stylo 4

## 12. Audio redesign

First native target can run without audio.

When added, audio must be event-driven:

```text
capture
store soul
deposit
room clear
jump
vacuum loop start/stop
low battery
hit/damage later
```

No full preload for first pass. No browser audio pool translation. Use a small cue registry and add native backend later.

## 13. Input redesign

All platform input must map to `InputIntent`.

Input sources later:

- keyboard/debug
- touch controls
- controller/gamepad

Game code must not care which physical input source produced the intent.

## 14. Debug/profiling redesign

First debug outputs:

```text
profile name
frame FPS
sim tick count
player position
battery
souls
targets alive
capture points filled
room index
```

Native version should log to Android logcat first. On-screen debug overlay can come after primitive rendering.

## 15. Stage plan

### Stage 1: Spec and scope

Current document. No runtime change.

### Stage 2: Pure C++ core skeleton

Create native core files and a smoke-test style simulation entry. No Android, no rendering.

### Stage 3: Native Android shell

Create minimal Android/NDK/GameActivity or compatible native shell without replacing the WebView APK yet.

### Stage 4: Primitive OpenGL ES renderer

Render floor, room, phone, targets, captures from `WorldState`.

### Stage 5: Stylo V2 parity

Native primitive game reaches gameplay parity with current WebView Stylo runtime.

### Stage 6: Add selected PC behavior

Only after parity:

- first-person camera
- camera smoothing improvements
- room/door loop if still wanted
- attack/discharge
- primitive city dressing if needed
- selected audio

### Stage 7: Asset upgrades

Only after primitive native version is stable on Stylo 4.

## 16. Non-negotiable constraints

- Preserve known-good WebView APK.
- Native work must be staged separately.
- No direct PC asset payload import.
- No full chicken/ant environment first.
- No FBX-first native pipeline.
- No broad untested patch stack.
- Simulation must be separable from rendering.
- Stylo 4 profile is the first hardware truth target.
