# Build Graph

Audit date: 2026-07-25
Branch: `agent/ui-phone-layout-normalization`

## Desktop Targets

- `DigitalBreakdown` links GLFW, OpenGL, miniaudio, IXWebSocket on macOS, the
  desktop host files, shared game logic from `native-android/app/src/main/cpp`,
  and native multiplayer protocol code.
- `Pass7ParityTest` links shared `Game.cpp` and checks gameplay parity contracts.
- `MultiplayerProtocolTest` links shared `Game.cpp` and native protocol code.
- `PhoneDisplayStateTest` links shared `Game.cpp` and checks phone display state
  plus generated display-menu layout.
- `PhoneMenuLayoutTest` is retained as a target name for existing scripts, but it
  now tests `PhoneDisplayLayout.hpp`; the obsolete `PhoneMenuLayout.hpp` file was
  removed.
- `GameplayRoleAndSoulMotionTest`, `GameplayGeometryAndConfigTest`,
  `TargetLifecycleTest`, `GameplayStateContractsTest`, and
  `PhoneBodyContractTest` remain lightweight shared-header contract tests.

## Runtime Assets

The desktop build copies these directories beside the generated executable or app
bundle target directory:

- `native-desktop/audio`
- `native-desktop/fonts`
- `native-models`
- `native-tv-gifs`

## Primary Source Flow

```text
native-desktop/main.cpp
  -> Game.hpp / Game.cpp
  -> PhoneMenuModel.hpp
  -> PhoneDisplayLayout.hpp

native-desktop/DesktopRenderer.cpp
  -> GameState
  -> PhoneDisplayState
  -> PhoneDisplayLayout.hpp
  -> Source Sans 3 via stb_truetype
  -> OpenGL texture on phone screen

native-desktop/DesktopAudio.cpp
  -> GameState audio events
  -> miniaudio

native-desktop/DesktopMultiplayer.cpp
  -> MultiplayerProtocol
  -> IXWebSocket on macOS
```

Graph files:

- `docs/code-audit/graphs/targets.dot`
- `docs/code-audit/graphs/includes.dot`
