# Asset Provenance Register

Every file distributed with Digital Breakdown must have a known origin and a documented right to ship it.

Do not record private purchase receipts, license keys, addresses, or account numbers in this public file. Keep those in encrypted business records and reference them by an internal receipt ID.

## Status values

- `original` — created by the project owner;
- `licensed` — third-party asset with documented distribution rights;
- `open-source` — covered by a compatible software/content license;
- `platform` — supplied by a platform SDK under its terms;
- `review-needed` — origin or shipping right is not yet fully documented;
- `not-for-release` — development reference that must not enter release packages.

## Shipped asset groups

| Path or group | Type | Status | Creator/source | License or ownership basis | Platforms | Notes |
|---|---|---|---|---|---|---|
| `native-models/phone.dbmesh` | Model | original | Project | Project-owned | Desktop, Android | Confirm source model history before public release |
| `native-models/human.dbhuman` | Model | original | Project | Project-owned | Desktop, Android | Procedural/native representation |
| `native-models/flower.dbmesh` | Model | original | Project | Project-owned | Desktop, Android | Confirm source model history |
| `native-desktop/audio/` | Audio | review-needed | Project/mixed | Verify each cue individually | Desktop | Do not assume a folder-level status proves every file |
| `native-android/app/src/main/res/raw/*.mp3` | Audio | review-needed | Mirrors desktop audio | Verify each cue individually | Android | Must match approved desktop source list |
| `native-tv-gifs/*.dbgif` | Visual content | review-needed | Project/conversion pipeline | Verify source media rights individually | Desktop, Android | Highest-priority release audit area |
| `native-android/app/src/main/cpp/game/BitmapFont.hpp` | Font/render data | review-needed | Project/generated | Confirm source font and redistribution rights | All native | Replace or document before release if derived from a font |
| GLFW | Software dependency | open-source | GLFW contributors | zlib/libpng | Desktop | Include third-party notice |
| miniaudio | Software dependency | open-source | David Reid | Public domain or MIT-0 option; record chosen basis | Desktop | Include notice if using MIT-0 basis |
| IXWebSocket | Software dependency | open-source | Machine Zone contributors | BSD-3-Clause | macOS desktop networking | Include notice |
| OkHttp | Software dependency | open-source | Square contributors | Apache-2.0 | Android | Include notice and license text as required |
| Cloudflare Workers tooling | Development/backend dependency | open-source/platform | Cloudflare and contributors | Package licenses and service terms | Backend | Not all tooling ships in game packages |

## Per-asset review fields

For each externally sourced asset, retain privately:

- exact source URL or vendor;
- creator and copyright owner;
- acquisition date;
- license text/version at acquisition;
- receipt or permission reference;
- permitted platforms and commercial use;
- attribution requirements;
- modification rights;
- whether sublicensing/redistribution inside a game is permitted;
- source file hash;
- final shipped file hash.

## Release gate

A public release must fail its human review if any packaged asset remains `review-needed` or `not-for-release`.

Before release:

1. enumerate package contents for every platform;
2. compare them with this register;
3. produce `THIRD_PARTY_NOTICES.md` from approved dependencies;
4. verify required attribution appears in the distributed package or documentation;
5. archive licenses and receipts outside the public repository;
6. verify that development references and source media were not copied into packages.
