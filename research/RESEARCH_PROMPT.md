# Digital Breakdown Runtime Parity Research

Audit the attached current repository snapshot. Do not design a new game architecture from general principles.

The JavaScript browser runtime is the behavioral authority. The native Android C++ runtime already builds and runs, but camera and movement behavior differ. The objective is to identify and close the current behavioral gap with the fewest code changes and the fewest dependencies possible.

## Constraints

- Ground every conclusion in the attached files.
- Prefer plain C++17 and direct platform APIs.
- Do not recommend a game engine, ECS framework, React, Vue, Vite, Electron, Tauri, or generalized middleware.
- Do not redesign functioning systems merely to conform to common industry architecture.
- External research is allowed only when it answers a concrete uncertainty exposed by the current code.
- Runtime behavior and observable results matter more than conceptual elegance.

## Reconstruct the current system

Identify the browser and native functions that own:

- frame timing
- input
- movement
- jumping
- camera
- player rotation
- vacuum movement effects
- battery movement effects

Determine whether `native/core` and `native-android/app/src/main/cpp/game` are currently separate simulation implementations. Explain their real ownership and overlap from the code.

## Exact browser/native comparison

For movement, extract all constants, equations, normalization, camera-relative basis calculations, acceleration, speed limiting, friction, gravity, ground/air control, jump/double-jump behavior, coyote time, jump buffering, landing momentum, rotation interpolation, battery scaling, vacuum slowdown, and update order.

For camera, extract input source and units, sensitivity, dt treatment, yaw/pitch accumulation, clamps, target calculation, offset, distance, height, first-person calculation, smoothing, look-at calculation, collision behavior, and update order relative to movement.

Present a table with:

- Behavior
- Browser file/function
- Browser equation/constants
- Native file/function
- Native equation/constants
- Observable mismatch
- Minimum correction

Do not say only “similar” or “different.” Show equations, signs, coordinate axes, constants, and execution order.

## Minimal parity test

Design the smallest dependency-free test that:

- records a fixed sequence of normalized input
- executes the same sequence in browser and native
- exports JSONL or CSV per simulation step
- compares player position, velocity, yaw, grounded state, battery, camera yaw/pitch, and camera position
- uses justified tolerances
- distinguishes timestep divergence from equation divergence

Provide minimal patches or pseudocode against the current files.

## Current decisions

Answer:

1. Should parity corrections happen first in current Android `Game.cpp`, or immediately move into `native/core`?
2. What is the least disruptive extraction sequence?
3. Which duplicated constants or structures already create risk?
4. Which abstractions should not be created yet?
5. Is any external dependency actually necessary now?
6. What must be measured at runtime instead of inferred?

## External research rules

For every external source, state the exact repository question it answers. Prefer official language/compiler/Android documentation and primary source code. Exclude unrelated engine and architecture information.

## Final output

A. Current runtime map  
B. Browser/native movement comparison  
C. Browser/native camera comparison  
D. Exact mismatch ranking  
E. Minimal trace/replay design  
F. Least disruptive migration plan  
G. Five immediate changes ordered by leverage  
H. Questions requiring direct runtime observation
