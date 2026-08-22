import { spawnSync } from 'node:child_process';
import { platform } from 'node:os';
import { join, resolve } from 'node:path';
import readline from 'node:readline/promises';
import { stdin as input, stdout as output } from 'node:process';

const ROOT = resolve(import.meta.dirname, '..');
const WINDOWS = platform() === 'win32';
const POWERSHELL = WINDOWS ? 'powershell.exe' : 'pwsh';
const DBDEV = join(ROOT, 'tools', 'dbdev.ps1');
const ADB_DOCTOR = join(ROOT, 'scripts', 'adb-doctor.ps1');
const rl = readline.createInterface({ input, output });

function run(command, args = []) {
  console.log(`\n> ${command} ${args.join(' ')}`);
  const result = spawnSync(command, args, { cwd: ROOT, stdio: 'inherit', shell: false });
  return result.status ?? 1;
}

function ps(script, args = []) {
  return run(POWERSHELL, ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', script, ...args]);
}

const sections = [
  ['System', [
    ['Environment and Git status', () => ps(DBDEV, ['diagnostics'])],
    ['Repair development environment', () => ps(DBDEV, ['repair'])],
  ]],
  ['Desktop', [
    ['Build Release', () => ps(DBDEV, ['desktop-build'])],
    ['Build and launch Release', () => ps(DBDEV, ['desktop-run'])],
    ['Clean configure, build, and launch Release', () => ps(DBDEV, ['desktop-run', '-Reconfigure'])],
    ['Run desktop tests', () => ps(DBDEV, ['desktop-test'])],
    ['Smoke test Release', () => ps(DBDEV, ['desktop-smoke'])],
    ['Rally playtest', () => ps(DBDEV, ['playtest', '-Mode', 'rally'])],
    ['Traversal playtest', () => ps(DBDEV, ['playtest', '-Mode', 'traversal'])],
    ['Room inspector', () => ps(DBDEV, ['playtest', '-Mode', 'rooms'])],
  ]],
  ['Android', [
    ['Build native APK', () => ps(DBDEV, ['android-build'])],
    ['Build, install, launch, and stream', () => ps(DBDEV, ['android-stream'])],
    ['ADB diagnostics', () => ps(ADB_DOCTOR)],
  ]],
  ['Web', [
    ['Build Web/WebView bundle', () => run(WINDOWS ? 'npm.cmd' : 'npm', ['run', 'build:web'])],
    ['Synchronize Capacitor Android', () => run(WINDOWS ? 'npm.cmd' : 'npm', ['run', 'cap:sync'])],
  ]],
  ['Repository', [
    ['Inspect branch state', () => run('git', ['status', '--short', '--branch'])],
    ['Fetch remote references', () => run('git', ['fetch', '--prune'])],
  ]],
  ['Release', [
    ['Verify latest Windows release', () => ps(DBDEV, ['release-windows'])],
    ['Verify latest Android release', () => ps(DBDEV, ['release-android'])],
  ]],
];

const actions = new Map();
let nextKey = 1;
for (const [, entries] of sections) {
  for (const [label, action] of entries) actions.set(String(nextKey++), [label, action]);
}

async function main() {
  while (true) {
    process.stdout.write('\x1Bc');
    console.log('Digital Breakdown — cross-platform developer menu');
    let key = 1;
    for (const [section, entries] of sections) {
      console.log(`\n${section}`);
      for (const [label] of entries) console.log(`  ${String(key++).padStart(2)}) ${label}`);
    }
    console.log('\n   0) Exit');
    const choice = (await rl.question('\nSelect: ')).trim();
    if (choice === '0') break;
    const selected = actions.get(choice);
    if (!selected) {
      console.log('\nUnknown selection.');
    } else {
      console.log(`\n== ${selected[0]} ==`);
      try {
        console.log(`\nExit status: ${await selected[1]()}`);
      } catch (error) {
        console.error(error instanceof Error ? error.message : String(error));
      }
    }
    await rl.question('\nPress Return to continue...');
  }
  rl.close();
}

await main();
