# Automation Memory and Evidence Contract

Purpose: make recurring agents accumulate useful knowledge instead of repeating work, drifting stale claims, or turning private process into public noise.

## Core model

Treat each automated run as a bounded data-processing step:

```text
raw archive / repo / CI / prior positions
    -> retrieve minimum relevant evidence
    -> classify current claim/state
    -> deduplicate against known work
    -> produce one bounded result
    -> store durable pointers + compact checkpoint
```

The active prompt is cache. GitHub, development-session archives, CI, commits, artifacts, and durable docs are storage.

## Canonical identifiers and lineage

Every durable automated claim should be scoped to explicit identifiers whenever available:

- repository
- PR / issue number
- branch
- exact head SHA
- base SHA
- workflow run / job ID
- source path / symbol
- prior position or checkpoint number

Do not write `CI passed` when the exact head is unknown. Do not carry a claim from head A to head B without revalidation when the relevant code changed.

Use `SUPERSEDES` or `RELATED_TO` pointers when revisiting an existing claim instead of creating a parallel duplicate.

## Claim schema

For substantive automated reasoning, capture these fields conceptually even when the user-facing form is compact:

```text
CLAIM_ID=<stable role/checkpoint identifier>
SUBJECT=<symbol/subsystem/public surface>
STATUS=<HYPOTHESIS|OBSERVED|PROVEN|DISPROVEN|DEFERRED>
VALID_FOR=<exact head/ref or public surface state>
EVIDENCE=<durable pointers>
SUPERSEDES=<prior claim or NONE>
FALSIFIER=<what evidence would overturn it>
CONFIDENCE=<HIGH|MEDIUM|LOW with evidence basis>
ACTION=<none|test|doc|ratchet|patch-handoff|public-edit>
```

A claim without scope or provenance is commentary, not durable state.

## Deduplication

Before producing a new claim or action:

1. Search the current contract docs / ledger / prior checkpoints for the same subject and conclusion.
2. If equivalent evidence already exists, do not create a new commit or verbose restatement.
3. If new evidence strengthens or weakens an existing claim, update/supersede that claim explicitly.
4. Prefer one canonical current conclusion with links to prior history over multiple near-identical summaries.

`NEW_EVIDENCE=NONE` is a valid result.

## Freshness and invalidation

Treat claims as cache entries with validity scope.

Revalidate when:

- the exact head changed in a file relevant to the claim;
- the base branch changed or was rebased;
- CI results belong to an older head;
- protocol/gameplay/save versions changed;
- a public PR body or README changed underneath the PR Agent;
- a previously missing physical/runtime test becomes available.

Do not invalidate unrelated claims merely because any commit occurred. Scope invalidation to the relevant subsystem/files when possible.

## Retrieval quality

Do not fetch the whole archive by default.

Start with a precise question and retrieve only enough evidence to resolve it. Prefer:

1. exact current state/checkpoint;
2. targeted source/test/CI evidence;
3. matching archived development session when historical rationale matters;
4. broader history only when ambiguity remains.

If retrieval is weak or ambiguous, mark the result `DEFERRED` or `UNCHARACTERIZED` rather than filling gaps with plausible prose.

## Evaluation fields

Each meaningful run should make it possible to judge whether the automation was useful. Track compactly where applicable:

- `NOVELTY=<NEW|STRENGTHENED|DUPLICATE|NONE>`
- `EVIDENCE_GAIN=<what became better known>`
- `ACTION_VALUE=<durable change / handoff / none>`
- `FALSE_POSITIVE_RISK=<LOW|MEDIUM|HIGH>`
- `CONTEXT_EFFICIENCY=<targeted|broad>`

Morning synthesis should summarize not only what agents said, but what survived challenge and produced durable value.

## Sensitivity and publication boundaries

Storage abundance does not imply publication permission.

Classify information by destination:

```text
PRIVATE_ARCHIVE
    full ChatGPT development sessions, personal context, raw reasoning, local paths/logs when unnecessary publicly

ENGINEERING_ARCHIVE
    commits, CI, artifacts, technical docs, agent ledgers suitable for project history

PUBLIC_INDEX
    README, PR/issue titles/bodies, release notes, concise current explanations
```

Never promote private/personal material into public GitHub merely because it helped reconstruct the work. Public summaries may use the technical conclusion without exposing irrelevant personal/session details.

## Mutation threshold

Recurring analysis is cheap; durable mutations must earn existence.

Commit/edit only when all are true:

- evidence is sufficiently strong;
- the action is non-duplicate;
- validity scope is explicit;
- it creates durable navigational, contractual, test, or enforcement value;
- rollback/review surface is narrow;
- it respects the agent's safety boundary.

Otherwise emit a handoff or no-op.

## Morning / periodic compaction

Periodic synthesis should perform compaction, not concatenation:

```text
accepted current claims
+ strongest evidence pointers
+ disproven/superseded claims worth remembering
+ unresolved uncertainty
+ durable actions earned
+ next decision
```

Do not replay every hourly position.

The goal is increasing information density over time: more evidence in storage, less context required to correctly resume work.
