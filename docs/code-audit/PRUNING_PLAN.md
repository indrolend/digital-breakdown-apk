# Pruning Plan

Audit date: 2026-07-25

## Completed In This Pass

- Removed the obsolete `PhoneMenuLayout.hpp` parallel geometry model.
- Updated `PhoneMenuLayoutTest` to test `PhoneDisplayLayout.hpp` while retaining
  the target name used by existing verification scripts.
- Routed desktop menu activation away from old row geometry and through
  `PhoneMenuModel` semantic actions.
- Kept mouse hit-testing on the physical phone screen and resolved row selection
  through `PhoneDisplayLayout`.
- Removed active-gameplay readable HUD data from the phone texture.
- Removed stale phone-as-HUD screenshot evidence.
- Reduced phone-display cache dependencies to visible phone texture content.
- Corrected the textured phone display offset from a freehand `0.023` world-unit
  lift to `PHONE_SCREEN_DEPTH * 0.75`.

## Defer Until Measured

- Replace the fallback screen box with registered loaded-model screen materials
  when that mapping is proven reliable.
- Extract phone texture composition from `DesktopRenderer.cpp` into a small
  renderer helper. This should be done after visual behavior stabilizes so the
  extraction is mechanical.
- Unify CPU text metrics used by layout and rasterization if future text fitting
  needs exact Source Sans width.

## Do Not Prune In This Pass

- Gameplay HUD rendering.
- TV GIF rendering.
- Secret-room TV behavior.
- Android renderer files.
- Bitmap HUD font fallback.
- Multiplayer or protocol paths.
