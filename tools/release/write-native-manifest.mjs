import fs from "node:fs";
import crypto from "node:crypto";
import path from "node:path";

const releaseDir = process.argv[2] ?? "release";
const repository = process.env.GITHUB_REPOSITORY ?? "indrolend/digital-breakdown-apk";
const commit = process.env.GITHUB_SHA ?? "local";
const shortCommit = commit.slice(0, 7);
const runId = process.env.GITHUB_RUN_ID ?? "local";
const workflow = process.env.GITHUB_WORKFLOW ?? "local";
const humanVersion = process.env.DB_HUMAN_VERSION ?? `experimental-${new Date().toISOString().slice(0, 10).replaceAll("-", ".")}`;
const publishedAt = new Date().toISOString();
const releaseBase = `https://github.com/${repository}/releases/download/latest-native`;

const protocolHeader = fs.readFileSync("native-network/MultiplayerProtocol.hpp", "utf8");
const protocolVersion = Number(protocolHeader.match(/PROTOCOL_VERSION\s*=\s*(\d+)/)?.[1]);
const gameplayVersion = Number(protocolHeader.match(/GAMEPLAY_VERSION\s*=\s*(\d+)/)?.[1]);
const identity = JSON.parse(fs.readFileSync("tools/release/build-identity.json", "utf8"));
const saveFormatVersion = Number(identity.saveFormatVersion);

function artifact(filename, platform, architecture, packageType) {
  const file = path.join(releaseDir, filename);
  const data = fs.readFileSync(file);
  return {
    platform,
    architecture,
    filename,
    assetName: filename,
    url: `${releaseBase}/${filename}`,
    sha256: crypto.createHash("sha256").update(data).digest("hex"),
    size: data.length,
    package: packageType,
  };
}

const artifacts = [
  artifact("DigitalBreakdown-Windows.zip", "windows", "x64", "zip"),
  artifact("DigitalBreakdown-macOS-Universal.zip", "macos", "universal", "zip"),
  artifact("DigitalBreakdown-Android.apk", "android", "armeabi-v7a", "apk"),
];

const windows = artifacts.find((item) => item.platform === "windows");
const macos = artifacts.find((item) => item.platform === "macos");
const android = artifacts.find((item) => item.platform === "android");

const manifest = {
  schemaVersion: 3,
  channel: "experimental",
  humanVersion,
  commit,
  shortCommit,
  protocolVersion,
  gameplayVersion,
  saveFormatVersion,
  publishedAt,
  workflow,
  runId,
  artifacts,
  windows: {
    available: true,
    architecture: windows.architecture,
    configuration: "Release",
    portable: true,
    url: windows.url,
    sha256: windows.sha256,
  },
  macos: {
    available: true,
    architecture: macos.architecture,
    configuration: "Release",
    package: macos.package,
    url: macos.url,
    sha256: macos.sha256,
  },
  android: {
    available: true,
    architecture: android.architecture,
    configuration: fs.existsSync(path.join(releaseDir, "android-variant.txt"))
      ? fs.readFileSync(path.join(releaseDir, "android-variant.txt"), "utf8").trim()
      : "unknown",
    applicationId: "com.indrolend.digitalbreakdown.native",
    url: android.url,
    sha256: android.sha256,
  },
};

fs.writeFileSync(path.join(releaseDir, "build-manifest.json"), `${JSON.stringify(manifest, null, 2)}\n`);
fs.writeFileSync(path.join(releaseDir, "checksums.txt"), artifacts.map((item) => `${item.sha256}  ${item.filename}`).join("\n") + "\n");
console.log(`Wrote ${path.join(releaseDir, "build-manifest.json")}`);
