# Digital Breakdown native parity contract

## Authoritative browser implementation

The only authoritative browser source for native parity is:

`reference/browser-pass7/index_module.mjs`

The authoritative source repository is:

`https://github.com/indrolend/digitalbreakdownreference`

Before analysis, run:

`powershell -ExecutionPolicy Bypass -File tools/ai/sync-browser-reference.ps1`

On non-Windows shells, run:

`git clone --depth 1 https://github.com/indrolend/digitalbreakdownreference.git reference/browser-pass7`

Do not use these older files as behavioral specifications:

- `www/runtimes/stylo-v2.3-sticks.mjs`
- `www/runtimes/stylo-v2.mjs`
- any other browser prototype under `www/`

They may be inspected for historical context only.

## Native destination

Shared gameplay code:

`native-android/app/src/main/cpp/game/`

Desktop renderer:

`native-desktop/`

Android renderer:

`native-android/app/src/main/cpp/`

## Translation rules

1. Translate observable behavior from the authoritative Pass 7 source.
2. Preserve constants, state transitions, coordinate conventions, and update order.
3. Inspect complete function bodies and all transitive dependencies before editing.
4. Do not redesign mechanics.
5. Do not introduce behavior based on older runtimes.
6. Keep gameplay behavior shared between Windows and Android.
7. Work only on the explicitly requested subsystem.
8. Do not claim parity merely because code compiles.
9. Do not embed a WebView, JavaScript runtime, or Three.js into the finished native game.
10. If the reference sync fails, stop and report the failure instead of substituting an older runtime.

## Validation

Run:

`powershell -ExecutionPolicy Bypass -File tools/dbdev.ps1 desktop-build`

Then run:

`powershell -ExecutionPolicy Bypass -File tools/dbdev.ps1 desktop-smoke`

Report:

- exact browser functions inspected
- constants and state fields translated
- files changed
- build and test results
- remaining approximations
