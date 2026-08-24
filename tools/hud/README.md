# DATA CommandHUD core

`hud` is a local development front door for DATA. It verifies the DATA repository, records command evidence and Git state around each run, preserves raw stdout/stderr, and emits a deterministic continuation packet.

The core deliberately separates **evidence** from **presentation**. Raw command output, exit status, command text, and recorded Git state are the underlying evidence. Reduced summaries such as test counts or failure labels are convenience views over that evidence, not a replacement for it.

## Install the local entrypoint

From the repository root:

```text
npm link
```

This exposes the repository's `hud` bin through npm's user-level binary directory. It does not install a service or start a UI. Remove it with `npm unlink --global digital-breakdown`.

Without linking, use `npm run hud -- <command>`.

## Workflow

```text
hud context
hud objective "Investigate Windows renderer regression"
hud frontier "Rebuild and visually verify shadows"
hud continue
hud run --objective "Verify native gameplay" npm test
hud packet --copy
hud history
hud last --json
hud tools
hud update
```

`hud run` streams the command while writing separate raw stdout and stderr logs. Use `--quiet` when only the final reduced packet is wanted. A nonzero underlying command remains nonzero: `PASS` exits 0, `FAIL` exits 1, `BLOCKED` exits 2, and HUD transport/tooling `ERROR` exits 3.

## State

Transient HUD state does not enter the repository. The default Windows location is:

```text
%LOCALAPPDATA%\CommandHud\
```

Layout:

```text
CommandHud/
  projects/
    indrolend_data.json
    indrolend_data/state.json
  runs/
    indrolend_data/<run-id>/
      run.json
      stdout.log
      stderr.log
```

Each run directory is created once by HUD. `stdout.log`, `stderr.log`, and `run.json` are written as run evidence and HUD does not rewrite an existing run record. They are ordinary local files, however, not cryptographically tamper-proof storage.

`state.json` contains only the most recent run pointer plus the current objective and frontier. History is derived from run records; there is no parallel history database.

## Evidence currency: repository scope only

`hud continue` compares recorded repository evidence with the current Git HEAD and a content fingerprint of tracked and non-ignored untracked files. The current implementation therefore answers a narrow question:

> Does this recorded run correspond to the same relevant repository bytes and HEAD that exist now?

The internal classifications are `CURRENT`, `STALE`, and `UNKNOWN`. They are **repository-evidence classifications**, not a claim that the entire execution environment is unchanged.

For example, repository evidence may remain `CURRENT` after changes to:

- environment variables,
- PATH or tool resolution,
- installed compiler/SDK versions,
- external services,
- machine configuration outside the repository.

Those conditions are not presently part of `repositoryCurrency()`. Callers and renderers should present the classification as repository-scoped (for example, `REPO_CURRENT`) rather than implying universal validity.

Ignored build/cache output does not affect the repository fingerprint because Git itself classifies that output as ignored. Relevant tracked or non-ignored untracked byte changes do affect it.

Set `HUD_STATE_ROOT` to isolate state in tests or automation.

## Output reduction

Reducers exist to remove presentation noise, not to invent authority. When a tool exposes a stable machine-readable format, that format should be preferred over heuristics. Regex/text reduction is a fallback for human-oriented terminal output.

A reduced result such as:

```text
21/21 node tests
```

means the reducer found matching evidence in the captured output. The raw stdout/stderr remain available for inspection and should be used whenever the reduction is insufficient or ambiguous.

## Authority and safety

- A Git repository without `distribution/project.json` identifying `indrolend/data` is rejected.
- When invoked outside any repository, HUD may use the last verified DATA registration.
- When invoked inside a different repository, HUD fails instead of silently switching roots.
- HUD never commits, pushes, publishes, deploys, resets, or cleans by default.
- Repository scripts remain the verification adapters; GitHub Actions remains release authority.

## What this core currently proves

The useful contract is intentionally smaller than a general-purpose "AI terminal":

1. execute a bounded command against the verified DATA repository;
2. retain its real stdout, stderr, exit status, timing, and Git state;
3. derive a compact deterministic presentation from that evidence;
4. later determine whether the recorded evidence is still current for the repository state it was tied to.

Visual CommandHUD experiments are renderers over this model. They should not become a parallel source of truth.
