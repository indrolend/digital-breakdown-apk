# Asset Credits

This project uses a mix of original/generated assets and third-party free assets. Third-party assets must keep their creator, source URL, and license recorded here before any public release package is published.

## Third-Party Source Assets

These local source assets were provided during development. Their source pages and license metadata were verified through Sketchfab's public model API on 2026-07-24.

| Working Name | Local Source | Format | Current Use | Creator | Source URL | License | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CRT TV model | `/Users/joelgutierrez/Downloads/crt-tv/source/model.zip` | DAE + textures | Candidate secret TV room model | Timothy Ahene | https://sketchfab.com/3d-models/crt-tv-9ba4baa106e64319a0b540cf0af5aa9e | Sketchfab Free Standard | DAE metadata lists Assimp as exporter only; source attribution comes from Sketchfab page/API. |
| Wires model | `/Users/joelgutierrez/Downloads/wires/source/wires.fbx` | FBX + textures | Candidate secret TV room cable dressing | Bulat.Shakirov | https://sketchfab.com/3d-models/wires-fca671dcab294aff97fc422915e791ef | Creative Commons Attribution 4.0 International (CC BY 4.0) | FBX strings list texture names only; source attribution comes from Sketchfab page/API. |
| Walk Cycle human model | `reference/browser-pass7/assets/embedded-assets.js` (`HUMAN_FBX_BASE64`) | FBX embedded as base64; baked to DBH1 | Human/enemy model and walk animation | Niraj Ekaant | https://sketchfab.com/3d-models/walk-cycle-05c7560e49c1441aa0c70d3dc7bc710b | Creative Commons Attribution 4.0 International (CC BY 4.0) | Source comment exists in browser reference; Sketchfab API verified creator/license on 2026-07-24. |

## User-Provided Source Assets

These assets are recorded as user-provided/original unless a third-party source is later identified.

| Working Name | Local Source | Format | Current Use | Creator / Owner | Source URL | License | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| iPhone 17 Pro phone model | `reference/browser-pass7/assets/embedded-assets.js` (`IPHONE_GLB_BASE64`) | GLB embedded as base64; baked to DBM1 | Player phone model | Joel Gutierrez / project-provided | N/A | Project-owned/user-provided, pending confirmation | Browser reference comment says “iPhone 17 Pro model uploaded by user”; embedded GLB metadata only lists Blender glTF exporter and no external creator/license/source URL. |

## Current Native Runtime Assets

| Asset | Repository Path | Source / Attribution Status | Notes |
| --- | --- | --- | --- |
| Phone model | `native-models/phone.dbmesh` | Baked derivative of user-provided iPhone 17 Pro GLB, pending final ownership confirmation. | Baked native runtime mesh generated from `IPHONE_GLB_BASE64`. |
| Flower model | `native-models/flower.dbmesh` | Derived from checked-in browser reference asset; needs final source attribution audit before release. | Baked native runtime mesh. |
| Human model | `native-models/human.dbhuman` | Baked derivative of “Walk Cycle” by Niraj Ekaant, CC BY 4.0. | Baked native runtime model generated from `HUMAN_FBX_BASE64`. |
| TV GIF clips | `native-tv-gifs/*.dbgif` | Curated from the allowlist in `tools/build_tv_gifs.py`; each source URL should be reviewed before release. | Baked 12x8 runtime clips. |

## Release Rule

Before publishing builds outside local/internal testing:

1. Confirm each license still allows the planned use at release time.
2. Preserve required attribution text in release notes, credits, or an in-game credits screen.
3. Keep generated/baked derivatives traceable to their original source asset.
