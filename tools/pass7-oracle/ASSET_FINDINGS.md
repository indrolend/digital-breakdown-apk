# Pass 7 authoritative asset findings

Source: `reference/browser-pass7/assets/embedded-assets.js`, interpreted only
through `reference/browser-pass7/index_module.mjs`.

## Phone model

- Embedded iPhone GLB SHA-256:
  `fef80e9154a509d0f7a3aafd530661faea9f3ac4b65e8c8f9c0f9b4ab5f9c619`.
- The GLB is 9,591,068 bytes and contains 34 nodes, 31 meshes, 16 materials,
  26 textures, and 26 embedded images.
- Raw transformed bounds measure
  `1.9801890017 × 4.0507021873 × 0.2827525247`.
- The runtime recenters the complete loaded scene and scales it by
  `0.03949932446`, producing exact normalized dimensions of approximately
  `0.0782161279 × 0.16 × 0.0111685337`.
- The primary `Screen_BG` geometry is a distinct nearly planar full-front mesh.
  `Screen_Glass` is also used by three small rear camera/lens meshes; those names
  do not exclusively identify the display.

## Runtime layering and native implications

- The browser creates the procedural `0.08 × 0.16 × 0.012` phone body and the
  `0.07 × 0.125` screen plane before loading the GLB.
- On successful GLB load it hides only the fallback body and camera bump. The
  procedural screen plane remains and continues to define screen basis, latch
  coordinates, screen deformation, emission, and the third-person vacuum pull
  point.
- Browser collision and ingestion constants deliberately use the procedural
  `0.08 × 0.16 × 0.012` solid, not the slightly smaller exact GLB bounds.
  Importing the detailed GLB is therefore a visual-parity concern, not a
  prerequisite for vacuum behavior parity.
- The current native box dimensions reproduce the authoritative fallback and
  gameplay solid exactly. Replacing those gameplay dimensions with GLB bounds
  would be a behavioral regression.
- Exact visual parity can be added independently by converting the audited GLB
  into renderer-native static vertex/index/material data while retaining the
  shared procedural screen plane and shared gameplay solid.

## Human model

- The authoritative walking human is a 310,220-byte binary FBX with SHA-256
  `dcca6f77854f58b183a5f5a07eff734790a387f85e4e8856d8246c2f972634db`.
- The browser measures the parsed, animated source, scales its height to `1.16`,
  grounds it using the source minimum Y, applies a `PI` forward-yaw correction,
  and plays animation clip zero.
- Attack presentation then adds procedural root and named-bone rotations on top
  of the mixer pose. Importing only the static mesh would therefore still miss
  browser walking and strike parity; the conversion path must retain skeleton,
  weights, clip zero, bone names, and animation timing.

## Flower model

- The flower GLB is 105,092 bytes with SHA-256
  `90453869a584665564ac396659515ca7415a7e1926593c1faea04a0e48bcc641`.
- It has 8 nodes, 2 meshes, 2 materials, no textures, and 1 embedded animation.
- The browser recenters it and scales its largest dimension to `0.72`, yielding
  exact normalized dimensions of approximately
  `0.72 × 0.3832546240 × 0.3644968913`.
- The runtime does not create an animation mixer for the flower, so the embedded
  animation is not played. Observable motion comes from the owning group's bob
  and Y rotation. Native conversion should preserve the default model pose and
  should not start the unused clip.

Run `node tools/pass7-oracle/extract-assets.mjs` to regenerate the detailed JSON
report and extracted GLBs under `build/pass7-oracle/assets/`.
