import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { resolve } from 'node:path';

const root = fileURLToPath(new URL('..', import.meta.url));
export const DATA_CHECKS = ['hud:check', 'lint', 'assets', 'test', 'multiplayer'];

function runScript(name) {
  const result = process.platform === 'win32'
    ? spawnSync(process.env.ComSpec || 'cmd.exe', ['/d', '/s', '/c', `npm.cmd run ${name}`], { cwd: root, encoding: 'utf8', windowsHide: true })
    : spawnSync('npm', ['run', name], { cwd: root, encoding: 'utf8' });
  if (result.stdout) process.stdout.write(result.stdout);
  if (result.stderr) process.stderr.write(result.stderr);
  if (result.error) process.stderr.write(`${result.error.message}\n`);
  return result.status ?? 1;
}

export function verifyData({ execute = runScript, write = (value) => process.stdout.write(value), now = Date.now } = {}) {
  for (const name of DATA_CHECKS) {
    const startedAt = now();
    const status = execute(name);
    const durationMs = Math.max(0, now() - startedAt);
    if (status !== 0) {
      write(`DATA_VERIFY_STAGE=FAIL name=${name} exit=${status} durationMs=${durationMs}\n`);
      write(`DATA_VERIFY=FAIL check=${name} exit=${status}\n`);
      return status;
    }
    write(`DATA_VERIFY_STAGE=PASS name=${name} durationMs=${durationMs}\n`);
  }
  write(`DATA_VERIFY=PASS checks=${DATA_CHECKS.length}\n`);
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exitCode = verifyData();
