# Data

Data is an experimental cross-platform action game with native
Windows, macOS, and Android runtimes plus an authoritative multiplayer relay.

The project is community-oriented and operated without a profit objective.
Contributions, forks, research, and independent servers are welcome.

## License

Project-owned source code is released under the
[GNU Affero General Public License v3.0](LICENSE). Network deployments of
modified versions must offer their corresponding source code as required by
the AGPL.

Third-party dependencies and assets remain under their respective licenses.
The public source repository intentionally excludes media whose redistribution
rights have not been confirmed. See [Asset Credits](docs/ASSET_CREDITS.md).

## Desktop development

Requirements:

- CMake 3.22 or newer
- A C++17 compiler
- Windows 10/11 or macOS

Configure and build:

```text
cmake -S native-desktop -B build/desktop-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop-release --config Release --parallel
ctest --test-dir build/desktop-release -C Release --output-on-failure
```

More detailed workflows are in [Development](docs/DEVELOPMENT.md).

## CommandHUD

The terminal context-condensing shell is CommandHUD's primary product surface. The browser-connected renderer and experimental fixed-screen TUI remain secondary clients over the same authoritative local runtime and semantic state.

CommandHUD provides two local interfaces over the same repository, shell, Git, and immutable evidence state:

- Install CommandHUD once from its [product repository](https://github.com/indrolend/hate.this.meaningless.life), then double-click `CommandHUD.cmd` for the visual desktop application.
- Double-click `CommandHUD Shell.cmd`, or run `hud shell`, for the terminal-only interface with condensed output by default.

The terminal interface accepts ordinary pasted PowerShell, Bash, or Command Prompt commands. After every command it displays the shortened ChatGPT-ready result and automatically copies that identical result to the clipboard. Complete stdout and stderr remain available through `/raw`; `/copy` copies the last result again. CommandHUD runtime and documentation are owned by `indrolend/hate.this.meaningless.life`; this repository owns only DATA identity and typed command declarations in `distribution/project.json`.

Both launchers select this repository through an explicit root and delegate to the installed `hud` command. The optional desktop shortcut is machine-specific and is not part of the repository: create a shortcut to the `.cmd` file inside your own clone if you want desktop access.

In an interactive terminal, the original CommandHUD face marks the prompt and a single status row animates factual `RUNNING` to `PASS`, `FAIL`, or `STOPPED` transitions. Motion is automatically disabled for redirected output and reduced-motion environments. Run `hud shell --no-animation` for an explicitly static interface.

The default interface prioritizes robust terminal behavior: a simple `> ` prompt accepts ordinary commands, condensed output follows, and the same context is copied automatically. It uses no alternate screen, mouse mode, or cursor-positioned layout. The fixed visual experiment remains available with `hud shell --tui`; use it only where the terminal supports its control sequences reliably.

## Contributing

Please keep gameplay deterministic, preserve multiplayer protocol compatibility,
and include tests for behavioral changes. Do not commit credentials or media
without documented redistribution permission.
