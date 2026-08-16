# CommandHUD capability inventory

| Capability | Existing owner(s) | Path | Language | Unique value | Duplication | Recommended owner | Action |
|---|---|---|---|---|---|---|---|
| Verified DATA root | `dbdev`, DataFactory | `tools/dbdev.ps1`; historical `packages/vscode-extension/src/core.js` | PowerShell/Node | Both reject missing Git state | Reimplemented per frontend | CommandHUD core | ADAPT |
| Git authority and dirty state | `dbdev`, DataFactory, menus | `tools/dbdev.ps1`; historical `core.js`; `tools/dev-menu.mjs` | PowerShell/Node | Existing output is useful but incomplete upstream context | Three implementations | CommandHUD core | ADAPT |
| Command execution | menus, legacy CommandHUD | `tools/dev-menu.mjs`; `scripts/dev-menu.sh`; historical `CommandHud-v21.ps1` | Node/shell/PowerShell | Legacy HUD streams/stops and records exit | UI-bound and transient | CommandHUD core | ADAPT |
| Persistent run evidence | legacy CommandHUD | historical `%LOCALAPPDATA%/CommandHud/sessions/*/history.jsonl` | PowerShell | Raw logs and command history | Session-scoped; no project authority | CommandHUD core run records | ADAPT |
| Continuation packet | DataFactory order packet | historical `packages/vscode-extension/src/core.js` | Node | Authority and evidence contract | Order-only, no execution result | CommandHUD core | ADAPT |
| Environment doctor/repair | `dbdev` adapters | `tools/environment/*.ps1` | PowerShell | Visual Studio, CMake, Android-specific probing | Not generic execution | Existing adapter | KEEP |
| Native gameplay verification | repository verifier | `tools/run-native-tests.mjs`; `scripts/verify-gameplay.*` | Node/shell/PowerShell | Canonical focused gameplay tests | None | Repository adapter | KEEP |
| Asset mirror verification | repository verifier | `tools/verify_asset_mirrors.py` | Python | Canonical mirror invariant | None | Repository adapter | KEEP |
| Multiplayer verification | package scripts | `multiplayer-server/package.json` | npm | Types, tests, audit, dry deploy | None | Repository adapter | KEEP |
| Release publication | GitHub Actions | `.github/workflows/native-release.yml` | Actions | Canonical artifact/provenance graph | Local scripts consume it | GitHub Actions | KEEP |
| Distribution update | release downloader | `tools/release/get-latest-native.ps1`; `distribution/project.json` | PowerShell/JSON | Verified download/install behavior | Manifest lookup repeated | Core comparison + downloader adapter | ADAPT |
| Interactive device operations | menus and `dbdev` | `tools/dev-menu.mjs`; `tools/dbdev.ps1`; device scripts | Node/PowerShell | Device install/log/scrcpy workflows | Menus duplicate routing | Existing device scripts | KEEP |
| C# developer UI | dev UI | `tools/dev-ui/` | C# | Simple Windows buttons/progress | Executes through `dbdev` | Future frontend only | DEPRECATE as owner |
| Node development menu | dev menu | `tools/dev-menu.mjs` | Node | Cross-platform interactive shortcuts | Duplicates command routing | Adapter compatibility only | DEPRECATE as owner |
| Shell development menu | dev menu | `scripts/dev-menu.sh`; `dev-menu.ps1` | shell/PowerShell | Legacy discoverability | Duplicates Node menu | Adapter compatibility only | DEPRECATE as owner |
| HUD presentation/history | legacy CommandHUD | historical `hate.this.meaningless.life/legacy/commandhud/CommandHud-v21.ps1` | PowerShell | Streaming, stopping, clipboard, rerun UX | UI owns execution/history | Future frontend over core records | DEPRECATE as owner |
| VS Code project/order UI | DataFactory | historical `hate.this.meaningless.life/packages/vscode-extension/` | Node/webview | Bounded order and authority packet | No streaming persistent run protocol | Future frontend over core records | ADAPT later |
| Context pack generator | mobile context helper | `scripts/make-context-pack.ps1` | PowerShell | Historical mobile source bundle | Uses historical runtimes and oversized concatenation | CommandHUD packet/context | DEPRECATE |

## Canonical ownership

- **CORE:** root resolution, Git authority, execution, immutable run records, reducers, packets, clipboard export, tool discovery, update comparison.
- **ADAPTER:** native tests, asset verification, multiplayer checks, environment repair, device operations, release downloader.
- **EXTERNAL AUTHORITY:** GitHub Actions owns release publication; `distribution/project.json` and the canonical manifest own update identity.
- **FRONTENDS:** future UI or VS Code surfaces consume core records and must not implement execution or history independently.
