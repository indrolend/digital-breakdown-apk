# Phone Menu Layout

The phone-bound menus use `PhoneMenuModel.hpp` for semantic rows and
`PhoneDisplayLayout.hpp` as their shared physical phone-screen geometry source.
The desktop renderer and pointer hit-testing both consume the same calculated
display rows so visual selection markers and clickable regions cannot drift
apart.

The visible phone screen is divided into an inner safe rectangle, a title/header
zone, a content zone, and a footer zone. Current proportions are:

- Horizontal safe margin: 11% of visible screen width on each side.
- Top safe margin: 9.5% of visible screen height.
- Bottom safe margin: 9.5% of visible screen height.
- Header/title zone: 15% of visible screen height.
- Footer zone: 12% of visible screen height.

Menus render directly on the phone screen material instead of on an opaque panel.
Desktop menu text uses bundled Source Sans 3 Regular and Semibold through
`stb_truetype`, with the bitmap font kept as a runtime fallback. Short pages use
one vertical navigation column with a compact cyan selection marker and brighter
selected text. Dense pages use the same safe rectangle, but request the denser
row spacing token and aligned label/value columns. Controls is split into two
pages so row scale stays legible and every row remains inside the safe area.
Section labels are rows for drawing only; they do not receive selectable indices
or hit regions.

Controls rows carry semantic actions such as `Rebind`, `NextControls`,
`AdjustMouse`, and `Defaults`. Keyboard, gamepad, and mouse activation read those
semantic actions instead of assuming that a visual row number maps to a fixed
global control.

Audit note: the older `PhoneMenuLayout.hpp` parallel geometry path was removed in
the 2026-07-25 phone-display audit. See `docs/code-audit/` for the current
authority graph and pruning record.
