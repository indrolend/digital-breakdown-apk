## Summary

Describe the behavioral or tooling change.

## Scope

- [ ] Shared gameplay/state
- [ ] Windows desktop
- [ ] macOS desktop
- [ ] Android
- [ ] Multiplayer protocol/backend
- [ ] Debug suite/diagnostics
- [ ] Build/release tooling

## State and compatibility

- Protocol/schema version changed: **no / yes — explain**
- Save/progression compatibility changed: **no / yes — explain**
- New runtime feature registered in the debug suite: **not applicable / yes**
- Migration or deployment action required: **no / yes — explain**

## Validation

- [ ] `git diff --check`
- [ ] Native parity tests
- [ ] Multiplayer protocol tests
- [ ] Backend `npm run check`
- [ ] Backend `npm run deploy:dry`
- [ ] Android build
- [ ] Windows build/playtest
- [ ] macOS build/playtest

List commands, devices, known failures, and anything not tested.

## Release impact

- [ ] No release note required
- [ ] Patch release candidate
- [ ] Feature/minor release candidate
- [ ] Rolling `latest-native` only

## Cleanup

- [ ] No generated build output, logs, diagnostic sessions, credentials, SDK paths, or local configuration are tracked.
- [ ] The branch has one clear purpose and superseded PRs/branches are identified.
