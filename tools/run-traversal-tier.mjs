import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { resolve } from 'node:path';

const root = resolve(import.meta.dirname, '..');
const mode = process.argv[2];
if (!['check', 'prove'].includes(mode)) {
  console.error('TRAVERSAL=FAIL reason=mode');
  process.exit(2);
}

const buildDir = resolve(root, process.env.DB_NATIVE_BUILD_DIR || 'build/gameplay-checks');
const targets = mode === 'check'
  ? ['TraversalCalibrationTest', 'SlopeTraversalFixtureTest']
  : ['TraversalCalibrationTest', 'SlopeTraversalFixtureTest'];
const testPattern = mode === 'check'
  ? '^(TraversalCalibrationTest|SlopeTraversalCheck)$'
  : '^(TraversalCalibrationTest|SlopeTraversalFixtureTest)$';

function run(stage, command, args) {
  const started = performance.now();
  const result = spawnSync(command, args, { cwd: root, encoding: 'utf8', windowsHide: true });
  if (result.stdout) process.stdout.write(result.stdout);
  if (result.stderr) process.stderr.write(result.stderr);
  const durationMs = Math.round(performance.now() - started);
  const status = result.status ?? 1;
  console.log(`TRAVERSAL_STAGE=${status === 0 ? 'PASS' : 'FAIL'} name=${stage} durationMs=${durationMs}`);
  if (result.error) console.error(result.error.message);
  if (status !== 0) process.exit(status);
  return durationMs;
}

const totalStarted = performance.now();
if (!existsSync(resolve(buildDir, 'CMakeCache.txt'))) {
  run('configure', 'cmake', ['-S', 'native-desktop', '-B', buildDir]);
}
run('build', 'cmake', ['--build', buildDir, '--config', 'Release', '--target', ...targets]);
run('test', 'ctest', ['--test-dir', buildDir, '-C', 'Release', '-R', testPattern, mode === 'check' ? '--verbose' : '--output-on-failure']);
console.log(`TRAVERSAL=PASS mode=${mode} tests=2 durationMs=${Math.round(performance.now() - totalStarted)}`);
