import fs from "node:fs";
import path from "node:path";

const values = new Map();
for (let index = 2; index < process.argv.length; index += 2) {
  values.set(process.argv[index], process.argv[index + 1]);
}

const required = ["--output", "--platform", "--architecture", "--commit", "--portable"];
for (const key of required) {
  if (!values.get(key)) throw new Error(`Missing ${key}`);
}

const commit = values.get("--commit");
if (!/^[0-9a-f]{40}$/i.test(commit) || /^0+$/.test(commit)) {
  throw new Error(`Invalid release commit: ${commit}`);
}

const protocolHeader = fs.readFileSync("native-network/MultiplayerProtocol.hpp", "utf8");
const readVersion = (name) => {
  const value = Number(protocolHeader.match(new RegExp(`${name}\\s*=\\s*(\\d+)`))?.[1]);
  if (!Number.isSafeInteger(value)) throw new Error(`Missing ${name}`);
  return value;
};
const identity = JSON.parse(fs.readFileSync("tools/release/build-identity.json", "utf8"));
const output = values.get("--output");
const buildInfo = {
  schemaVersion: 1,
  project: "DATA",
  displayName: "Digital Breakdown",
  creator: "indrolend",
  commit,
  shortCommit: commit.slice(0, 7),
  platform: values.get("--platform"),
  architecture: values.get("--architecture"),
  configuration: "Release",
  portable: values.get("--portable") === "true",
  protocolVersion: readVersion("PROTOCOL_VERSION"),
  gameplayVersion: readVersion("GAMEPLAY_VERSION"),
  saveFormatVersion: Number(identity.saveFormatVersion),
  builtAt: new Date().toISOString(),
  workflow: process.env.GITHUB_WORKFLOW ?? "local",
  runId: process.env.GITHUB_RUN_ID ?? "local",
};

fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, `${JSON.stringify(buildInfo, null, 2)}\n`);
console.log(`DESKTOP_BUILD_INFO=PASS platform=${buildInfo.platform} commit=${buildInfo.shortCommit}`);
