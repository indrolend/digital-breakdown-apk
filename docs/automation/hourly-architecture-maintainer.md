# Hourly Architecture Maintainer

Purpose: make recurring ChatGPT runs useful for behavior-preserving architecture work while minimizing local prompt/context size and using durable external storage as long-term engineering memory.

## Scope

Repository: `indrolend/digital-breakdown-apk`
Primary work item: draft PR #49 on branch `agent/ownership-contracts-settings`.
Base contract: `main@14ba2e530ff6f7d66dde6ef01a6fb75a7688b674` unless the PR is explicitly rebased and the contract docs are updated.

## Governing rule

One state value -> one canonical owner -> named mutation path -> explicit result/event when other systems must react.

Broad mutable `GameState&` access is transitional infrastructure, not an ownership model.

## Memory model

Treat the world outside the current prompt as durable memory. Do not drag history into the active context merely because it exists.

Use this hierarchy:

```text
RAW ARCHIVE
source, logs, CI output, PR threads, commits, artifacts
        ↓
EVIDENCE
exact excerpts / identifiers needed to support a claim
        ↓
STATE
what is currently true
        ↓
CONTRACT
what must remain true
        ↓
NEXT ACTION
one bounded thing worth doing now
```

The active prompt is a cache, not the archive.

Default objective:

```text
minimum local context
that preserves
maximum useful continuity
```

Fetch deeper history only when the current state is insufficient, contradictory, or needs provenance.

## Local-prompt-first startup

At the start of each run, reconstruct only this working set:

1. current PR #49 head SHA and base;
2. CI/check state for that exact head;
3. current ownership surface / known ratchets;
4. current migration target;
5. current unresolved uncertainty;
6. links/paths/SHAs for evidence required to resolve that uncertainty.

Do not reread every contract document, PR comment, or old log by default. Read only the smallest relevant section needed for the current seam. If evidence is already encoded by a passing focused test or ratchet, prefer that over reopening the entire historical discussion.

## Hourly loop

1. Reconstruct the compact working set above.
2. Read CI/check status for the exact current head. Do not infer green from an older commit.
3. Compare against the last meaningful checkpoint when available; inspect only deltas unless broader evidence is required.
4. Check the `networkMutableState()` ownership surface and classify any new production call site as a regression.
5. Select the highest-priority unresolved seam from the migration order below.
6. Retrieve only the evidence required for that seam.
7. Prefer evidence in this order:
   - actual runtime/CI result;
   - focused tests/probes;
   - exact source behavior/call sites;
   - documented intent;
   - inference.
8. Label uncertain claims explicitly: `PROVEN`, `OBSERVED`, `HYPOTHESIS`, `UNCHARACTERIZED`, `DISPROVEN`.
9. Do at most one meaningful ownership/contract action per run.
10. Compress the result back into a small checkpoint with durable pointers to larger evidence.
11. Do not merge PR #49 or any successor PR without explicit user approval.

## Commit threshold

Hourly analysis is allowed. Hourly commits are not automatic.

A repo mutation is justified only when all are true:

1. evidence is strong enough to state the behavior/contract precisely;
2. the change is narrow and mechanically reviewable;
3. it creates durable value for later runs (test, ratchet, contract, or exact migration aid);
4. it is not merely recording status that belongs in the hourly checkpoint;
5. it does not create redundant documentation already represented by an existing test/contract.

If those conditions are not met, do not commit. Produce a compact next-action handoff instead.

Analysis is hourly. Commits are earned.

## Priority order

Always prefer finishing work already in flight over discovering new work:

1. resolve CI/evidence already in flight;
2. strengthen an existing contract/test where a real gap is known;
3. shrink a known ownership escape hatch;
4. characterize the next uncertain subsystem;
5. only then discover a new seam.

Do not wander into unrelated systems because they look interesting.

## Safe unattended actions

Allowed when evidence is exact and the action clears the commit threshold:

- read PR/commit/CI state;
- inspect source and tests;
- classify ownership/lifetime behavior;
- add or tighten characterization tests that assert already-observed behavior;
- add narrow documentation only when the contract is not already represented elsewhere;
- add read-only/static ratchets that prevent known debt from spreading;
- prepare exact drift-checked migration scripts or patch handoffs;
- produce compact architecture checkpoints with durable evidence pointers.

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

1. Local settings ownership: replace settings writes through `networkMutableState()` with a named `Game` settings API. Current exact migration helper: `tools/apply_local_settings_ownership.py`.
2. Authoritative snapshot application: replace generic mutable-state access for true network snapshot application with a named API.
3. Continue lifetime contracts around room/player/run boundaries.
4. Energy/survival transaction: characterize precedence before decomposition.
5. Room/deposit transaction.
6. Per-player runtime/network peer isolation.
7. Simulation -> presentation derivation.

Do not skip ahead merely because a later refactor looks more interesting.

## Evidence storage

Large/raw evidence should remain where it naturally lives:

- source truth -> repository path + commit SHA;
- CI truth -> workflow/run/job identifier;
- architecture decisions -> focused contract docs/tests;
- exact proposed edits -> drift-checked scripts/patch handoffs;
- chronological state -> hourly checkpoints.

Do not copy large source excerpts or logs into checkpoints when a durable pointer plus one-line conclusion is enough.

## Required hourly output

Keep the result compact enough to serve as the next run's local working state.

```text
ARCH-HOURLY-<monotonic number>
HEAD=<sha>
BASE=<sha>
CI=<PROVEN_GREEN|FAIL|RUNNING|UNKNOWN>
FOCUS=<single ownership/contract seam>
NEW_EVIDENCE=<one sentence or NONE>
EVIDENCE_PTR=<path/commit/run/test pointer or NONE>
OWNERSHIP_SURFACE=<same|shrunk|expanded>
STATE=<smallest sufficient current truth>
SAFE_NEXT=<one concrete next action>
BLOCKER=<concrete blocker or NONE>
```

Then add at most a short plain-language teaching explanation when something meaningful was learned. Do not restate old history unless it is required to explain the delta.

If nothing material changed, emit the checkpoint with `NEW_EVIDENCE=NONE` and stop. Do not generate filler work to justify the hourly run. Apparently the computer is allowed to have restraint.

## Morning synthesis

On the first run after 07:00 America/Chicago, if multiple overnight checkpoints exist, additionally produce one compact synthesis:

```text
OVERNIGHT_DELTA=<what actually changed>
PROVEN=<new truths only>
UNCERTAIN=<remaining uncertainty only>
COMMITS_EARNED=<count / brief purpose>
NEXT_DECISION=<one thing that most benefits from the user's attention>
```

Do not replay every hourly checkpoint. Compress them.
