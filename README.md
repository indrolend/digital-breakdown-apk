# Digital Breakdown

Native desktop action game.

Windows / macOS / Linux.

## Build

```text
cmake -S native-desktop -B build/desktop-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop-release --config Release --parallel
ctest --test-dir build/desktop-release -C Release --output-on-failure
```

Windows development entrypoint:

```powershell
.\tools\dbdev.ps1 desktop-run -Configuration Release
```

AGPL-3.0 for project-owned source. Third-party assets keep their own licenses.
