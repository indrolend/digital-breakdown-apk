import test from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import { isAbsolute, join, relative, resolve } from 'node:path';

const root = resolve(import.meta.dirname, '..');
const project = JSON.parse(readFileSync(join(root, 'distribution', 'project.json'), 'utf8'));

test('DATA declares a valid owned CommandHUD integration', () => {
  assert.equal(project.id, 'indrolend/data');
  const commands = project.commandHud?.commands;
  assert.ok(Array.isArray(commands));
  assert.deepEqual(commands.map(({ name }) => name), [
    'verify', 'lint', 'assets', 'native-tests', 'multiplayer', 'multiplayer-dry-deploy',
  ]);
  assert.equal(new Set(commands.map(({ name }) => name)).size, commands.length);

  for (const command of commands) {
    assert.match(command.name, /^[a-z0-9][a-z0-9:._-]*$/i);
    assert.ok(command.command);
    assert.ok(Array.isArray(command.argv) && command.argv.length > 0);
    const owner = resolve(root, command.owner);
    const ownerRelative = relative(root, owner);
    assert.equal(isAbsolute(ownerRelative) || ownerRelative === '..' || ownerRelative.startsWith(`..${process.platform === 'win32' ? '\\' : '/'}`), false);
    assert.equal(existsSync(owner), true, command.owner);
  }
});

test('DATA verification composes existing authorities without reimplementing them', () => {
  const command = project.commandHud.commands.find(({ name }) => name === 'verify');
  assert.equal(command.kind, 'test');
  assert.equal(command.resultMarkers, true);
  assert.equal(command.stageMarker, 'DATA_VERIFY_STAGE');
  assert.deepEqual(command.stages.map(({ name }) => name), ['hud:check', 'lint', 'assets', 'test', 'multiplayer']);
  for (const stage of command.stages) {
    assert.ok(Array.isArray(stage.paths) && stage.paths.length > 0, `${stage.name} paths`);
    for (const path of stage.paths) {
      assert.equal(path.includes('*'), false, `${stage.name} uses literal repository scopes`);
      assert.equal(isAbsolute(path), false, `${stage.name} scope stays repository-relative`);
    }
  }
  const source = readFileSync(join(root, 'tools', 'verify.mjs'), 'utf8');
  assert.match(source, /DATA_CHECKS = \['hud:check', 'lint', 'assets', 'test', 'multiplayer'\]/);
  assert.match(source, /npm\.cmd run/);
  assert.match(source, /DATA_VERIFY=FAIL/);
  assert.match(source, /DATA_VERIFY=PASS checks=/);
  assert.doesNotMatch(source, /verify_asset_mirrors|verify-gameplay|vitest|tsc/);
});

test('DATA lint authority owns static JavaScript, TypeScript, and whitespace checks', () => {
  const command = project.commandHud.commands.find(({ name }) => name === 'lint');
  assert.equal(command.kind, 'lint');
  assert.equal(command.resultMarkers, true);
  const source = readFileSync(join(root, 'tools', 'lint.mjs'), 'utf8');
  assert.match(source, /git.*ls-files/);
  assert.match(source, /--cached.*--others.*--exclude-standard/);
  assert.match(source, /--check/);
  assert.match(source, /tsc.*--noEmit/);
  assert.match(source, /git'.*diff.*--check/);
  assert.match(source, /LINT=PASS javascript=/);
});

test('DATA launchers delegate to the installed product with an explicit root', () => {
  const shell = readFileSync(join(root, 'CommandHUD Shell.cmd'), 'utf8');
  const desktop = readFileSync(join(root, 'CommandHUD.cmd'), 'utf8');
  assert.match(shell, /hud shell --root "%~dp0\."/);
  assert.match(desktop, /hud desktop --root "%~dp0\."/);
  assert.doesNotMatch(`${shell}\n${desktop}`, /tools[\\/]hud|node .*cli\.mjs/i);
});

test('native verification owns one cross-platform factual result marker and checkout-scoped build cache', () => {
  const command = project.commandHud.commands.find(({ name }) => name === 'native-tests');
  assert.equal(command.resultMarkers, true);
  const windows = readFileSync(join(root, 'scripts', 'verify-gameplay.ps1'), 'utf8');
  const unix = readFileSync(join(root, 'scripts', 'verify-gameplay.sh'), 'utf8');
  const wrapper = readFileSync(join(root, 'tools', 'run-native-tests.mjs'), 'utf8');
  assert.match(windows, /NATIVE_VERIFICATION=PASS suite=gameplay/);
  assert.match(unix, /NATIVE_VERIFICATION=PASS suite=gameplay/);
  assert.match(windows, /NATIVE_STAGE=PASS name=build durationSeconds=/);
  assert.match(unix, /NATIVE_STAGE=PASS name=\$name durationSeconds=/);
  assert.match(windows, /CMakeCache\.txt/);
  assert.match(unix, /CMakeCache\.txt/);
  assert.match(wrapper, /checkoutKey/);
  assert.doesNotMatch(wrapper, /"digital-breakdown-gameplay-checks"\)/);
  const targetPattern = /\b(?:DigitalBreakdown|[A-Za-z0-9]+(?:Test|Probe|Soak))\b/g;
  const windowsTargets = [...new Set(windows.match(targetPattern) || [])].filter((name) => name !== 'CTest').sort();
  const unixTargets = [...new Set(unix.match(targetPattern) || [])].filter((name) => name !== 'CTest').sort();
  assert.deepEqual(unixTargets, windowsTargets);
});

test('multiplayer verification emits one factual marker only after its complete check chain', () => {
  const command = project.commandHud.commands.find(({ name }) => name === 'multiplayer');
  assert.equal(command.resultMarkers, true);
  assert.equal(command.kind, 'test');
  const multiplayerPackage = JSON.parse(readFileSync(join(root, 'multiplayer-server', 'package.json'), 'utf8'));
  assert.match(
    multiplayerPackage.scripts.check,
    /wrangler types --check && tsc --noEmit && vitest run && node -e .*MULTIPLAYER_CHECK=PASS suite=server/,
  );
});
