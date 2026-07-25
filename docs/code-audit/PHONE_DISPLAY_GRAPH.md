# Phone Display Graph

Audit date: 2026-07-25

## Current Authority

- `PhoneDisplay.hpp` owns persistent phone display state: mode, brightness,
  tint, pulse, material, light, and screen noise.
- `PhoneMenuModel.hpp` owns menu semantics: page titles, rows, selectable count,
  actions, binding ids, and settings values.
- `PhoneDisplayLayout.hpp` owns phone-menu geometry in logical texture pixels:
  safe bounds, header/content/footer regions, row rectangles, hit rectangles,
  baselines, text columns, and `phoneDisplayItemAt()`.
- `native-desktop/main.cpp` owns desktop input and menu activation. Mouse
  hit-testing raycasts to the physical phone screen and then asks
  `PhoneDisplayLayout` for the selected row.
- `native-desktop/DesktopRenderer.cpp` owns current texture rasterization and
  OpenGL upload for the physical phone display.

## Corrected Boundary

Gameplay-critical HUD data remains in the desktop HUD path. The phone display no
longer presents readable gameplay status such as room number, soul count, goal
count, target lock, or battery labels during active gameplay. Those values are
still available to gameplay and HUD systems; they are not duplicated onto the
phone screen as an alternate authoritative HUD.

During active gameplay, the physical phone screen now draws only a restrained,
ambient, reactive mosaic and subtle lines driven by phone/display activity. This
keeps the object alive without making it a second dashboard.

## Physical Registration

Before this pass, desktop rendering drew:

- a fallback emissive screen box at `state.phoneTransform.screenCenter`
- a textured display quad at `state.phoneTransform.screenCenter +
  screenNormal * 0.023`

Shared constants are:

- `PHONE_SCREEN_WIDTH = 0.07`
- `PHONE_SCREEN_HEIGHT = 0.125`
- `PHONE_SCREEN_DEPTH = 0.002`
- `PHONE_SCREEN_Z_OFFSET = 0.007`

The `0.023` textured-quad lift was much larger than the fallback screen depth and
made the display read as a floating rectangle in front of the model. This pass
replaced it with `PHONE_SCREEN_DEPTH * 0.75`, which keeps the textured display
just in front of the physical screen box while staying tied to the shared phone
screen contract. A future loaded-model material registration pass can still
replace the fallback screen box entirely when named screen materials are exposed.

## TV Boundary

The secret-room TV remains separate. This audit intentionally does not introduce
TV classes, move TV rendering into the phone-display path, or generalize both
objects into a premature "screen system."

Graph files:

- `docs/code-audit/graphs/phone-display-current.dot`
- `docs/code-audit/graphs/phone-display-target.dot`
