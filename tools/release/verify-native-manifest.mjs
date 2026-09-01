import fs from "node:fs";
import crypto from "node:crypto";
import path from "node:path";

const manifestPath = process.argv[2];
const artifactRoot = process.argv[3] ?? path.dirname(manifestPath ?? ".");

if (!manifestPath) {
  console.error("usage: node tools/release/verify-native-manifest.mjs <manifest> [artifact-root]");
  process.exit(2);
}

const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
const failures = [];

function requireString(object, key) {
  if (typeof object[key] !== "string" || object[key].length === 0) failures.push(`missing string ${key}`);
}

function requireNumber(object, key) {
  if (!Number.isSafeInteger(object[key])) failures.push(`missing integer ${key}`);
}

if (manifest.schemaVersion !== 3) failures.push(`unsupported schemaVersion ${manifest.schemaVersion}`);
for (const key of ["channel", "humanVersion", "commit", "shortCommit", "publishedAt"]) requireString(manifest, key);
for (const key of ["protocolVersion", "gameplayVersion", "saveFormatVersion"]) requireNumber(manifest, key);
if (!Array.isArray(manifest.artifacts) || manifest.artifacts.length === 0) failures.push("missing artifacts[]");

const seen = new Set();
for (const artifact of manifest.artifacts ?? []) {
  for (const key of ["platform", "architecture", "filename", "assetName", "sha256", "package", "url"]) requireString(artifact, key);
  requireNumber(artifact, "size");
  if (typeof artifact.url === "string" && !artifact.url.startsWith("https://")) failures.push(`${artifact.assetName} url is not HTTPS`);
  if (typeof artifact.sha256 === "string" && !/^[0-9a-f]{64}$/.test(artifact.sha256)) failures.push(`${artifact.assetName} has invalid sha256`);
  const id = `${artifact.platform}/${artifact.architecture}`;
  if (seen.has(id)) failures.push(`duplicate artifact ${id}`);
  seen.add(id);
  const file = path.join(artifactRoot, artifact.filename ?? "");
  if (fs.existsSync(file)) {
    const data = fs.readFileSync(file);
    const actual = crypto.createHash("sha256").update(data).digest("hex");
    if (actual !== artifact.sha256) failures.push(`${artifact.filename} checksum mismatch`);
    if (data.length !== artifact.size) failures.push(`${artifact.filename} size mismatch`);
  }
}

for (const platform of ["windows", "macos", "linux"]) {
  if (![...seen].some((id) => id.startsWith(`${platform}/`))) failures.push(`missing ${platform} artifact`);
}

if (failures.length) {
  console.error(failures.join("\n"));
  process.exit(1);
}

console.log(`Manifest OK: ${manifest.shortCommit} ${manifest.artifacts.length} artifacts`);
