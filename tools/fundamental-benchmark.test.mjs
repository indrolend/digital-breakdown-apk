#!/usr/bin/env node

import test from "node:test";
import assert from "node:assert/strict";
import { runFundamentalBenchmarks, correctnessDigest } from "./fundamental-benchmark.mjs";

test("fundamental compute benchmark remains deterministic and measurable", () => {
  const results = runFundamentalBenchmarks();
  const byName = Object.fromEntries(results.map(r => [r.name, r]));

  for (const name of ["xorshift32", "float64_affine", "sha256", "json_roundtrip", "recurrent_policy_10x8", "truth_compiler"]) {
    assert.ok(byName[name], `missing benchmark ${name}`);
    assert.ok(Number.isFinite(byName[name].ms) && byName[name].ms > 0, `${name} duration invalid`);
  }

  assert.equal(byName.xorshift32.check, 2669331932);
  assert.equal(byName.sha256.check, "b56fbd6a698b3597");
  assert.ok(byName.truth_compiler.files > byName.truth_compiler.truthFiles);
  assert.ok(byName.truth_compiler.bytes >= byName.truth_compiler.truthBytes);

  const digest = correctnessDigest(results);
  assert.match(digest, /^[0-9a-f]{16}$/);

  console.log(`BENCHMARK_CI semantic_digest=${digest}`);
  for (const r of results) console.log(`BENCHMARK_CI name=${r.name} median_ms=${r.ms.toFixed(3)}`);
});
