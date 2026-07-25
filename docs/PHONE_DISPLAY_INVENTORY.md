# Phone Display Inventory

This document is the audit checkpoint for the persistent phone display work. It
records the current browser contract, native implementation, duplicated paths,
and migration targets before structural renderer edits.

## Reference Contract

The browser Pass 7 runtime treats the phone screen as a physical part of the
phone, not as a disconnected UI overlay.

- `reference/browser-pass7/index.html` creates a fallback screen plane as a child
  of `phone`: a `PlaneGeometry(0.07, 0.125)` at local `z = 0.007`.
- The fallback screen uses a dark blue material with cyan-blue emissive color,
  transparency, and depth-write disabled.
- `screenLight` is a `RectAreaLight` parented to `screen`, so the light follows
  the phone transform automatically.
- Loaded GLB phone materials are traversed once on load. Materials whose names
  include `Screen_BG`, `Screen_Glass`, or `Screen_Rim` are cloned and pushed into
  `phoneScreenMaterials`.
- `setPhoneScreenEmission(color, intensity)` applies one emission color/intensity
  path to the fallback `screen` and all registered loaded-phone screen materials.
- Initial loaded-phone role handling already distinguishes background from the
  glass/rim by intensity: `Screen_BG` starts stronger than the other screen
  materials.
- `updateScreenFlash()` temporarily raises screen emission and `screenLight`
  intensity, then decays back to dark cyan-blue.

Browser paths that are intentionally unfinished separation:

- Browser HUD/menu surfaces remain mostly DOM/canvas overlays, not physical phone
  screen content.
- Browser `flashScreen()` is a global gameplay cue source; the eventual native
  display system should convert comparable events into display state pulses
  rather than copying browser DOM overlay behavior.

## Current Native Phone-Screen Content Renderers

- `DesktopRenderer::draw()` draws the loaded phone model through
  `drawStaticModel(phoneModelList_, ...)` or a fallback box, then draws a simple
  emissive screen box at `state.phoneTransform.screenCenter`.
- `drawPhoneMenuSurface()` draws menu content directly into world space using
  `phoneScreenText()`, `phoneScreenPaletteText()`, and `phoneScreenQuad()`.
- `phoneScreenQuad()` emits an immediate-mode world-space quad offset from
  `PhoneTransformState::screenCenter/right/up/normal`.
- `phoneScreenTexturedQuad()` emits individual textured glyph quads in world
  space against the phone screen plane.
- `drawSecretTvScreen()` is a separate texture path for the secret-room TV, not
  the player phone, but it is a useful model for native texture upload and
  screen-like rendering.
- `drawHud()` renders world HUD, upgrade menu, crosshair, meters, FPS, and other
  overlay text directly in framebuffer space.

Migration target:

- Phone menus, phone images, phone meters, phone warnings, phone death/restart
  text, and future phone effects should render into one persistent phone display
  texture.
- World HUD, crosshair, FPS counter, multiplayer debug/status overlays, and
  non-phone world markers should remain framebuffer/world-view overlays unless a
  feature explicitly belongs on the phone screen.

## Current Font Renderers

- `MenuFontAtlas` in `DesktopRenderer.cpp` loads Source Sans 3 Regular and
  Semibold with `stb_truetype`.
- `phoneTextWidth()` uses baked glyph `xadvance` when Source Sans is loaded, but
  `PhoneMenuLayout` still uses character-count width approximation for fit
  decisions.
- `phoneScreenText()` renders Source Sans glyphs as individual world-space quads.
- `phoneScreenBitmapText()` and `BitmapFont.hpp` provide a fixed 5x7 fallback
  font for phone text.
- `drawHud()` has an independent framebuffer-space bitmap-font lambda for HUD and
  upgrade menu text.

Transitional or duplicated:

- Source Sans rendering is menu-only and world-space.
- Bitmap font exists both as a phone fallback and as the active HUD text path.
- Layout width and renderer width do not share one authoritative font metrics
  contract yet.

## Current Phone Material And Emission Paths

- Browser reference has explicit roles: `Screen_BG`, `Screen_Glass`,
  `Screen_Rim`, plus fallback screen plane and attached light.
- Native `StaticModelData`/`compileStaticModel()` currently collapses loaded model
  batches into display lists with baked colors only; it does not preserve named
  screen-role material handles.
- Native fallback phone draws a body box and separate screen box.
- Native loaded phone draws the whole phone model and then still draws an
  additional screen box, so loaded/fallback screen ownership is not unified.
- Native screen color is currently a direct draw color derived from
  `Pass7Visual::PhoneEmission` and `PhoneVisualState::screenGlow`.
- Native has no phone-attached screen light. Scene lighting is fixed OpenGL
  lighting plus planar shadows.

Obsolete target:

- Applying identical color treatment to every loaded phone batch is not enough
  for the display contract. Native needs screen role metadata at model-load time
  and per-role material response at draw time.

## Current Coordinate Systems

- Phone physical units are in world scale constants:
  `PHONE_BODY_WIDTH`, `PHONE_BODY_HEIGHT`, `PHONE_BODY_DEPTH`,
  `PHONE_SCREEN_WIDTH`, `PHONE_SCREEN_HEIGHT`, and `PHONE_SCREEN_Z_OFFSET`.
- `PhoneTransformState` stores the phone world position, orientation, screen
  center, screen right/up/normal basis, and vacuum pull point.
- `PhoneMenuLayout` computes screen positions in phone-world units relative to
  `PHONE_SCREEN_WIDTH` and `PHONE_SCREEN_HEIGHT`.
- `drawPhoneMenuSurface()` consumes the phone-world layout directly.
- `drawHud()` uses a Retina-aware framebuffer logical canvas.
- `menuItemAt()` converts window coordinates to framebuffer coordinates and then
  to the HUD logical canvas for overlay hit testing.
- `phoneMenuItemAt()` projects phone-row world-space corners into framebuffer
  rectangles for mouse hit testing.

Migration target:

- Phone display layout should use fixed portrait logical pixels, starting at
  720x1280 unless the modeled screen aspect requires a different exact value.
- Camera distance and viewport size should change only the physical phone
  presentation, not phone display layout.

## Current Pointer And Touch Hit Tests

- Desktop mouse:
  - `menuItemAt()` handles upgrade-menu framebuffer rectangles.
  - `phoneMenuItemAt()` handles phone menus by projecting row corners into the
    framebuffer.
  - `mouseButtonCallback()` activates `hud.menuSelection` through
    `activateMenuSelection()`.
- Keyboard:
  - `keyCallback()` changes `hud.menuSelection`, activates menu actions, handles
    join-code text entry, and starts rebinding.
- Controller:
  - `pollGamepad()` maps gamepad input, menu navigation, and confirm/back.
  - Menu navigation still consumes semantic item counts, but actions are not yet
    fully routed through a separate menu controller.
- Android/touch shared game code:
  - `setTouch()` only tracks raw diagnostic touch coordinates.
  - `setTouchControls()` owns mobile gameplay input roles.
  - There is not yet a shared ray/UV/logical-pixel phone interaction contract.

Obsolete target:

- Projected row-corner hit testing should be replaced with ray-to-screen-plane
  intersection, UV conversion, and logical display hit regions.

## Current Display-Related State

- `GameState::phoneTransform` is the physical screen/body transform authority.
- `GameState::phoneVisual` stores screen scale, glow, screen offset, phone pose
  offsets, pitch, roll, and gameplay action response.
- `GameState::hud.menuSelection` is the current selectable menu index.
- `GameState::localSettings.menuPage` and `controlsPage` select the active phone
  menu page.
- `GameState::started`, `dead`, `uiPaused`, `cinematic.introActive`, and
  `upgradeMenu.active` determine whether menu presentation is visible.
- `CinematicState` stores intro/death/menu-exit camera timing and text
  interaction.
- `HudState` stores phone-adjacent or overlay metrics such as battery fill, souls,
  goal fill, low-battery state, crosshair, headshot pulses, and build label.
- `AudioState` and event cues indirectly drive future display pulse candidates:
  damage, capture, low battery, headshot/reward, game over, and secret TV cues.

Missing target:

- There is no `PhoneDisplayState`, `PhoneDisplayMode`, retained frame, brightness,
  material state, lighting state, black level, noise phase, or display pulse
  accumulator.

## Current Menu Pages And Non-Menu Screen Behaviors

Current `PhoneMenuPageViewModel` pages:

- Boot/intro: `DATA` with `Start`.
- Main menu: `DATA`, `Solo`, `Online`, `Settings`, `Exit`.
- Online: `Host`, `Join`, `Back`.
- Join Code: code entry display.
- Settings: `Controls`, `Audio`, `Graphics`, `Back`.
- Controls 1/2: move bindings plus `Next` and `Back`.
- Controls 2/2: action bindings, look sensitivity, `Previous`, `Defaults`,
  `Back`.
- Audio: music/SFX values and mutes.
- Graphics: preset, shadows, particles, FPS, back.
- Pause: `PAUSED`, `Resume`, `Controls`, `Audio`, `Graphics`, `Exit Run`.
- Death: single centered `Again?` action.

Other screen-related behaviors:

- Gameplay phone screen is currently just a glowing physical rectangle.
- Pause swaps into the same phone menu renderer, but does not retain gameplay
  display content.
- Death now presents `Again?` through the same phone menu renderer, but does not
  retain or decay a previous gameplay display texture.
- Upgrade menu is still a framebuffer overlay and not phone-display content.
- Low-battery, damage, capture, discharge, vacuum, and powerup feedback exist as
  game/HUD/audio state but are not unified into phone display emission state.

## UI Surfaces That Should Migrate To Persistent Phone Display

- Main, online, join-code, settings, controls, audio, graphics, pause, death, and
  restart screen content.
- Future gameplay phone content: battery/status, capture feedback, warning
  pulses, phone images/icons/meters, retained pause frame, retained death frame,
  and boot/restarting visuals.
- Screen emission, glass/rim response, screen-local light intensity, and
  transient display pulses.

## UI Surfaces That Should Remain World Or Framebuffer Overlays

- Crosshair and aiming indicators that live in the camera view.
- FPS counter/debug text.
- Upgrade selection overlay until a separate design decision moves upgrades onto
  the phone.
- Multiplayer connection debug/status that is not intended to appear on the
  physical phone.
- World labels/cues tied to objects or enemies rather than the player phone.
- Secret-room TV rendering, which should stay its own in-world screen object.

## Duplicated, Transitional, And Obsolete Paths

Duplicated:

- Menu page model and layout are shared, but activation still mixes semantic
  actions and selection-index branches in `main.cpp`.
- Source Sans metrics are available in the renderer, but layout still relies on
  character-count approximation.
- Native loaded-phone and fallback-phone screen handling do not share browser-like
  named material roles.

Transitional:

- `PhoneMenuModel.hpp` is a useful semantic model and should feed the future
  `PhoneDisplayLayout`.
- `PhoneMenuLayout.hpp` is useful as a temporary shared geometry model but should
  become logical-pixel display layout.
- `phoneScreenText()`, `phoneScreenPaletteText()`, and `phoneScreenQuad()` are
  good behavior references, but they should be replaced by a display canvas and
  texture renderer.
- `phoneMenuItemAt()` keeps current click behavior coherent, but it should be
  replaced by screen-object interaction.

Obsolete after migration:

- World-space menu glyph rendering.
- Character-count menu fit logic.
- Projected row-corner hit testing.
- Additional loaded-phone screen box if the loaded model exposes a usable
  `Screen_BG` role.
- Any per-mode phone UI renderer separate from the persistent display object.

## Recommended Next Implementation Slice

The next safe code slice is display foundation, not full menu migration:

1. Add shared `PhoneDisplayMode` and `PhoneDisplayState` to game-side code.
2. Derive display mode from existing state without changing behavior.
3. Add bounded emission/material/light values and tests for finite clamped output.
4. Add a native `PhoneDisplayRenderer` that can render a fixed test frame to an
   off-screen texture.
5. Apply that texture to the fallback screen path first while preserving the
   existing menu renderer until the texture path is verified.

This keeps gameplay and current menus stable while creating the permanent display
object that later phases can migrate into.
