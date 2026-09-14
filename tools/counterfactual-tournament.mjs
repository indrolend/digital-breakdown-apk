#!/usr/bin/env node

// Disposable counterfactual tournament.
//
// Given a directory of .patch files, materialize each candidate in a detached
// temporary git worktree, run the same probes against baseline and candidate,
// compare any SEMANTIC_DIGEST=<value> markers, measure *authoritative* source
// reduction, rank survivors, and delete every worktree. Candidate futures are
// evidence, not repository authorities.

import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import crypto from "node:crypto";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));
const normalize = p => p.split(path.sep).join("/");
const AUTHORITY_FILES = new Set(["distribution/project.json", "native-desktop/CMakeLists.txt", "tools/verify.mjs", ".github/workflows/ci.yml", ".github/workflows/native-release.yml"]);

function category(file) {
  if (/^native\/tests\//.test(file) || /\.test\.mjs$/.test(file)) return "proof";
  if (/^(docs\/|README\.md$|AGENTS\.md$)/.test(file)) return "history";
  if (/^(native\/game|native-desktop|native-network)\//.test(file)) return "runtime";
  if (AUTHORITY_FILES.has(file) || /^\.github\/workflows\//.test(file)) return "build";
  return "support";
}

function die(message) {
  console.error(`COUNTERFACTUAL_TOURNAMENT=FAIL ${message}`);
  process.exit(2);
}

function run(command, args, cwd, { allowFailure = false } = {}) {
  const r = spawnSync(command, args, { cwd, encoding: "utf8", windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  const status = r.status ?? 1;
  if (!allowFailure && status !== 0) throw new Error(`${command} ${args.join(" ")} failed (${status})`);
  return { status, stdout: r.stdout ?? "", stderr: r.stderr ?? "" };
}

function git(args, cwd = root, options) { return run("git", args, cwd, options); }

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
  return process.platform === "win32"
    ? run(process.env.ComSpec || "cmd.exe", ["/d", "/s", "/c", command], cwd, { allowFailure: true })
    : run(process.env.SHELL || "/bin/sh", ["-lc", command], cwd, { allowFailure: true });
}

function digestMarkers(text) {
  return text.split(/\r?\n/).map(line => line.match(/(?:^|\s)SEMANTIC_DIGEST=([^\s]+)/)?.[1]).filter(Boolean);
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
  const stats = { files: 0, additions: 0, deletions: 0, netLines: 0, runtime: { additions: 0, deletions: 0 }, touchedProof: false, touchedHistory: false, touchedAuthority: false, paths: [] };
  for (const line of r.stdout.split(/\r?\n/).filter(Boolean)) {
    const [aRaw, dRaw, pRaw] = line.split("\t");
    const file = normalize(pRaw || "");
    const a = /^\d+$/.test(aRaw) ? Number(aRaw) : 0;
    const d = /^\d+$/.test(dRaw) ? Number(dRaw) : 0;
    const kind = category(file);
    stats.files++;
    stats.additions += a;
    stats.deletions += d;
    stats.paths.push({ file, kind, additions: a, deletions: d });
    if (kind === "runtime" || kind === "build") { stats.runtime.additions += a; stats.runtime.deletions += d; }
    if (kind === "proof") stats.touchedProof = true;
    if (kind === "history") stats.touchedHistory = true;
    if (AUTHORITY_FILES.has(file)) stats.touchedAuthority = true;
  }
  stats.netLines = stats.additions - stats.deletions;
  return stats;
}

function rank(candidate) {
  if (!candidate.probes.passed) return -1e12;
  if (!candidate.semanticEquivalent) return -1e9;
  // Never reward deletion of proof/history. Penalize touching canonical authority.
  // Only runtime/build subtraction can produce positive selection pressure.
  let score = candidate.diff.runtime.deletions * 10 - candidate.diff.runtime.additions * 2 - candidate.diff.files * 5;
  if (candidate.diff.touchedProof) score -= 10000;
  if (candidate.diff.touchedHistory) score -= 1000;
  if (candidate.diff.touchedAuthority) score -= 5000;
  return score;
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
          candidate = { name, patch, applyPassed: false, applyError: apply.stderr.trim(), probes: { passed: false, results: [] }, semanticEquivalent: false, diff: { files: 0, additions: 0, deletions: 0, runtime: { additions: 0, deletions: 0 } }, score: -1e12 };
        } else {
          const probes = runProbes(cwd, args.probes);
          const diff = diffStats(cwd);
          candidate = { name, patch, applyPassed: true, probes, semanticEquivalent: semanticEquivalent(baseline, probes), diff };
          candidate.score = rank(candidate);
        }
      } catch (error) {
        candidate = { name, patch, applyPassed: false, error: String(error), probes: { passed: false, results: [] }, semanticEquivalent: false, diff: { files: 0, additions: 0, deletions: 0, runtime: { additions: 0, deletions: 0 } }, score: -1e12 };
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
    selection: "all probes pass; emitted semantic digests equal baseline; reward only runtime/build subtraction; penalize proof/history/authority edits",
    candidates
  };

  if (args.json) console.log(JSON.stringify(result, null, 2));
  else {
    console.log(`COUNTERFACTUAL_TOURNAMENT=PASS candidates=${candidates.length}`);
    for (const c of candidates) console.log(`${c.semanticEquivalent ? "SURVIVE" : "REJECT "} score=${String(c.score).padStart(8)} runtime=+${c.diff.runtime.additions}/-${c.diff.runtime.deletions} files=${c.diff.files} ${c.name}`);
  }
}

main();
