# State Ownership

Audit date: 2026-07-25

## Gameplay State

Owner: `Game.hpp` / `Game.cpp`

Examples:

- player movement, camera, enemies, targets, soul capture, battery, room
  progression, secret room state, multiplayer state, and phone transform.

Rules:

- Gameplay state can feed HUD, audio, renderer, multiplayer, and phone display.
- The phone display should not become the owner of gameplay truth.

## Authoritative Gameplay HUD

Owner: desktop HUD rendering in `DesktopRenderer.cpp`

Examples:

- room number
- goals filled/required
- door state
- token count
- battery meter
- souls/minimap display
- target/lock readability
- FPS/debug display when enabled

Rules:

- These remain readable in the normal player HUD.
- They should not be silently moved to the phone surface unless the design makes
  the phone itself the intended gameplay interaction.

## Phone Menu State

Semantic owner: `PhoneMenuModel.hpp`

Geometry owner: `PhoneDisplayLayout.hpp`

Activation/input owner: `native-desktop/main.cpp`

Rules:

- Menu rows should be selected by semantic `PhoneMenuAction`.
- Noninteractive section labels do not receive selectable indices or hit regions.
- Mouse hit-testing must raycast to the physical phone screen and resolve through
  `PhoneDisplayLayout`; no independent projected-row geometry should exist.

## Phone Display State

Owner: `PhoneDisplay.hpp` / `Game.cpp`

Renderer: current desktop implementation in `DesktopRenderer.cpp`

Rules:

- Menus, death restart, boot/title presentation, and phone-native ambience belong
  on the phone display.
- Active gameplay phone content may react to gameplay, but it should stay
  cosmetic or diegetic unless promoted deliberately into gameplay design.

## Secret-Room TV

Owner: current secret TV state/rendering paths

Rules:

- TV GIF playback and TV-room behavior remain independent from the player phone.
- Do not generalize TV and phone into a shared screen abstraction until both
  object contracts are stable.
