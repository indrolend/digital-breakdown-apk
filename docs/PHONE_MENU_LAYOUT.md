# Phone Menu Layout

The phone-bound menus use `PhoneMenuLayout.hpp` as their shared geometry source.
The desktop renderer and pointer hit-testing both consume the same calculated
rows so visual selection rectangles and clickable regions cannot drift apart.

The visible phone screen is divided into a dark panel, an inner safe rectangle,
a title/header zone, a content zone, and a footer zone. Current proportions are:

- Horizontal safe margin: 11% of visible screen width on each side.
- Top safe margin: 9.5% of visible screen height.
- Bottom safe margin: 9.5% of visible screen height.
- Header/title zone: 15% of visible screen height.
- Footer zone: 12% of visible screen height.

Short pages stay optically centered in the content zone. Dense pages use the same
safe rectangle, but request the denser row spacing token. Controls is split into
two pages so row scale stays legible and every row remains inside the safe area.
Section labels are rows for drawing only; they do not receive selectable indices
or hit regions.

Controls rows carry semantic actions such as `Rebind`, `NextControls`,
`AdjustMouse`, and `Defaults`. Keyboard, gamepad, and mouse activation read those
semantic actions instead of assuming that a visual row number maps to a fixed
global control.
