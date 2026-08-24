# DATA CommandHUD core

`tools/hud` is an experimental local command wrapper for DATA development. It verifies project identity, records Git authority around commands, preserves raw output, and emits a deterministic reduced result.

It is not the game runtime, not a replacement for Git, and not release authority. GitHub Actions and repository-specific build/release scripts remain authoritative for releases.

## Install the local entrypoint

From the repository root:

```text
npm link
```

This exposes the repository's `hud` bin through npm's user-level binary directory. It does not install a service or start a UI. Remove it with:

```text
npm unlink --global digital-breakdown
```

Without linking, use:

```text
npm run hud -- <command>
```

The package name is `digital-breakdown`; the repository directory may still be named `digital-breakdown-apk`.

## Current published commands

Examples:

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

`hud run` streams the child command while writing separate raw stdout and stderr logs. Use `--quiet` when only the final reduced packet is wanted.

The underlying command's failure is preserved rather than converted into success. The current exit-code convention is:

```text
PASS     0
FAIL     1
BLOCKED  2
ERROR    3
```

`ERROR` is reserved for HUD transport/tooling failures rather than ordinary command failure.

## Evidence and state

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

Each `run.json` is created as an immutable command record and points to the raw stdout/stderr evidence for that run. It records repository currency before and after execution.

`state.json` contains only small mutable pointers/context such as the most recent run plus the current objective and frontier. Command history is derived from immutable run records rather than maintained as a second history database.

`hud continue` compares durable evidence with the current HEAD and a content fingerprint of tracked files plus non-ignored untracked files. Evidence is classified as `CURRENT`, `STALE`, or `UNKNOWN`. Older records without currency information remain readable and are classified `UNKNOWN` rather than guessed to be current.

Set `HUD_STATE_ROOT` to isolate state in tests or automation.

## Authority and safety

The core intentionally distinguishes a directory from an authoritative project.

- A Git repository without `distribution/project.json` identifying `indrolend/data` is rejected as a DATA project.
- When invoked outside a repository, HUD may use the last verified DATA registration.
- When invoked inside a different repository, HUD fails instead of silently switching to DATA.
- HUD does not commit, push, publish, deploy, reset, or clean by default.
- Repository scripts remain verification adapters; GitHub Actions remains release authority.
- Raw evidence is retained even when a shorter semantic presentation is shown.

A separate Windows CommandHUD bridge may use this CLI when it is running inside a repository that contains a compatible `tools/hud/cli.mjs`. The bridge and this core have different responsibilities: the bridge owns local UI/PowerShell transport, while this core owns DATA-specific verification and semantic evidence.

## Development status

This README documents the behavior published on the branch containing it. Local or unmerged branches may contain additional workflow, state-projection, or visual-renderer experiments. Those should not be treated as public capabilities until they are merged.

Run the HUD tests from the repository root with:

```text
npm run hud:test
```
