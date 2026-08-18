# Digital Breakdown

**DATA** is an experimental cross-platform action game built around a playable smartphone, room-based combat, traversal, soul capture, and host-authoritative multiplayer.

Native runtimes currently target:

- Windows
- macOS
- Android

The project is community-oriented and operated without a profit objective. Contributions, forks, research, and independent servers are welcome.

## Project structure

- `native-android/app/src/main/cpp/game/` — shared native gameplay simulation
- `native-desktop/` — Windows/macOS desktop runtime and presentation
- `native-network/` — shared multiplayer protocol support
- `multiplayer-server/` — authoritative relay/server implementation
- `native/tests/` — gameplay, multiplayer, determinism, progression, and contract tests
- `docs/DEVELOPMENT.md` — detailed development and verification workflows
- `docs/ASSET_CREDITS.md` — third-party asset/license notes

The native simulation is shared across desktop and Android. Multiplayer keeps durable gameplay outcomes host-authoritative while allowing bounded local presentation/prediction where appropriate.

## Desktop development

Requirements:

- CMake 3.22 or newer
- a C++17 compiler
- Windows 10/11 or macOS

Configure, build, and run the registered native tests:

```text
cmake -S native-desktop -B build/desktop-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop-release --config Release --parallel
ctest --test-dir build/desktop-release -C Release --output-on-failure
```

See [Development](docs/DEVELOPMENT.md) for the current verification, packaging, Android, multiplayer, and platform-specific workflows.

## Contributing

Please preserve gameplay determinism and multiplayer compatibility, keep behavioral changes covered by tests, and avoid introducing parallel sources of truth for shared state.

Do not commit credentials or media without documented redistribution permission.

## License

Project-owned source code is released under the [GNU Affero General Public License v3.0](LICENSE). Network deployments of modified versions must offer their corresponding source code as required by the AGPL.

Third-party dependencies and assets remain under their respective licenses. The public source repository intentionally excludes media whose redistribution rights have not been confirmed. See [Asset Credits](docs/ASSET_CREDITS.md).
