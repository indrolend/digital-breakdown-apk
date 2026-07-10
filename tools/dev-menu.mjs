import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, writeFileSync } from 'node:fs';
import { homedir, platform } from 'node:os';
import { join, resolve } from 'node:path';
import readline from 'node:readline/promises';
import { stdin as input, stdout as output } from 'node:process';

const ROOT = resolve(import.meta.dirname, '..');
const IS_WINDOWS = platform() === 'win32';
const APP_ID = 'com.indrolend.digitalbreakdown.native';
const ACTIVITY = 'com.indrolend.digitalbreakdown.MainActivity';
const NATIVE_DIR = join(ROOT, 'native-android');
const APK = join(NATIVE_DIR, 'app', 'build', 'outputs', 'apk', 'debug', 'app-debug.apk');
const GRADLEW = join(ROOT, 'android', IS_WINDOWS ? 'gradlew.bat' : 'gradlew');
const rl = readline.createInterface({ input, output });

function clear() {
  process.stdout.write('\x1Bc');
}

function commandExists(command) {
  const probe = IS_WINDOWS ? ['where', command] : ['which', command];
  return spawnSync(probe[0], probe.slice(1), { stdio: 'ignore' }).status === 0;
}

function run(command, args = [], options = {}) {
  console.log(`\n> ${command} ${args.join(' ')}`);
  const result = spawnSync(command, args, {
    cwd: ROOT,
    stdio: 'inherit',
    shell: false,
    ...options,
  });
  return result.status ?? 1;
}

function ensureNativeLocalProperties() {
  const localProperties = join(NATIVE_DIR, 'local.properties');
  if (existsSync(localProperties)) return true;

  const candidates = [
    process.env.ANDROID_HOME,
    process.env.ANDROID_SDK_ROOT,
    IS_WINDOWS
      ? join(process.env.LOCALAPPDATA ?? '', 'Android', 'Sdk')
      : join(homedir(), 'Library', 'Android', 'sdk'),
  ].filter(Boolean);

  const sdk = candidates.find((candidate) => existsSync(candidate));
  if (!sdk) {
    console.error('\nAndroid SDK not found. Set ANDROID_HOME or install Android Studio.');
    return false;
  }

  mkdirSync(NATIVE_DIR, { recursive: true });
  const escaped = IS_WINDOWS ? sdk.replaceAll('\\', '\\\\') : sdk;
  writeFileSync(localProperties, `sdk.dir=${escaped}\n`, 'utf8');
  console.log(`Created ${localProperties}`);
  return true;
}

function adbReady() {
  if (!commandExists('adb')) {
    console.error('\nadb was not found in PATH.');
    return false;
  }
  const result = spawnSync('adb', ['devices'], { cwd: ROOT, encoding: 'utf8' });
  process.stdout.write(result.stdout ?? '');
  const lines = (result.stdout ?? '').split(/\r?\n/).slice(1);
  const ready = lines.some((line) => /\tdevice\s*$/.test(line));
  if (!ready) console.error('\nNo authorized Android device is connected.');
  return ready;
}

function buildNative() {
  if (!ensureNativeLocalProperties()) return 2;
  if (!existsSync(GRADLEW)) {
    console.error(`\nGradle wrapper not found: ${GRADLEW}`);
    return 2;
  }
  return run(GRADLEW, ['-p', NATIVE_DIR, 'assembleDebug']);
}

function installNative() {
  if (!adbReady()) return 2;
  if (!existsSync(APK)) {
    console.error('\nNative APK has not been built yet.');
    return 2;
  }
  run('adb', ['uninstall', APP_ID]);
  return run('adb', ['install', APK]);
}

function launchNative() {
  if (!adbReady()) return 2;
  return run('adb', ['shell', 'am', 'start', '-n', `${APP_ID}/${ACTIVITY}`]);
}

function nativeDemo() {
  let status = buildNative();
  if (status !== 0) return status;
  status = installNative();
  if (status !== 0) return status;
  status = launchNative();
  if (status !== 0) return status;
  if (!commandExists('scrcpy')) {
    console.log('\nscrcpy is not installed. The app was built, installed, and launched.');
    return 0;
  }
  return run('scrcpy', ['--max-size', '1024', '--video-bit-rate', '4M', '--stay-awake', '--no-audio']);
}

function nativeLogs() {
  if (!adbReady()) return 2;
  return run('adb', ['logcat', '-s', 'DBNATIVE:I', 'AndroidRuntime:E', '*:S']);
}

function screenshot() {
  if (!adbReady()) return 2;
  const outDir = join(ROOT, 'logs', 'native');
  mkdirSync(outDir, { recursive: true });
  const stamp = new Date().toISOString().replaceAll(':', '-').replaceAll('.', '-');
  const outFile = join(outDir, `screen-${stamp}.png`);
  const result = spawnSync('adb', ['exec-out', 'screencap', '-p'], { cwd: ROOT, encoding: null });
  if (result.status !== 0 || !result.stdout) return result.status ?? 1;
  writeFileSync(outFile, result.stdout);
  console.log(`Saved: ${outFile}`);
  return 0;
}

function webBuild() {
  const npm = IS_WINDOWS ? 'npm.cmd' : 'npm';
  return run(npm, ['run', 'build:web']);
}

function capacitorSync() {
  const npx = IS_WINDOWS ? 'npx.cmd' : 'npx';
  return run(npx, ['cap', 'sync', 'android']);
}

function gitPull() {
  return run('git', ['pull', '--ff-only']);
}

function status() {
  run('git', ['status', '--short', '--branch']);
  console.log('\n== Tools ==');
  for (const tool of ['node', 'npm', 'git', 'adb', 'scrcpy']) {
    console.log(`${tool.padEnd(8)} ${commandExists(tool) ? 'OK' : 'missing'}`);
  }
  if (commandExists('adb')) run('adb', ['devices', '-l']);
  return 0;
}

const actions = new Map([
  ['1', ['System status', status]],
  ['2', ['Build native APK', buildNative]],
  ['3', ['Install native APK', installNative]],
  ['4', ['Launch native app', launchNative]],
  ['5', ['Native demo: build + install + launch + scrcpy', nativeDemo]],
  ['6', ['Open scrcpy', () => commandExists('scrcpy') ? run('scrcpy', ['--max-size', '1024', '--video-bit-rate', '4M', '--stay-awake', '--no-audio']) : 2]],
  ['7', ['Native logs', nativeLogs]],
  ['8', ['Take screenshot', screenshot]],
  ['9', ['Build WebView bundle', webBuild]],
  ['10', ['Sync Capacitor Android', capacitorSync]],
  ['11', ['Pull latest GitHub changes', gitPull]],
]);

async function pause() {
  await rl.question('\nPress Return to continue...');
}

async function main() {
  while (true) {
    clear();
    console.log('Digital Breakdown\n');
    for (const [key, [label]] of actions) console.log(`${key.padStart(2)}) ${label}`);
    console.log(' 0) Exit');

    const choice = (await rl.question('\nSelect: ')).trim();
    if (choice === '0') break;

    const action = actions.get(choice);
    if (!action) {
      console.log('\nUnknown selection.');
      await pause();
      continue;
    }

    console.log(`\n== ${action[0]} ==`);
    let statusCode = 1;
    try {
      statusCode = await action[1]();
    } catch (error) {
      console.error(error instanceof Error ? error.message : String(error));
    }
    console.log(`\nExit status: ${statusCode}`);
    await pause();
  }
  rl.close();
}

await main();
