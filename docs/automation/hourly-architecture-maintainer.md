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

## Bout execution discipline

Longer interactive or unattended work bouts must optimize for engineering evidence, not for keeping the agent busy.

Before doing work, state internally:

```text
CLAIM=<one unresolved engineering claim>
EVIDENCE_TARGET=<one test/runtime/source fact that can resolve it>
MUTATION_BOUNDARY=<at most one coherent code/contract boundary>
STOP_WHEN=<specific evidence or blocker>
```

Rules:

1. One bout should normally resolve one claim. Do not chain into a second subsystem merely because the first finished early.
2. Prefer an existing test target, CI path, source owner, or connector operation over creating new execution infrastructure.
3. Temporary workflows/helpers are justified only when the same execution problem is likely to recur or no existing path can safely perform the change. One awkward edit is not infrastructure demand.
4. Tool friction must not decide architecture. Reuse execution infrastructure, but do not put a contract in the wrong conceptual owner merely to avoid a build-file edit.
5. Poll CI only when the answer changes the next action. While CI runs, do static review or stop; repeated unchanged status reads are waste.
6. Update PR/public prose once after the engineering truth settles, not after every intermediate checkpoint.
7. Do not add another production mutation while a previous mutation's verification is unresolved.
8. A green characterization test is evidence, not automatic permission to refactor the characterized subsystem immediately.
9. Prefer high evidence-per-mutation work: a small runtime test proving several precedence branches is better than a larger extraction that merely makes the code look organized.
10. Stop when the declared stopping condition is met. Restraint is part of the workflow.

The target is not maximum commits per hour. It is maximum reduction in uncertainty and ownership ambiguity per mutation.

## Hourly loop

1. Reconstruct the compact working set above.
2. Read CI/check status for the exact current head. Do not infer green from an older commit.
3. Compare against the last meaningful checkpoint when available; inspect only deltas unless broader evidence is required.
4. Check the `networkMutableState()` ownership surface and classify any new production call site as a regression.
5. Select the highest-priority unresolved seam from the migration order below.
6. Define the bout claim/evidence/mutation/stop condition before editing.
7. Retrieve only the evidence required for that seam.
8. Prefer evidence in this order:
   - actual runtime/CI result;
   - focused tests/probes;
   - exact source behavior/call sites;
   - documented intent;
   - inference.
9. Label uncertain claims explicitly: `PROVEN`, `OBSERVED`, `HYPOTHESIS`, `UNCHARACTERIZED`, `DISPROVEN`.
10. Do at most one meaningful ownership/contract action per run.
11. Compress the result back into a small checkpoint with durable pointers to larger evidence.
12. Do not merge PR #49 or any successor PR without explicit user approval.

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
- prepare exact drift-checked migration scripts or patch handoffs when an existing execution path genuinely cannot perform the change safely;
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

Completed/narrowed work remains evidence, not a reason to repeat discovery:

1. **Local settings ownership — narrowed.** Persisted preferences use `applyLocalPreferences`; mobile framing has an explicit owner API rather than being restored as a generic local preference.
2. **Authoritative snapshot application — narrowed.** Desktop/Android adapters call the protocol-owned `applyWorld(Game&, ...)` boundary instead of taking generic mutable state.
3. **Room advancement lifetime policy — named.** `updateRoomTopology()` detects crossing and `advanceClearedRoom()` owns the characterized cleared-room transition/upgrade handoff.
4. **Energy/survival transaction — core precedence PROVEN.** Focused runtime contracts now cover supplemental-first loss, survival/impact mitigation, last stand, multiplayer downed, solo soul reboot, and non-hit exhaustion death. Remaining question: feedback publication/order and the smallest result boundary before any extraction.
5. **Projectile deposit / room-clear transaction.** Make deposit and room-clear ownership explicit while preserving same-frame order.
6. **Per-player runtime/network peer isolation.** Continue reducing whole-player context swapping only behind existing isolation evidence.
7. **Desktop UI/session mutable-state debt.** `native-desktop/main.cpp` remains a broad mutable-state production caller for menu/session handling and built-in stress fixtures; separate this by conceptual owner rather than treating zero mutable-call count as the goal.
8. **Simulation -> presentation derivation.** Keep presentation adapters read-oriented and share evaluators only where behavior is intentionally common.

Do not skip ahead merely because a later refactor looks more interesting.

## Evidence storage

Large/raw evidence should remain where it naturally lives:

- source truth -> repository path + commit SHA;
- CI truth -> workflow/run/job identifier;
- architecture decisions -> focused contract docs/tests;
- exact proposed edits -> drift-checked scripts/patch handoffs only when needed;
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
EVIDENCE_PTR=<commit/file/test/workflow pointer or NONE>
OWNERSHIP_SURFACE=<same|shrunk|expanded>
STATE=<smallest sufficient current conclusion>
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
