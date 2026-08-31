import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const windows = process.platform === 'win32';

function run(command, args) {
  const result = spawnSync(command, args, { cwd: root, encoding: 'utf8', windowsHide: true });
  if (result.stdout) process.stdout.write(result.stdout);
  if (result.stderr) process.stderr.write(result.stderr);
  if (result.error) process.stderr.write(`${result.error.message}\n`);
  return result.status ?? 1;
}

const listed = spawnSync('git', ['ls-files', '-z', '--cached', '--others', '--exclude-standard', '--', '*.js', '*.mjs', '*.cjs'], { cwd: root, encoding: 'utf8', windowsHide: true });
if ((listed.status ?? 1) !== 0) {
  if (listed.stderr) process.stderr.write(listed.stderr);
  console.log('LINT=FAIL check=javascript-inventory');
  process.exit(1);
}

const javascript = listed.stdout.split('\0').filter(Boolean);
for (const path of javascript) {
  if (run(process.execPath, ['--check', path]) !== 0) {
    console.log(`LINT=FAIL check=javascript path=${path.replaceAll(' ', '%20')}`);
    process.exit(1);
  }
}

const typescriptStatus = windows
  ? run(process.env.ComSpec || 'cmd.exe', ['/d', '/s', '/c', 'npm.cmd --prefix multiplayer-server exec -- tsc --noEmit --project multiplayer-server/tsconfig.json'])
  : run('npm', ['--prefix', 'multiplayer-server', 'exec', '--', 'tsc', '--noEmit', '--project', 'multiplayer-server/tsconfig.json']);
if (typescriptStatus !== 0) {
  console.log('LINT=FAIL check=typescript');
  process.exit(1);
}

if (run('git', ['diff', '--check']) !== 0) {
  console.log('LINT=FAIL check=whitespace');
  process.exit(1);
}

console.log(`LINT=PASS javascript=${javascript.length} typescript=1 whitespace=1`);
