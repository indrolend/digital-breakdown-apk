#!/usr/bin/env node

import crypto from "node:crypto";
import { performance } from "node:perf_hooks";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import path from "node:path";

const root = fileURLToPath(new URL("..", import.meta.url));

function median(values) {
  const v = [...values].sort((a, b) => a - b);
  return v[Math.floor(v.length / 2)];
}

function timed(fn, rounds = 5) {
  const samples = [];
  let value;
  for (let i = 0; i < rounds; i++) {
    const start = performance.now();
    value = fn();
    samples.push(performance.now() - start);
  }
  return { ms: median(samples), value };
}

export function benchmarkPrng(iterations = 8_000_000) {
  let x = 0x9e3779b9 >>> 0;
  const r = timed(() => {
    let local = x;
    for (let i = 0; i < iterations; i++) {
      local ^= local << 13;
      local ^= local >>> 17;
      local ^= local << 5;
      local >>>= 0;
    }
    x = local;
    return local;
  });
  return { name: "xorshift32", iterations, ms: r.ms, rate: iterations / (r.ms / 1000), check: r.value >>> 0 };
}

export function benchmarkFloatKernel(length = 262_144, passes = 64) {
  const a = new Float64Array(length);
  const b = new Float64Array(length);
  for (let i = 0; i < length; i++) { a[i] = (i % 997) * 0.001; b[i] = (i % 991) * 0.002; }
  const operations = length * passes * 4;
  const r = timed(() => {
    let checksum = 0;
    for (let p = 0; p < passes; p++) {
      const bias = p * 0.000001;
      for (let i = 0; i < length; i++) {
        const v = a[i] * 1.0000003 + b[i] * 0.4999997 + bias;
        b[i] = v - a[i] * 0.125;
      }
    }
    for (let i = 0; i < length; i += 4096) checksum += b[i];
    return checksum;
  }, 3);
  return { name: "float64_affine", operations, ms: r.ms, rate: operations / (r.ms / 1000), check: Number(r.value.toFixed(9)) };
}

export function benchmarkSha256(mebibytes = 32, passes = 8) {
  const bytes = mebibytes * 1024 * 1024;
  const buffer = Buffer.allocUnsafe(bytes);
  for (let i = 0; i < buffer.length; i += 4) buffer.writeUInt32LE((i * 2654435761) >>> 0, i);
  const totalBytes = bytes * passes;
  const r = timed(() => {
    let last = "";
    for (let p = 0; p < passes; p++) last = crypto.createHash("sha256").update(buffer).digest("hex");
    return last;
  }, 3);
  return { name: "sha256", bytes: totalBytes, ms: r.ms, rate: totalBytes / (r.ms / 1000), check: r.value.slice(0, 16) };
}

export function benchmarkJson(records = 80_000) {
  const data = Array.from({ length: records }, (_, i) => ({ i, x: (i * 17) % 1009, y: (i * 31) % 1013, alive: (i & 3) !== 0 }));
  const r = timed(() => {
    const text = JSON.stringify(data);
    const parsed = JSON.parse(text);
    return { bytes: Buffer.byteLength(text), check: parsed[records - 1].x + parsed[records - 1].y };
  }, 3);
  return { name: "json_roundtrip", records, bytes: r.value.bytes, ms: r.ms, rate: r.value.bytes / (r.ms / 1000), check: r.value.check };
}

export function benchmarkRecurrentPolicy(steps = 400_000) {
  const input = new Float64Array(10);
  const hidden = new Float64Array(8);
  const next = new Float64Array(8);
  const wIn = new Float64Array(80);
  const wRec = new Float64Array(64);
  for (let i = 0; i < wIn.length; i++) wIn[i] = Math.sin(i * 0.37) * 0.2;
  for (let i = 0; i < wRec.length; i++) wRec[i] = Math.cos(i * 0.23) * 0.15;
  const r = timed(() => {
    hidden.fill(0);
    for (let step = 0; step < steps; step++) {
      for (let i = 0; i < input.length; i++) input[i] = Math.sin((step + i) * 0.011);
      for (let h = 0; h < 8; h++) {
        let v = 0;
        for (let i = 0; i < 10; i++) v += input[i] * wIn[h * 10 + i];
        for (let j = 0; j < 8; j++) v += hidden[j] * wRec[h * 8 + j];
        next[h] = Math.tanh(v);
      }
      hidden.set(next);
    }
    return Array.from(hidden, x => Number(x.toFixed(9)));
  }, 3);
  return { name: "recurrent_policy_10x8", steps, ms: r.ms, rate: steps / (r.ms / 1000), check: r.value };
}

export function benchmarkTruthCompiler(rounds = 5) {
  const script = path.join(root, "tools", "truth-compiler.mjs");
  const times = [];
  let parsed;
  for (let i = 0; i < rounds; i++) {
    const start = performance.now();
    const r = spawnSync(process.execPath, [script, "--json"], { cwd: root, encoding: "utf8", windowsHide: true });
    const elapsed = performance.now() - start;
    if ((r.status ?? 1) !== 0) throw new Error(`truth compiler failed: ${r.stderr}`);
    parsed = JSON.parse(r.stdout);
    times.push(elapsed);
  }
  return {
    name: "truth_compiler",
    ms: median(times),
    files: parsed.tracked.files,
    bytes: parsed.tracked.bytes,
    truthFiles: parsed.authoritativeTruthSurface.files,
    truthBytes: parsed.authoritativeTruthSurface.bytes,
    check: parsed.authoritativeTruthSurface.digest
  };
}

export function runFundamentalBenchmarks() {
  return [
    benchmarkPrng(),
    benchmarkFloatKernel(),
    benchmarkSha256(),
    benchmarkJson(),
    benchmarkRecurrentPolicy(),
    benchmarkTruthCompiler()
  ];
}

export function correctnessDigest(results) {
  const stable = results.map(({ name, check, files, bytes, truthFiles, truthBytes }) => ({ name, check, files, bytes, truthFiles, truthBytes }));
  return crypto.createHash("sha256").update(JSON.stringify(stable)).digest("hex").slice(0, 16);
}

function formatRate(result) {
  if (result.name === "sha256" || result.name === "json_roundtrip") return `${(result.rate / (1024 * 1024)).toFixed(1)} MiB/s`;
  if (result.name === "truth_compiler") return `${result.ms.toFixed(2)} ms`;
  return `${(result.rate / 1e6).toFixed(2)} M/s`;
}

function main() {
  const results = runFundamentalBenchmarks();
  const digest = correctnessDigest(results);
  console.log(`FUNDAMENTAL_BENCHMARK platform=${process.platform} arch=${process.arch} node=${process.version}`);
  for (const r of results) {
    console.log(`BENCHMARK_RESULT name=${r.name} median_ms=${r.ms.toFixed(3)} rate=${formatRate(r)}`);
  }
  const truth = results.find(r => r.name === "truth_compiler");
  console.log(`BENCHMARK_TRUTH tracked_files=${truth.files} tracked_bytes=${truth.bytes} truth_files=${truth.truthFiles} truth_bytes=${truth.truthBytes}`);
  console.log(`SEMANTIC_DIGEST=${digest}`);
  if (process.argv.includes("--json")) console.log(JSON.stringify({ schemaVersion: 1, platform: process.platform, arch: process.arch, node: process.version, digest, results }, null, 2));
}

if (process.argv[1] && path.resolve(process.argv[1]) === path.resolve(fileURLToPath(import.meta.url))) main();
