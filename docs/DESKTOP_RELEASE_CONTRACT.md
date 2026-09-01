# Desktop release convergence contract

The release unit is a complete staged application, not a loose executable.
Windows, macOS, and Linux artifacts must come from the same commit and report
the same protocol, gameplay, save-format, and human release identity.

| Contract | Windows x64 | macOS | Linux x86-64 |
|---|---|---|---|
| Shipping boundary | `DigitalBreakdown/` in ZIP | `DigitalBreakdown.app` in ZIP | `DigitalBreakdown/` in tar.gz |
| Release build | MSVC x64 | universal x86_64 + arm64, target 11.0 | Ubuntu 22.04 x86-64 |
| Native regression suite | CTest | CTest | CTest |
| Runtime assets | beside executable | `Contents/Resources` | beside executable |
| Embedded provenance | `BuildIdentity` | `BuildIdentity` | `BuildIdentity` |
| Staged provenance | `build-info.json` | `Contents/Resources/build-info.json` | `build-info.json` |
| Dependency audit | PE imports | `otool -L` and bundle structure | `ldd` and ELF architecture |
| Build-tree smoke | `--smoke-test` | `--smoke-test` | `--smoke-test` |
| Extracted-package smoke | required | required | required |

The aggregate `desktop-release-contract` artifact contains all three packages,
their hashes in `checksums.txt`, and one verified schema-3 manifest. Steam and
itch.io should eventually consume these packages without gameplay forks.

## Current platform characterization

- Windows entered convergence with the most mature contract: full CTest,
  runtime staging, PE dependency inspection, extracted ZIP smoke, provenance,
  and hashing.
- macOS already compiled a universal application and performed a packaged
  smoke test, but ran only two direct test binaries and treated loose files in
  `Contents/MacOS` as resources. Convergence promotes full CTest and a formal
  `Contents/Resources` boundary with an explicit Info.plist.
- Linux already had XDG save-path behavior and mostly portable GLFW/OpenGL
  code. It lacked a controller-haptics implementation, TLS/WebSocket linkage,
  CI, staging, ELF audit, and a package contract. Convergence supplies those
  without forking gameplay.
- Android remains source-preserved and independently validated. APK tooling,
  signing, and packaging are outside the desktop release critical path.

## Release eligibility

A commit is a desktop artifact candidate only when all three platform jobs and
the aggregate contract job pass. A CI smoke test is noninteractive evidence;
it is not a substitute for graphical and controller testing on real machines.
macOS signing/notarization and storefront upload remain later, explicitly
authorized operations.

## Deferred cleanup candidates

- The shared gameplay directory under `native-android/` is misleading but must
  not move until the three-platform contract is green; a later move should be
  mechanical and behavior-neutral.
- `www/runtimes/` remains historical, non-authoritative material. Deletion is
  deferred until every active reference is proven absent.
- Developer `.command` launchers and legacy release/recovery scripts should be
  reviewed after artifact convergence, not deleted during platform debugging.
- Publishing, updater activation, SteamPipe, itch butler, signing, and
  notarization require separate release work and authorization.
