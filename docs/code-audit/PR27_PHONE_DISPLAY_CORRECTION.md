# PR 27 Phone Display Correction

Date: 2026-07-25

## Reference Context

Inspected local SPA references for route/item navigation patterns:

- `/Users/joelgutierrez/basicbrowserslim/js/spa/navModel.js`
- `/Users/joelgutierrez/basicbrowserslim/js/spa/appKernel.js`
- `/Users/joelgutierrez/basic-browser-spa/js/spa/router.js`
- `/Users/joelgutierrez/basic-browser-spa/js/spa/routes.js`
- `/Users/joelgutierrez/basic-browser-spa/main.js`

Useful transfer: preserve a small route stack and selected item per route, while
keeping visual transition state separate from semantic route state.

## Implemented

- Phone menu page changes now flow through `pushMenuPage`, `popMenuPage`, and
  `openMenuRoot` in `native-desktop/main.cpp`.
- `PhoneDisplayLayout.hpp` remains the shared geometry authority for renderer
  rows and pointer hit-testing.
- Controls is one grouped scrollable page with nonselectable section labels and
  aligned label/value columns.
- Menu left/right now adjust or toggle the current row only; up/down owns row
  movement.
- The phone-menu camera frames the phone at roughly 80% viewport height.
- Menu presentation keeps only a quiet phone shadow, suppressing gameplay
  shadow clutter.
- Death no longer renders an interactive phone menu. The phone display turns
  off, while `Again?` and `Quit` are rendered and hit-tested by the HUD layer.
- Critical headshot feedback shares one electric-magenta pulse across phone
  tint/light, target marker tint, and the wispy perimeter wash.

## Safe Area

The logical phone display uses:

- horizontal safe margin: 12%
- top safe margin: 12%
- bottom safe margin: 16%
- header zone: 16%
- footer zone: 12%

Rows draw only when fully inside the content window, which prevents scrolled
Controls rows from passing under the fixed title or bottom hardware.

## Capture Evidence

Fresh captures from the rebuilt macOS app bundle:

- `artifacts/pr27-menu-pass-final2/main.png`
- `artifacts/pr27-menu-pass-final2/pause.png`
- `artifacts/pr27-menu-pass-final2/controls-top.png`
- `artifacts/pr27-menu-pass-final2/controls-bottom.png`
- `artifacts/pr27-menu-pass-final2/audio.png`

Capture command pattern:

```sh
build/native-desktop-mac-current/bin/DigitalBreakdown.app/Contents/MacOS/DigitalBreakdown --capture-menu-frame artifacts/pr27-menu-pass-final2/main.ppm --menu-page main
```

## Remaining Visual-System Work

1. Add an explicit scroll position indicator for long phone pages if playtesting
   shows users miss lower Controls rows.
2. Promote route/history helpers into a tiny shared header if Android/native
   shell menus need the same navigation stack.
3. Replace the fallback phone screen box with named material registration when
   the loaded phone model exposes stable screen material ids.
4. Add a direct pointer-hit regression for the detached death overlay.
