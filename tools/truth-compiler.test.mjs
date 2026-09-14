#!/usr/bin/env node

import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import path from "node:path";

const root = path.resolve(import.meta.dirname, "..");
const raw = execFileSync(process.execPath, [path.join(root, "tools/truth-compiler.mjs"), "--json"], { encoding: "utf8" });
const result = JSON.parse(raw);

assert.equal(result.schemaVersion, 1);
assert.equal(result.authorities.productExecutable, "DigitalBreakdown");
assert.equal(result.authorities.gameplay, "native/game");
assert.equal(result.authorities.build, "native-desktop/CMakeLists.txt");
assert.equal(result.authorities.testRunner, "CTest");
assert.equal(result.invariants.historicalProseIsRuntimeAuthority, false);
assert.equal(result.invariants.singleProductExecutable, true);
assert.ok(result.authoritativeTruthSurface.files > 0);
assert.ok(result.authoritativeTruthSurface.files < result.tracked.files);
assert.ok(result.authoritativeTruthSurface.bytes > 0);
assert.match(result.authoritativeTruthSurface.digest, /^[0-9a-f]{16}$/);

const actions = result.authorities.semanticActions.map(x => x.name);
for (const expected of ["play", "check", "prove", "explore", "ship"]) assert.ok(actions.includes(expected), `missing ${expected}`);

console.log("TRUTH_COMPILER=PASS");
