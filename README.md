# Digital Breakdown

Digital Breakdown is a native desktop action game for Windows, macOS, and Linux.

Project-owned source is licensed under the [GNU Affero General Public License v3.0](LICENSE). Third-party assets retain their own licenses; see [asset credits](docs/ASSET_CREDITS.md).

## Build and test

Requirements: CMake 3.22 or newer and a C++17 compiler.

```text
cmake -S native-desktop -B build/desktop-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop-release --config Release --parallel
ctest --test-dir build/desktop-release -C Release --output-on-failure
```

On Windows, the supported developer entrypoint is:

```powershell
.\tools\dbdev.ps1 desktop-run -Configuration Release
```

Useful commands include `desktop-build`, `desktop-test`, `desktop-smoke`, `room-smoke`, and `playtest`. Run `.\tools\dbdev.ps1 help` for the current list.

## Source layout

- `native/game`: gameplay and shared simulation
- `native/tests`: native behavioral tests
- `native-desktop`: desktop host, rendering, input, audio, and packaging
- `native-network`: multiplayer protocol
- `native-models`, `native-tv-gifs`, `native-desktop/audio`, `native-desktop/fonts`: runtime assets
- `multiplayer-server`: relay server

Gameplay changes must remain deterministic and include behavioral tests. Do not commit credentials or media without documented redistribution permission.
