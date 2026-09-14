# Digital Breakdown

Native desktop action game.

Windows / macOS / Linux.

## Develop

CommandHUD is the human-facing development entrance. From the repository root:

```text
hud desktop --root .
```

Windows users can double-click `CommandHUD.cmd`. It exposes five semantic actions:

- **Play** — build and launch `DigitalBreakdown`
- **Check** — run the fast traversal confidence tier
- **Prove** — run the exhaustive traversal confidence tier
- **Explore** — run the mutation/stress gauntlet
- **Ship** — run the complete repository verification chain

CommandHUD delegates to repository-owned scripts; it is not a second build or test system.

## Underlying authorities

```text
cmake -S native-desktop -B build/desktop-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop-release --config Release --parallel
ctest --test-dir build/desktop-release -C Release --output-on-failure
```

The product executable is `DigitalBreakdown`, including developer/playtest modes such as Rooms and Traversal Lab. CMake owns builds, CTest owns native behavioral verification, `.github/workflows/ci.yml` runs the same complete verification used locally, and `.github/workflows/native-release.yml` owns the three-platform artifact contract.

Windows script-level debugging remains available beneath CommandHUD:

```powershell
.\tools\dbdev.ps1 desktop-run -Configuration Release
```

AGPL-3.0 for project-owned source. Third-party assets keep their own licenses.
