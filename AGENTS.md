# Working in Digital Breakdown

Before editing, fetch the remote, inspect the current branch and working tree, and derive architecture from current source, CMake, tests, CI, and references. Historical prose and old branches are not runtime authority.

The supported product is the native desktop game on Windows, macOS, and Linux.

Authority map:

- Human development entrance: `CommandHUD.cmd` / `hud desktop --root .`
- Semantic actions: `distribution/project.json` (`Play`, `Check`, `Prove`, `Explore`, `Ship`)
- Product and playtest modes: the `DigitalBreakdown` executable
- Build system: `native-desktop/CMakeLists.txt`
- Native test registry and runner: CTest
- Full repository verification: `node tools/verify.mjs`
- Continuous integration: `.github/workflows/ci.yml` invoking the same verification
- Release contract: `.github/workflows/native-release.yml`

Keep platform adapters and specialized modes below these interfaces. Do not add another root launcher, product executable, test runner, or release path for an existing responsibility.

- Gameplay and simulation: `native/game`
- Behavioral tests: `native/tests`
- Desktop host and presentation: `native-desktop`
- Multiplayer protocol: `native-network`
- Relay server: `multiplayer-server`

Preserve deterministic behavior, fixed-capacity gameplay state, protocol layout, save compatibility, and authoritative simulation unless the task explicitly changes them. Prefer focused changes and behavioral tests. Do not introduce parallel gameplay authorities, broad frameworks, dynamic entity architectures, or historical browser/mobile parity requirements.

Validate gameplay changes with the native desktop build and CTest suite. Validate protocol changes against both `native-network` and `multiplayer-server`. Treat `docs/DESKTOP_RELEASE_CONTRACT.md` and `docs/ASSET_CREDITS.md` as release constraints.
