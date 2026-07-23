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
