#!/usr/bin/env node

// Disposable counterfactual tournament.
//
// Given a directory of .patch files, materialize each candidate in a detached
// temporary git worktree, run the same probes against baseline and candidate,
// compare any SEMANTIC_DIGEST=<hex/text> markers, measure source delta, rank
// survivors, and delete every worktree. Candidate futures are evidence, not
// repository authorities.

import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import crypto from "node:crypto";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));

function die(message) {
  console.error(`COUNTERFACTUAL_TOURNAMENT=FAIL ${message}`);
  process.exit(2);
}

function run(command, args, cwd, { allowFailure = false } = {}) {
  const r = spawnSync(command, args, { cwd, encoding: "utf8", windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  const status = r.status ?? 1;
  if (!allowFailure && status !== 0) {
    const error = new Error(`${command} ${args.join(" ")} failed (${status})`);
    error.result = r;
    throw error;
  }
  return { status, stdout: r.stdout ?? "", stderr: r.stderr ?? "" };
}

function git(args, cwd = root, options) {
  return run("git", args, cwd, options);
}

function parseArgs(argv) {
  const out = { probes: [], patchDir: null, json: false, fullVerify: false, keep: false };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--patch-dir") out.patchDir = argv[++i];
    else if (a === "--probe") out.probes.push(argv[++i]);
    else if (a === "--full-verify") out.fullVerify = true;
    else if (a === "--json") out.json = true;
    else if (a === "--keep-worktrees") out.keep = true;
    else if (a === "--help") {
      console.log("node tools/counterfactual-tournament.mjs --patch-dir DIR [--probe COMMAND] [--full-verify] [--json]");
      process.exit(0);
    } else die(`unknown_arg=${a}`);
  }
  if (!out.patchDir) die("missing=--patch-dir");
  if (!out.probes.length) out.probes = ["node tools/truth-compiler.test.mjs", "npm run hud:check"];
  if (out.fullVerify) out.probes.push("node tools/verify.mjs");
  return out;
}

function shell(command, cwd) {
  const r = process.platform === "win32"
    ? run(process.env.ComSpec || "cmd.exe", ["/d", "/s", "/c", command], cwd, { allowFailure: true })
    : run(process.env.SHELL || "/bin/sh", ["-lc", command], cwd, { allowFailure: true });
  return { command, ...r };
}

function digestMarkers(text) {
  const values = [];
  for (const line of text.split(/\r?\n/)) {
    const m = line.match(/(?:^|\s)SEMANTIC_DIGEST=([^\s]+)/);
    if (m) values.push(m[1]);
  }
  return values;
}

function runProbes(cwd, probes) {
  const results = probes.map(command => {
    const r = shell(command, cwd);
    return {
      command,
      status: r.status,
      semanticDigests: digestMarkers(`${r.stdout}\n${r.stderr}`),
      outputDigest: crypto.createHash("sha256").update(`${r.stdout}\n${r.stderr}`).digest("hex").slice(0, 16)
    };
  });
  return { passed: results.every(r => r.status === 0), results };
}

function semanticEquivalent(baseline, candidate) {
  if (!candidate.passed) return false;
  for (let i = 0; i < baseline.results.length; i++) {
    const a = baseline.results[i].semanticDigests;
    const b = candidate.results[i].semanticDigests;
    if (a.length || b.length) {
      if (a.length !== b.length) return false;
      for (let j = 0; j < a.length; j++) if (a[j] !== b[j]) return false;
    }
  }
  return true;
}

function diffStats(cwd) {
  const r = git(["diff", "--numstat", "HEAD"], cwd);
  let additions = 0, deletions = 0, files = 0;
  for (const line of r.stdout.split(/\r?\n/).filter(Boolean)) {
    const [a, d] = line.split("\t");
    if (/^\d+$/.test(a)) additions += Number(a);
    if (/^\d+$/.test(d)) deletions += Number(d);
    files++;
  }
  return { files, additions, deletions, netLines: additions - deletions };
}

function rank(candidate) {
  if (!candidate.probes.passed) return -1e12;
  if (!candidate.semanticEquivalent) return -1e9;
  // Selection pressure: preserve tested semantics, then prefer subtraction and
  // smaller change radius. This is intentionally simple and inspectable.
  return candidate.diff.deletions * 10 - candidate.diff.additions * 2 - candidate.diff.files * 5;
}

function safeName(file) {
  return path.basename(file, path.extname(file)).replace(/[^A-Za-z0-9._-]+/g, "-").slice(0, 64) || "candidate";
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const patchDir = path.resolve(root, args.patchDir);
  if (!fs.existsSync(patchDir)) die(`patch_dir_not_found=${patchDir}`);
  const patches = fs.readdirSync(patchDir).filter(x => x.endsWith(".patch")).sort();
  if (!patches.length) die(`no_patches=${patchDir}`);

  const baselineHead = git(["rev-parse", "HEAD"]).stdout.trim();
  if (git(["status", "--porcelain"]).stdout.trim()) die("working_tree_must_be_clean");
  const baseline = runProbes(root, args.probes);
  if (!baseline.passed) die("baseline_probes_failed");

  const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), "db-counterfactual-"));
  const candidates = [];

  try {
    for (let index = 0; index < patches.length; index++) {
      const patch = patches[index];
      const name = safeName(patch);
      const cwd = path.join(tempRoot, `${String(index).padStart(3, "0")}-${name}`);
      let candidate;
      try {
        git(["worktree", "add", "--quiet", "--detach", cwd, baselineHead]);
        const apply = git(["apply", "--whitespace=nowarn", path.join(patchDir, patch)], cwd, { allowFailure: true });
        if (apply.status !== 0) {
          candidate = { name, patch, applyPassed: false, applyError: apply.stderr.trim(), probes: { passed: false, results: [] }, semanticEquivalent: false, diff: { files: 0, additions: 0, deletions: 0, netLines: 0 }, score: -1e12 };
        } else {
          const probes = runProbes(cwd, args.probes);
          const diff = diffStats(cwd);
          candidate = { name, patch, applyPassed: true, probes, semanticEquivalent: semanticEquivalent(baseline, probes), diff };
          candidate.score = rank(candidate);
        }
      } catch (error) {
        candidate = { name, patch, applyPassed: false, error: String(error), probes: { passed: false, results: [] }, semanticEquivalent: false, diff: { files: 0, additions: 0, deletions: 0, netLines: 0 }, score: -1e12 };
      } finally {
        if (!args.keep && fs.existsSync(cwd)) git(["worktree", "remove", "--force", cwd], root, { allowFailure: true });
      }
      candidates.push(candidate);
    }
  } finally {
    git(["worktree", "prune"], root, { allowFailure: true });
    if (!args.keep) fs.rmSync(tempRoot, { recursive: true, force: true });
  }

  candidates.sort((a, b) => b.score - a.score || a.name.localeCompare(b.name));
  const result = {
    schemaVersion: 1,
    baseline: { head: baselineHead, probes: baseline.results.map(x => ({ command: x.command, semanticDigests: x.semanticDigests })) },
    selection: "probes pass; emitted semantic digests match baseline; prefer deletions and smaller change radius",
    candidates
  };

  if (args.json) console.log(JSON.stringify(result, null, 2));
  else {
    console.log(`COUNTERFACTUAL_TOURNAMENT=PASS candidates=${candidates.length}`);
    for (const c of candidates) {
      console.log(`${c.semanticEquivalent ? "SURVIVE" : "REJECT "} score=${String(c.score).padStart(8)} files=${c.diff.files} +${c.diff.additions}/-${c.diff.deletions} ${c.name}`);
    }
  }
}

main();
