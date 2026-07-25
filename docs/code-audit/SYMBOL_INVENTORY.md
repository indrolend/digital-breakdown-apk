# Symbol Inventory

Audit date: 2026-07-25

## Keep

- `PhoneDisplayState`, `PhoneDisplayMode`, `PhoneDisplayMaterialState`,
  `PhoneDisplayLightingState`: persistent physical phone-display state.
- `updatePhoneDisplayState()`: shared game-side display state evolution.
- `PhoneMenuPageViewModel`, `PhoneMenuElement`, `PhoneMenuAction`,
  `PhoneMenuRowKind`: semantic phone-menu model.
- `PhoneDisplayMenuLayout`, `PhoneDisplayMenuRow`, `PhoneDisplayRect`,
  `makePhoneDisplayMenuLayout()`, `phoneDisplayRowForSelection()`,
  `phoneDisplayItemAt()`: single geometry and hit-test authority for physical
  phone menus.
- `renderPhoneDisplayPixels()`: current CPU rasterization entry point for phone
  texture content. It should eventually move out of `DesktopRenderer.cpp`, but it
  remains live.
- `phoneDisplayRenderKey()`: current texture-cache invalidation key. It now
  hashes visible phone content, not removed gameplay HUD fields.
- `drawSecretTvScreen()`: separate TV rendering path; deliberately unchanged.
- `drawHud()`: authoritative desktop gameplay HUD path; deliberately preserved.

## Removed In This Pass

- `PhoneMenuLayout.hpp`: obsolete parallel phone-menu geometry model. It was no
  longer referenced by production desktop or Android source after routing tests
  and activation through `PhoneDisplayLayout`/`PhoneMenuModel`.

## Already Removed Before This Pass

The following legacy world-space menu renderer symbols are no longer present in
current production source:

- `drawPhoneMenuSurface`
- `phoneScreenText`
- `phoneScreenPaletteText`
- `phoneScreenBitmapText`
- `phoneScreenQuad`
- `phoneScreenTexturedQuad`
- `projectWorldToFramebuffer`

Historical docs still mention these symbols as migration context.

## Extract Later

- Source Sans CPU font loading and text rasterization currently live inside
  `DesktopRenderer.cpp`. They should be extracted into a small phone-display
  composition module after this authority correction is stable.
- Phone display texture cache storage currently lives in `DesktopRenderer`.
  Extraction is useful, but not required to prove the current ownership boundary.
