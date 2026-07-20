# Security Policy

## Reporting a vulnerability

Do not open a public issue for a vulnerability involving credentials, private player data, multiplayer abuse, remote crashes, or package-signing material.

Report privately to `security@indrolend.com` with:

- affected game or server version;
- platform;
- reproduction steps;
- expected impact;
- logs or proof of concept with unrelated personal data removed.

Until that mailbox is active, security reports should remain private and should not be posted publicly.

## Supported versions

During pre-release development, only the newest published playtest build and the current production multiplayer protocol are supported. Versioned stable releases will document their support window in their release notes.

## Repository rules

The following must never be committed:

- Android keystores or passwords;
- Apple signing certificates or private keys;
- Steam credentials;
- Cloudflare API tokens;
- production `.env` files;
- player support exports containing personal data;
- crash logs that expose unrelated local file paths or account names.

Credentials belong in the platform secret store. Production and staging credentials must be separate and least-privileged.

## Multiplayer service

The service should fail closed on malformed or unsupported traffic. Changes to packet structure, accepted fields, ownership rules, limits, or compatibility require protocol tests and an explicit `protocolVersion` decision in `version.json`.

## Release response

For a confirmed vulnerability:

1. preserve evidence privately;
2. disable the affected service or feature when necessary;
3. rotate exposed credentials;
4. patch the supported branch;
5. produce a new immutable release rather than replacing an old version silently;
6. publish a concise advisory after users can update.
