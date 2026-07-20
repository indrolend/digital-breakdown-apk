# Privacy and Data Map

This file is the engineering source for the public privacy policy. It must describe what the shipped game and backend actually do, not what they are intended to do later.

## Current privacy posture

Digital Breakdown should operate without player accounts, advertising SDKs, contact uploads, precise location, voice capture, or cross-app tracking.

Multiplayer uses temporary room/session identifiers. Persistent personal profiles should not be introduced without a separate privacy and security review.

## Data inventory

| Data | Purpose | Processed by | Stored | Intended retention | Player-visible deletion |
|---|---|---|---|---|---|
| IP address and connection metadata | Network delivery, abuse prevention, infrastructure logs | Cloudflare and network providers | Potentially in provider logs | Provider-configured minimum | Not directly addressable without provider support |
| Temporary room code | Connect participants | Multiplayer worker and clients | Memory while room exists | Room lifetime | Room expires or is closed |
| Temporary player/session identifier | Distinguish participants | Multiplayer worker and clients | Memory while session exists | Session lifetime | Disconnect/room expiration |
| Protocol and game version | Compatibility and diagnostics | Client and multiplayer worker | Operational logs if enabled | Short, documented window | Log expiration |
| Replicated gameplay state | Provide multiplayer | Multiplayer worker and room participants | Memory while room exists | Room lifetime | Room expiration |
| Error and rate-limit events | Reliability and abuse investigation | Multiplayer worker/provider logs | Only when logging is enabled | Target: 7–30 days | Log expiration |
| Local settings and progression | Player experience | Player device | Local device storage | Until user deletes/reset data | In-game reset or OS uninstall/data clear |
| Optional diagnostic export | Player-requested support | Player device; support mailbox only if sent | Only after explicit user action | Delete after issue resolution | User can inspect before sending |

## Required implementation constraints

- Do not claim that no data is processed while network providers process IP addresses.
- Do not add analytics, advertising, crash-reporting SDKs, or account systems without updating this document first.
- Production logs must avoid raw gameplay payloads unless temporarily required for a specific incident.
- Never log credentials, room secrets beyond operational necessity, full device file paths, or unrelated device information.
- Diagnostic uploads must be opt-in.
- Support exports should use random report identifiers rather than player names.

## Public privacy policy checklist

Before any public store listing, publish a plain-language policy that states:

- who operates the game and how to contact them;
- that no account is currently required;
- what network and gameplay data is processed for multiplayer;
- the role of infrastructure providers;
- retention windows;
- whether data is sold or used for advertising;
- how players can request information or deletion where applicable;
- the policy effective date and revision history.

## Review triggers

Review this map when adding:

- accounts or authentication;
- persistent server-side progression;
- chat, voice, friends, or moderation systems;
- analytics or crash reporting;
- direct payments;
- cloud saves;
- user-generated content;
- a new backend provider;
- console or mobile platform services that collect additional identifiers.
