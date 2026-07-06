# Digital Breakdown Mobile Port Status

## Known-good state

- `www/android-entry.mjs` restored from `www/runtimes/stylo-v2.mjs`
- `createRenderer(root);` is present in `boot()`
- APK builds, installs, launches
- Clean boot log:
  - `[STYLO-V2] boot`
  - `[STYLO-V2] room reset`
  - `[STYLO-V2] READY`
  - `[STYLO-V2] RUNNING`

## Known broken state

- Previous controller patch stack caused repeated:
  - `Cannot read property 'toFixed' of undefined`
- Broken file archived as:
  - `archive/android-entry.broken-tofixed-loop.mjs`

## Main workflow

Build, install, launch, and capture logs:

    .\scripts\build-install-run.ps1

Copy filtered logs:

    .\scripts\copy-debug-log.ps1

## Development rule

Do not stack broad regex patches.

Preferred sequence:

1. Start from known-good runtime.
2. Create a named runtime variant in `www/runtimes/`.
3. Make one small change.
4. Build/install/run.
5. Commit only after clean boot.

## Next feature target

Create:

- `www/runtimes/stylo-v2.5-input-router.mjs`

Input router priority:

1. Controller in AUTO mode
2. Pointer/touchpad
3. Keyboard

Manual modes:

- `0` auto
- `1` keyboard
- `2` pointer/touchpad
- `3` controller
