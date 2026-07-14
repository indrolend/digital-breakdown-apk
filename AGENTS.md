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
