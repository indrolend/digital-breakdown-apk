import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";

const windows = process.platform === "win32";
const command = windows ? "powershell.exe" : "bash";
const checkoutRoot = resolve(import.meta.dirname, "..");
const checkoutKey = createHash("sha256").update(checkoutRoot.toLowerCase()).digest("hex").slice(0, 12);
const windowsBuildRoot = join(process.env.LOCALAPPDATA || tmpdir(), "CodexBuild", `digital-breakdown-gameplay-checks-${checkoutKey}`);
const args = windows
  ? ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "scripts/verify-gameplay.ps1", "-BuildDir", windowsBuildRoot]
  : ["scripts/verify-gameplay.sh"];

const result = spawnSync(command, args, { stdio: "inherit" });
if (result.error) {
  console.error(`Unable to start native verification: ${result.error.message}`);
  process.exit(1);
}
process.exit(result.status ?? 1);
