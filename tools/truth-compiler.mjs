#!/usr/bin/env node

// Bounded repository truth probe. This is development instrumentation, not
// gameplay authority. It derives current truth from the supported native
// product/build/test surfaces and emits a small machine-readable digest.

import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { execFileSync } from "node:child_process";

const root = path.resolve(import.meta.dirname, "..");
const normalize = (p) => p.split(path.sep).join("/");

function git(args) {
  return execFileSync("git", ["-C", root, ...args], { encoding: "utf8" }).trim();
}

function trackedFiles() {
  return git(["ls-files", "-z"]).split("\0").filter(Boolean).map(normalize).sort();
}

function classify(file) {
  if (/^(native\/game|native-desktop|native-network|native-models|native-tv-gifs)\//.test(file)) return "runtime";
  if (file === "native-desktop/CMakeLists.txt" || /^(package(-lock)?\.json|tools\/verify\.mjs|\.github\/workflows\/)/.test(file)) return "build";
  if (/^native\/tests\//.test(file) || /(^|\/)(test|tests)\//.test(file) || /\.test\.mjs$/.test(file)) return "proof";
  if (/^(docs\/|README\.md$|AGENTS\.md$)/.test(file)) return "history";
  if (/\.(png|jpe?g|gif|wav|mp3|ogg|glb|gltf|ttf|otf)$/i.test(file)) return "asset";
  return "support";
}

function bytes(file) {
  try { return fs.statSync(path.join(root, file)).size; } catch { return 0; }
}

function sha256(text) {
  return crypto.createHash("sha256").update(text).digest("hex").slice(0, 16);
}

function main() {
  const files = trackedFiles();
  const groups = {};
  for (const file of files) {
    const kind = classify(file);
    const g = groups[kind] ??= { files: 0, bytes: 0 };
    g.files++;
    g.bytes += bytes(file);
  }

  const project = JSON.parse(fs.readFileSync(path.join(root, "distribution/project.json"), "utf8"));
  const commands = project.commandHud?.commands ?? [];
  const authorities = {
    humanEntrance: ["CommandHUD.cmd", "hud desktop --root ."],
    productExecutable: "DigitalBreakdown",
    gameplay: "native/game",
    presentation: "native-desktop",
    protocol: "native-network",
    build: "native-desktop/CMakeLists.txt",
    testRunner: "CTest",
    verification: "tools/verify.mjs",
    semanticActions: commands.map(({name, owner}) => ({name, owner}))
  };

  const truthFiles = files.filter(f => ["runtime", "build", "asset"].includes(classify(f)));
  const result = {
    schemaVersion: 1,
    head: git(["rev-parse", "HEAD"]),
    branch: git(["branch", "--show-current"]),
    dirty: Boolean(git(["status", "--porcelain"])),
    tracked: { files: files.length, bytes: files.reduce((n, f) => n + bytes(f), 0), groups },
    authoritativeTruthSurface: {
      files: truthFiles.length,
      bytes: truthFiles.reduce((n, f) => n + bytes(f), 0),
      digest: sha256(truthFiles.join("\n"))
    },
    authorities,
    invariants: {
      singleProductExecutable: true,
      singleBuildAuthority: true,
      singleNativeTestRunner: true,
      historicalProseIsRuntimeAuthority: false
    }
  };

  if (process.argv.includes("--json")) console.log(JSON.stringify(result, null, 2));
  else {
    console.log("DIGITAL BREAKDOWN TRUTH");
    console.log(`head ${result.head.slice(0, 12)} ${result.dirty ? "DIRTY" : "clean"}`);
    console.log(`tracked ${result.tracked.files} files / ${result.tracked.bytes} bytes`);
    console.log(`truth-surface ${result.authoritativeTruthSurface.files} files / ${result.authoritativeTruthSurface.bytes} bytes`);
    for (const [kind, g] of Object.entries(groups).sort()) console.log(`${kind.padEnd(8)} ${String(g.files).padStart(4)} files ${String(g.bytes).padStart(10)} bytes`);
    console.log(`semantic-actions ${commands.map(c => c.name).join(",")}`);
  }
}

main();
