# Hourly Architecture Maintainer

Purpose: make recurring ChatGPT runs useful for behavior-preserving architecture work without letting an unattended task invent broad refactors.

## Scope

Repository: `indrolend/digital-breakdown-apk`
Primary work item: draft PR #49 on branch `agent/ownership-contracts-settings`.
Base contract: `main@14ba2e530ff6f7d66dde6ef01a6fb75a7688b674` unless the PR is explicitly rebased and the contract docs are updated.

## Governing rule

One state value -> one canonical owner -> named mutation path -> explicit result/event when other systems must react.

Broad mutable `GameState&` access is transitional infrastructure, not an ownership model.

## Hourly loop

1. Read PR #49 metadata and current head.
2. Read CI/check status for that exact head. Do not infer green from an older commit.
3. Compare the current branch against its base and inspect only newly changed files since the previous meaningful checkpoint when possible.
4. Run/search the ownership surface for `networkMutableState()` and classify any new production call site as a regression.
5. Read the current contract docs before proposing architecture changes:
   - `docs/current-main-ownership-contracts.md`
   - `docs/reset-lifetime-characterization.md`
   - `docs/energy-survival-transaction-characterization.md`
6. Prefer evidence in this order:
   - actual runtime/CI result;
   - source behavior and exact call sites;
   - focused tests/probes;
   - documented intent;
   - inference.
7. Emit explicit epistemic labels where useful: `PROVEN`, `OBSERVED`, `HYPOTHESIS`, `UNCHARACTERIZED`, `DISPROVEN`.
8. Pick at most one next ownership category per run.
9. If the next change cannot be made safely with the available mutation surface, produce an exact machine-actionable patch/handoff instead of fabricating a broad file replacement.
10. Do not merge PR #49 or any successor PR without explicit user approval.

## Safe unattended actions

These are allowed when the evidence is exact and the change is narrow:

- read PR/commit/CI state;
- inspect source and tests;
- classify ownership/lifetime behavior;
- add or tighten characterization tests that assert already-observed behavior;
- add narrow documentation of observed contracts;
- add read-only/static ratchets that prevent known architectural debt from spreading;
- prepare exact drift-checked migration scripts or patch handoffs;
- update the rolling architecture checkpoint in the chat response.

## Actions requiring the user or an interactive implementation pass

Do not perform these merely because an hourly run has time:

- merge to `main`;
- broad production refactors;
- protocol-visible layout changes;
- gameplay-value changes;
- architectural rewrites such as ECS/framework introduction;
- whole-file replacement of large owner files when only a small hunk is intended;
- removal of a current behavior unless a test/evidence path shows it is accidental and the user has asked to change it.

## Current migration order

1. Local settings ownership: replace settings writes through `networkMutableState()` with a named `Game` settings API. The current exact migration helper is `tools/apply_local_settings_ownership.py`.
2. Authoritative snapshot application: replace generic mutable-state access for true network snapshot application with a named API.
3. Continue lifetime contracts around room/player/run boundaries.
4. Energy/survival transaction: characterize precedence before decomposition.
5. Room/deposit transaction.
6. Per-player runtime/network peer isolation.
7. Simulation -> presentation derivation.

Do not skip ahead merely because a later refactor looks more interesting.

## Required hourly output

Keep the result compact and useful. Report only meaningful change. Use this shape:

```text
ARCH-HOURLY-<monotonic number>
HEAD=<sha>
CI=<PROVEN_GREEN|FAIL|RUNNING|UNKNOWN>
NEW_EVIDENCE=<one sentence or NONE>
OWNERSHIP_SURFACE=<same|shrunk|expanded>
SAFE_NEXT=<one concrete next action>
BLOCKER=<concrete blocker or NONE>
```

Then explain the teaching point in plain conversational language: what the evidence means, what concept it demonstrates, and why the next action follows. Do not drown the user in raw logs.

If nothing material changed, say so. Do not generate filler work to justify the hourly run. Apparently the computer is allowed to have restraint.
