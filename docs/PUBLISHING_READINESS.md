# Publishing Readiness

This file records only work that is concrete for the current solo project. It is not a studio checklist and it does not make future storefront, business, support, or community decisions in advance.

## Current publishing direction

- Use local and GitHub builds for development.
- Use restricted itch.io distribution when outside testers are needed.
- Consider a Steam page and Steam Playtest after the game can be installed and tested without direct intervention.
- Treat Android as a separate compatibility target, not an automatic simultaneous public launch.
- Do not publish automatically from every `main` commit.

## Before any outside build

- [x] One file defines the visible game version and multiplayer protocol: `version.json`.
- [x] Compiled builds record the version, build number, commit, and protocol.
- [ ] The game displays that identity in a menu, overlay, or diagnostic screen.
- [ ] The distributed package comes from a clean commit and is retained with its checksum.
- [ ] Testers can distinguish development builds from public releases.
- [ ] Installation and launch have been tested on the intended machine or device.
- [ ] Multiplayer messages are bounded and invalid traffic is rejected.

## Before a public or paid build

- [ ] Every packaged asset has a known source and commercial redistribution permission.
- [ ] Signing keys and storefront credentials are backed up outside the repository.
- [ ] The actual data handled by multiplayer and diagnostics is documented accurately.
- [ ] A working support contact and public privacy page exist.
- [ ] The game has a practical rollback path: previous package, source tag, and compatible backend deployment.
- [ ] The release has a tag, exact commit, checksums, release notes, and known issues.
- [ ] The build has been tested by someone other than the developer.

## Decisions to defer until they become real

Do not add infrastructure merely because a larger studio might use it. Add these only when the project actually needs them:

- legal entity and business-account structure;
- separate staging infrastructure;
- save-schema migration framework;
- asset/content schema versioning;
- compatibility ranges across multiple protocol generations;
- Windows code signing;
- macOS notarization automation;
- public vulnerability intake;
- community contribution governance;
- formal release branches;
- analytics, accounts, cloud saves, or crash-upload services.

When one of these becomes necessary, document the concrete requirement and implement the smallest mechanism that satisfies it.
