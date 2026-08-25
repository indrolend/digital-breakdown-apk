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
    'assets', 'native-tests', 'multiplayer', 'multiplayer-dry-deploy',
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

test('DATA launchers delegate to the installed product with an explicit root', () => {
  const shell = readFileSync(join(root, 'CommandHUD Shell.cmd'), 'utf8');
  const desktop = readFileSync(join(root, 'CommandHUD.cmd'), 'utf8');
  assert.match(shell, /hud shell --root "%~dp0\."/);
  assert.match(desktop, /hud desktop --root "%~dp0\."/);
  assert.doesNotMatch(`${shell}\n${desktop}`, /tools[\\/]hud|node .*cli\.mjs/i);
});
