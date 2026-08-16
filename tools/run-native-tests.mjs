import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { tmpdir } from "node:os";

const windows = process.platform === "win32";
const command = windows ? "powershell.exe" : "bash";
const windowsBuildRoot = join(process.env.LOCALAPPDATA || tmpdir(), "CodexBuild", "digital-breakdown-gameplay-checks");
const args = windows
  ? ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "scripts/verify-gameplay.ps1", "-BuildDir", windowsBuildRoot]
  : ["scripts/verify-gameplay.sh"];

const result = spawnSync(command, args, { stdio: "inherit" });
if (result.error) {
  console.error(`Unable to start native verification: ${result.error.message}`);
  process.exit(1);
}
process.exit(result.status ?? 1);
