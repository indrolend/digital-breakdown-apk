# Desktop release contract

The release unit is a complete staged application, not a loose executable. Windows, macOS, and Linux artifacts must come from one commit and report identical protocol, gameplay, save-format, and release identity.

| Contract | Windows | macOS | Linux |
| --- | --- | --- | --- |
| Package | application directory in ZIP | `.app` in ZIP | application directory in tar.gz |
| Architecture | x64 | universal x86_64 + arm64 | x86-64 |
| Native regression suite | CTest | CTest | CTest |
| Runtime assets | beside executable | `Contents/Resources` | beside executable |
| Provenance | `build-info.json` | `Contents/Resources/build-info.json` | `build-info.json` |
| Package smoke test | required | required | required |
| Dependency/architecture audit | required | required | required |

A release candidate is eligible only when all three platform jobs and the aggregate artifact contract pass. CI smoke testing does not replace graphical and controller acceptance on real hardware. Signing, notarization, and storefront publication require explicit release authorization.
