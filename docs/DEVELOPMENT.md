# Digital Breakdown development

This repository is the canonical cross-platform Digital Breakdown source tree. The
GitHub repository's current `digital-breakdown-apk` name is historical; it does not
describe the repository's scope.

## Architecture and source ownership

| Subsystem | Canonical location | Platform adapters or legacy overlap | Action |
| --- | --- | --- | --- |
| Gameplay, game state, saves, menus | `native-android/app/src/main/cpp/game/` | Desktop compiles `Game.cpp` from this path | Keep shared; move only in a separately validated migration |
| Focused gameplay contracts | `native-android/app/src/main/cpp/game/gameplay/` | Older logic remains orchestrated by `Game.cpp` | Continue incremental extraction |
| Desktop runtime, renderer, audio, updates | `native-desktop/` | No interchangeable Android implementation | Keep platform-specific |
| Android runtime and renderer | `native-android/app/src/main/` | `android/` is the Capacitor WebView wrapper | Keep the native and WebView targets distinct |
| Multiplayer protocol | `native-network/` | Worker protocol mirror in `multiplayer-server/src/protocol.ts` | Verify with `protocol-consistency.mjs` |
| Multiplayer relay | `multiplayer-server/` | None | Keep host-authoritative protocol boundary |
| Web/WebView runtime | `www/`, `android/`, `capacitor.config.json` | `reference/browser-pass7/` is a behavioral oracle, not shipped runtime | Keep generated bundle untracked |
| Models, TV clips, audio | `native-models/`, `native-tv-gifs/`, platform resource folders | Audio is duplicated for native packaging | Retain until packaging can consume one source safely |
| Tests | `native/tests/`, `native-android/app/src/test/`, worker tests | CMake registers the desktop/shared suite | Keep near the build system that runs each suite |

The Android-shaped shared gameplay path is misleading, but moving it would touch
CMake, Gradle, includes, CI, and history. A future move to `src/shared/game/`
should be a dedicated mechanical change with both desktop and Android builds green.

## One developer interface

- Graphical Windows UI: `Open Digital Breakdown Dev.cmd`
- Terminal menu: `npm run menu`
- Scriptable Windows interface: `tools/dbdev.ps1 <command>`

Both UIs call the same canonical scripts. Lines in the form
`@@DBPROGRESS|percentage|message` are machine-readable milestones for the graphical
UI. They are intentionally also visible in a plain terminal, alongside normal stage
messages.

## Windows desktop

Required: Visual Studio with Desktop development with C++, CMake 3.22 or newer,
Git, and network access for pinned CMake dependencies.

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\desktop\run-desktop.ps1 -Configuration Release -BuildOnly
powershell -ExecutionPolicy Bypass -File .\tools\desktop\run-desktop.ps1 -Configuration Release
powershell -ExecutionPolicy Bypass -File .\scripts\verify-gameplay.ps1
ctest --test-dir .\build\desktop-release -C Release --output-on-failure
```

Release is the default because the game is performance-sensitive. Debug remains
available explicitly with `-Configuration Debug`. Outputs are under
`build/desktop-<configuration>/bin/`; audio, fonts, models, and TV clips are copied
beside the executable. Windows uses the static MSVC runtime.

## macOS desktop

Required: Xcode command-line tools, CMake 3.22 or newer, Git, and OpenGL-compatible
macOS. CMake fetches GLFW 3.4, miniaudio 0.11.25, and IXWebSocket 11.4.6. TLS uses
the Apple-supported IXWebSocket configuration.

```bash
bash scripts/verify-macos-baseline.sh
open build/macos-release/bin/DigitalBreakdown.app
```

After the first checkout, Finder users can double-click `Play Data on
Mac.command` for an incremental Release build and normal launch, or `Playtest
Traversal Lab.command` to launch directly into the isolated movement course.

The app bundle identifier is `com.indrolend.digitalbreakdown.native`. Architecture
comes from the selected CMake/Xcode toolchain; universal packaging belongs in the
release workflow rather than local default builds.

The verifier treats commit `b4e3ecb` as the last manually verified Mac Release
baseline. It rejects unrelated or older source, starts from an empty build directory,
runs the full CTest suite and smoke test, checks the embedded Release identity,
verifies packaged assets, and confirms every requested binary architecture. Set
`DB_MAC_ARCHITECTURES='x86_64;arm64'` to reproduce the universal CI package.

## Android native

Required: JDK 21, Android SDK 36, NDK `27.2.12479018`, CMake from the Android
toolchain, and Gradle 8.14.3 via the tracked wrapper under `android/`.
The minimum SDK is 27; native ABIs are `armeabi-v7a` and `arm64-v8a`.

```powershell
.\android\gradlew.bat -p .\native-android assembleDebug
powershell -ExecutionPolicy Bypass -File .\tools\device\deploy-local.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\adb-doctor.ps1
```

The APK is `native-android/app/build/outputs/apk/debug/app-debug.apk`; the application
ID is `com.indrolend.digitalbreakdown.native`. Do not change it as part of a
repository rename.

## Web and Capacitor WebView

Required: Node.js with npm. Use the lockfile for reproducible installs.

```powershell
npm ci
npm run build:web
npm run cap:sync
```

The WebView wrapper under `android/` is separate from the native Android app. The
generated `www/android-bundle.js`, Gradle outputs, CMake outputs, logs, and
`local.properties` files are not source.

## Multiplayer and releases

```powershell
node .\tools\release\protocol-consistency.mjs
Push-Location .\multiplayer-server
npm ci
npm run protocol:check
npm test
Pop-Location
```

Release manifests are written and verified by `tools/release/`. Publishing releases,
deploying the Worker, merging, and renaming the GitHub repository are external
actions and require explicit approval.

## Naming migration

Recommended project/repository/npm name: `digital-breakdown`.

1. Update neutral documentation, npm metadata, clone URLs, badges, workflow defaults,
   release-manifest repository defaults, and update URLs in one reviewable PR.
2. Rename the GitHub repository only after that PR is ready. GitHub redirects help,
   but local remotes and external consumers should still be updated explicitly.
3. Keep `DigitalBreakdown` as the desktop executable and product name.
4. Keep Android application ID and macOS bundle ID stable.
5. Decide release artifact names independently; compatibility aliases may be useful.

## Troubleshooting

- Heavy desktop frame drops: confirm the build label says `Release`; delete or
  reconfigure only the project-owned `build/desktop-release` cache, then compare a
  smoke run. VSync is enabled with `glfwSwapInterval(1)` outside test mode.
- Raw progress lines in a terminal are expected protocol output, not errors.
- Missing CMake/compiler/ADB: run `tools/dbdev.ps1 doctor`.
- Never commit `backup-*` directories. Compare them first, then remove them manually
  only when their contents are known to be redundant.
