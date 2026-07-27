# Orphaned Files

Audit date: 2026-07-25

## Removed

- `native-android/app/src/main/cpp/game/PhoneMenuLayout.hpp`

Reason: superseded by `PhoneDisplayLayout.hpp`. Current desktop rendering,
desktop hit-testing, and updated layout tests use the physical phone display
layout directly. Menu semantics remain in `PhoneMenuModel.hpp`.

## Tracked Artifact Removed

- `artifacts/ui-phone-layout/gameplay-display-20260725-1/phone.png`

Reason: generated evidence for the previous phone-as-gameplay-HUD experiment.
That direction is now corrected; keeping this artifact would make the branch look
like it still endorses moving readable HUD authority onto the phone.

## Historical Docs Kept

- `docs/PHONE_DISPLAY_INVENTORY.md`
- `docs/PHONE_MENU_LAYOUT.md`

Reason: these documents are useful context. They have been superseded by this
audit packet where the old wording no longer matches the current source.

## Not Orphaned

- Android shared game headers under `native-android/app/src/main/cpp/game` remain
  source-of-truth shared code for both desktop and Android. Android renderer
  parity is not audited in this desktop-focused pass, so Android render files are
  not pruned.
- `BitmapFont.hpp` remains the gameplay HUD font path and phone text fallback.
- `drawSecretTvScreen()` remains the TV-room screen path and is not part of the
  phone display prune.
