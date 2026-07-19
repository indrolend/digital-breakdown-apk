# Pass 7 executable oracle

This tool generates an isolated, instrumented host for the authoritative browser runtime at
`reference/browser-pass7/index_module.mjs`. It never edits the reference directory.

Build the host:

```powershell
node tools/pass7-oracle/build-oracle.mjs
```

Serve the repository root and open the oracle:

```powershell
node tools/pass7-oracle/serve-oracle.mjs
```

Then open `http://127.0.0.1:8765/build/pass7-oracle/index.html`. When the module is
ready, `window.__PASS7_ORACLE__` provides deterministic `start`, `step`, input, fixture placement,
and `snapshot` operations.

The generated `oracle-manifest.json` records the authoritative source hash and the two generated-only
hooks used to control `frame.now` and `frame.dt`. Gameplay logic is not copied into the bridge.

Example in browser developer tools:

```js
const oracle = window.__PASS7_ORACLE__;
oracle.start();
oracle.clearSouls();
oracle.placeSoul(0, { position: [0, 0.5, 11.5], slurpable: true });
oracle.setVacuum(true);
const trace = oracle.step(120, 1 / 60);
console.log(trace);
```

Each snapshot includes the actual camera world direction, phone and screen world matrices, vacuum
state, computed crosshair spread/rotation, and active soul lifecycle state. Native comparison traces
should use the same field names and units.

The generated host automatically runs the vacuum/crosshair suite at 60 FPS and posts the full trace
back to the local server at `build/pass7-oracle/traces/pass7-vacuum-crosshair-suite.json`. The suite
covers free, attracted, and latched aim loss; reacquisition; seven camera pitches; and rapid camera/
phone-screen divergence. It also records flower-stack capacity and supplemental-power spending
priority against a fixed one-second 60 FPS vacuum trace.

Verify the recorded state invariants:

```powershell
node tools/pass7-oracle/verify-suite.mjs
```

Audit and extract the authoritative embedded GLBs:

```powershell
node tools/pass7-oracle/extract-assets.mjs
```

Generated GLBs and reports are written to `build/pass7-oracle/assets/`. The
report includes hashes, scene structure, exact transformed mesh bounds,
materials, and the browser phone-height normalization.
