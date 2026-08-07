# Experimental release contract

`Native Cross-Platform Release` is the authoritative rolling experimental
publisher. It builds Windows, macOS, and Android from one commit, generates a
versioned manifest from the actual artifacts, verifies that manifest, and only
updates the `latest-native` release from `main` pushes or an explicitly
published manual dispatch from `main`.

`Native macOS Validation` is a manual validation workflow only. It is not a
release authority and must not be treated as a source for `latest-native`.

## Build identity

Desktop builds generate `BuildIdentity` at CMake configure time. It reports:

- channel: `experimental`
- human version: `experimental-YYYY.MM.DD` unless overridden by CI
- full and short Git commit
- multiplayer protocol version
- gameplay compatibility version
- save-format version
- platform and architecture
- build timestamp

Protocol and gameplay compatibility are still defined in
`native-network/MultiplayerProtocol.hpp`. The Worker declaration in
`multiplayer-server/src/protocol.ts` and the Android gameplay declaration in
`MultiplayerClient.java` are checked by
`tools/release/protocol-consistency.mjs`.

## Manifest schema

The rolling release publishes `build-manifest.json` with `schemaVersion: 3`.
Required top-level fields are:

- `channel`
- `humanVersion`
- `commit`
- `shortCommit`
- `protocolVersion`
- `gameplayVersion`
- `saveFormatVersion`
- `publishedAt`
- `artifacts`

Each artifact declares `platform`, `architecture`, `filename`, `assetName`,
`url`, `sha256`, `size`, and `package`. Legacy `windows` and `android` fields
remain for the PowerShell recovery installer.

Validate a manifest with:

```bash
node tools/release/verify-native-manifest.mjs release/build-manifest.json release
```

## Recovery installer and updater

`tools/release/get-latest-native.ps1` remains the recovery installer. It stages
commit-specific releases under `%LOCALAPPDATA%\DigitalBreakdown\releases` and
verifies SHA256 before extraction or launch.

The native desktop app can check the rolling manifest asynchronously from the
Settings menu or with:

```bash
DigitalBreakdown --check-updates
```

This PR deliberately does not activate a downloaded package from inside the
running app. Native in-app download, staged extraction, helper handoff, and
rollback activation remain follow-up work. The current safe activation path is
the PowerShell recovery installer.

## Multiplayer deployment

The Worker exposes `/health` with service, environment, protocol, Worker
version metadata, commit override if supplied, and deployment timestamp override
if supplied. Clients preflight this endpoint before creating or joining rooms
and report protocol mismatches as version mismatches instead of generic
connection failures.

Wrangler defines `staging` and `production` environments with distinct Worker
names. Pull requests and main pushes run type checks, tests, protocol
consistency, and a staging dry run. Production deployment is not performed by
the native release workflow.

Manual deployment order:

```bash
cd multiplayer-server
npm run check
npm run deploy:dry -- --env staging
npm run deploy -- --env staging
curl https://<staging-worker>/health
npm run deploy -- --env production
curl https://digital-breakdown-multiplayer.indrolend.workers.dev/health
```

Do not commit Cloudflare credentials. Use GitHub environments or local Wrangler
authentication for deployment.
