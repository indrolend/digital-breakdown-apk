# Publishing Readiness

This is the release gate for self-publishing Digital Breakdown. It is intentionally sized for one developer: each item should prevent a concrete class of failure rather than create ceremony.

## Current intended path

1. restricted itch.io testing;
2. public Steam page;
3. controlled Steam Playtest;
4. Steam-first Windows/macOS release;
5. Android closed testing and later independent release decision.

GitHub Actions artifacts are engineering evidence, not the ordinary player distribution channel.

## Identity and ownership

- [ ] Publishing owner decided: individual or legal entity.
- [ ] Project IP ownership documented.
- [ ] Employment and invention-assignment obligations reviewed.
- [ ] Dedicated business/support/security/privacy email addresses active.
- [ ] Storefront, domain, GitHub, email, Cloudflare, banking, and signing accounts use MFA.
- [ ] Recovery codes and key backups stored offline.

## Version and reproducibility

- [x] `version.json` is the authoritative product/compatibility manifest.
- [x] Native and backend protocol versions derive from the manifest.
- [x] Android and macOS package versions derive from the manifest.
- [ ] Windows executable metadata displays game version and build number.
- [ ] Main menu/About/debug export displays version, build, commit, protocol, save schema, and asset schema.
- [ ] Release tags are immutable (`vMAJOR.MINOR.PATCH[-stage.N]`).
- [ ] Every release contains checksums and a build manifest.
- [ ] A clean checkout of the release tag can reproduce the package.

## Assets and licensing

- [ ] Every packaged asset has an approved entry in `docs/ASSET_PROVENANCE.md`.
- [ ] Audio cues have individual origin/license records.
- [ ] Every TV-room visual has verified commercial redistribution rights.
- [ ] Font origin and redistribution rights are verified.
- [ ] `THIRD_PARTY_NOTICES.md` is complete.
- [ ] Release packages contain no reference, source, temporary, or unlicensed files.

## Privacy

- [x] Engineering data map exists in `docs/PRIVACY_DATA_MAP.md`.
- [ ] Public privacy policy is published at a stable URL.
- [ ] Actual Cloudflare logging and retention are verified against the policy.
- [ ] Diagnostic export is inspectable and opt-in.
- [ ] Diagnostic export removes secrets, IP addresses, unrelated paths, and account names.
- [ ] No analytics, ads, accounts, or persistent server profiles are added without review.

## Security

- [x] Private vulnerability-reporting policy exists in `SECURITY.md`.
- [ ] `security@indrolend.com` is active and monitored.
- [ ] Android production signing key has encrypted and offline backups.
- [ ] macOS signing/notarization credentials use protected CI secrets.
- [ ] Windows signing decision is documented.
- [ ] Staging and production backend credentials are separate.
- [ ] Multiplayer has payload, rate, room, participant, idle, and lifetime limits.
- [ ] Unsupported protocols fail with a clear update message.
- [ ] Production backend has a tested disable/rollback procedure.

## Accessibility and usability

- [ ] Keyboard/controller bindings are remappable.
- [ ] Important audio cues have visual equivalents.
- [ ] Music and effects volumes are separate.
- [ ] Camera motion, shake, flashing, and datamosh can be reduced.
- [ ] HUD/text scaling is usable at supported resolutions.
- [ ] Game state is not communicated by color alone.
- [ ] Low-spec mode is tested on target hardware.
- [ ] Install/update/uninstall paths are tested by someone other than the developer.

## Support

- [ ] Support page lists supported platforms and minimum specifications.
- [ ] Bug reports request game version, build, platform, and reproduction steps.
- [ ] Known-issues page exists for each public test/release.
- [ ] Save locations, reset procedure, and backup behavior are documented.
- [ ] Support promises are realistic for one person.
- [ ] Public builds provide a safe diagnostic export.

## Multiplayer operations

- [ ] Health endpoint reports service commit, protocol, and deployment identity.
- [ ] Staging deployment is tested before production.
- [ ] Multiplayer test windows and capacity are defined.
- [ ] Room/session data expires automatically.
- [ ] Logs are bounded and have an explicit retention period.
- [ ] A previous compatible backend deployment can be restored quickly.
- [ ] Server/client compatibility is tested before publishing a client update.

## Store and release materials

- [ ] One-sentence description is accurate.
- [ ] Long description explains the current game rather than promised future scope.
- [ ] Trailer shows actual representative gameplay.
- [ ] Screenshots use the current build and contain no debug/private information.
- [ ] Capsule art and logos are available in required dimensions.
- [ ] Press kit contains approved screenshots, logo, description, contact, and credits.
- [ ] Refund/support/privacy/security URLs are stable.
- [ ] Release notes and known issues are written before upload.

## Test progression

### Restricted test

- [ ] 10–20 outside testers can install and launch without direct assistance.
- [ ] Crash, performance, control, and multiplayer evidence is collected.
- [ ] Test packages are clearly labeled and revocable.

### Steam Playtest

- [ ] Store page accurately represents current gameplay.
- [ ] Backend capacity and test windows are defined.
- [ ] Save/protocol compatibility policy is disclosed.
- [ ] Feedback intake and triage are manageable.

### Paid release or Early Access

- [ ] Current build is worth its current price without relying on promises.
- [ ] Update, communication, and support workload is sustainable.
- [ ] Release branch and emergency patch process are tested.
- [ ] Financial, tax, banking, and storefront records are organized.

## Release authority

For a one-person project, the developer is also release manager. Before publishing, write a short release decision record containing:

- version and tag;
- exact commit;
- included platforms;
- backend deployment;
- save/protocol compatibility;
- known issues;
- rollback target;
- explicit decision: `ship`, `delay`, or `cancel`.
