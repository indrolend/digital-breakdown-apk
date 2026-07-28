# Digital Breakdown Native Parity

## Authoritative Browser Reference

The authoritative browser implementation is committed inside this repository:

reference/browser-pass7/index_module.mjs

Do not use any files under:

www/runtimes/

as behavioral references.

Those are historical implementations only.

## Goal

Translate the browser runtime into shared native C++ while preserving observable behavior.

Do not redesign mechanics.

## Rules

- Preserve constants.
- Preserve update order.
- Preserve state transitions.
- Preserve coordinate conventions.
- Preserve input semantics.
- Preserve observable behavior.
- Only modify the requested subsystem.
- Do not touch unrelated gameplay.
- Gameplay simulation should remain shared between desktop and Android.
- Do not embed JavaScript or Three.js into the native runtime.

## Gameplay architecture invariants

- Keep `TargetState` pooled and fixed-size.
- Derive active-human and loose-soul roles from existing state; do not add parallel entity hierarchies.
- Do not introduce ECS, dynamic allocation, or broad architecture rewrites during focused fixes.
- Keep capture commit and inventory awarding in the existing capture transaction.
- Keep network ownership and protocol-visible layout stable unless protocol work is explicitly requested.
- Treat `TargetState::pos` as canonical simulation position.
- Keep hover, spin, lattice deformation, and other presentation offsets visual-only.
- Prefer focused modules under `game/gameplay/` over adding more unrelated logic to `Game.cpp`.

## Validation

Before editing, verify that:

reference/browser-pass7/index_module.mjs

exists.

Inspect the browser implementation before changing native code.

Do not substitute another runtime if this file exists.

For gameplay changes, run one of:

```bash
bash scripts/verify-gameplay.sh
```

```powershell
./scripts/verify-gameplay.ps1
```

The canonical focused suite includes role/soul-motion checks, browser parity smoke coverage, multiplayer protocol coverage, and `git diff --check`.

## Maintainable gameplay editing contract

### Source of truth

- Continue gameplay architecture work from `agent/gameplay-architecture-distillation` or a branch explicitly based on it.
- Inspect the current branch and current source before editing. Do not resume from stale snippets or older rescue branches.

### Gameplay invariants

- `TargetState` remains a fixed pooled object reused across inactive, human, and loose-soul roles.
- Human-to-soul conversion occurs in place. Do not introduce separate soul entities or an ECS.
- `captureSoul()` remains the inventory-awarding capture transaction.
- Network and protocol layout must not change without explicit protocol tests and approval.
- Simulation coordinates remain canonical. Hover, bob, spin, squash, and other presentation offsets must not mutate simulation position.
- Prefer derived role predicates over new replicated flags.

### Change shape

- Prefer focused source files under roughly 500 lines.
- Keep orchestration in `Game.cpp`; move cohesive behavior into narrow gameplay modules.
- Avoid broad formatting, unrelated cleanup, dynamic allocation, and framework rewrites.
- Make dependencies explicit through small context structs rather than passing the entire `Game` object.

# Digital Breakdown Multiplayer Development Instructions

## Current multiplayer initiative

Primary development branch:

`agent/windows-multiplayer-visual-pause-fix`

This branch is becoming the complete online multiplayer parity pull request.

Latest validated checkpoint at the time these instructions were written:

`f77beaf17603e5a21cac46dfd331650d39ab48b6`

Before working, verify the current remote branch tip rather than assuming this SHA remains current.

## Architecture

- Native desktop client is Windows-focused.
- Gameplay code is shared under `native-android/app/src/main/cpp/game`.
- Desktop transport, rendering, menus, input, and window behavior are under `native-desktop`.
- Binary multiplayer protocol is under `native-network`.
- Cloudflare relay Worker is under `multiplayer-server`.
- Multiplayer remains host-authoritative.
- Guests may predict local presentation and locomotion but must not decide authoritative outcomes.

## Current versions

- Multiplayer protocol: 7
- Gameplay version: 5
- The deployed production Worker may still be protocol 5.
- Do not assume production is compatible with protocol-7 clients.
- Do not deploy the Worker without explicit user approval.

## Completed multiplayer work

The branch includes:

- multiplayer Escape/menu behavior fixes
- independent local multiplayer pause behavior
- guest locomotion and jump prediction/reconciliation
- expanded authoritative snapshots
- guest enemy visual reconstruction
- buffered remote interpolation
- state hashing and parity diagnostics
- a Windows multiplayer parity harness
- local protocol-7 Worker validation

## Current objective

Continue toward a complete online multiplayer parity pull request.

Prioritize:

1. Remote player action-pose parity
2. Enemy attack and soul-transition parity
3. Projectile and room-transition parity
4. Guest movement quality under latency and jitter
5. Lifecycle behavior:
   - pause/resume
   - explicit leave
   - host departure
   - guest disconnect
   - timeout
   - version mismatch
6. Cross-machine and impaired-network acceptance tests
7. Long-duration soak validation

## Required safety rules

Do not:

- deploy Cloudflare without explicit permission
- publish a GitHub release
- merge into main
- force-push
- modify or delete backup branches
- use `git reset --hard`
- amend previously pushed commits unless explicitly requested
- include build outputs, logs, executables, caches, Wrangler state, or `node_modules` in commits
- perform a broad architecture rewrite without first presenting evidence and a plan

## Git workflow

Before editing:

```powershell
git status --short --branch
git fetch origin --prune
git log -5 --oneline --decorate
```

Do not overwrite uncommitted work.

Before committing:

```powershell
git diff --stat
git diff --check
git status --short
```

Stage exact paths only. Do not use:

```powershell
git add .
git add -A
```

After pushing, verify:

```powershell
git rev-parse HEAD
git rev-parse origin/agent/windows-multiplayer-visual-pause-fix
git rev-list --left-right --count origin/agent/windows-multiplayer-visual-pause-fix...HEAD
```

The two SHAs must match and divergence must be `0 0`.

## Standard validation

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\verify-gameplay.ps1
cmake --build .\build\windows-release --config Release --parallel
ctest --test-dir .\build\windows-release `
  -C Release `
  --output-on-failure
```

Worker validation:

```powershell
Push-Location .\multiplayer-server
npm.cmd run protocol:check
npm.cmd test
Pop-Location
```

Parity validation should use a protocol-7 local or staging Worker, not the protocol-5 production Worker:

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\tools\test_multiplayer_parity_windows.ps1 `
  -Exe .\build\windows-release\bin\Release\DigitalBreakdown.exe `
  -ServiceUrl http://127.0.0.1:<worker-port> `
  -TimeoutSeconds 90
```

Expected success marker:

`MULTIPLAYER_PARITY_OK room=<code>`

## Build provenance

Uncommitted source changes do not produce a new Git commit identity. Do not claim a new build SHA until the changes are committed and the project is rebuilt.
