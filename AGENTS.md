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

## Validation

Before editing, verify that:

reference/browser-pass7/index_module.mjs

exists.

Inspect the browser implementation before changing native code.

Do not substitute another runtime if this file exists.

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

### Gameplay verification

Run the native desktop build and available gameplay/protocol tests, including where present:

- `Pass7ParityTest`
- `MultiplayerProtocolTest`
- smoke test
- model test
- proximity test
- `git diff --check`

Report exactly which commands ran and which could not run.
