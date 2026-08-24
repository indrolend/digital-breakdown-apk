import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { execFileSync } from 'node:child_process';
import { repositoryCurrency } from './core.mjs';

function fixture() {
  const root = mkdtempSync(join(tmpdir(), 'hud-validity-'));
  mkdirSync(join(root, 'distribution'));
  writeFileSync(join(root, 'distribution', 'project.json'), JSON.stringify({ id: 'indrolend/data' }));
  writeFileSync(join(root, '.gitignore'), 'ignored/\n');
  writeFileSync(join(root, 'file.txt'), 'initial\n');
  execFileSync('git', ['init', '-b', 'main'], { cwd: root });
  execFileSync('git', ['config', 'user.email', 'hud@example.invalid'], { cwd: root });
  execFileSync('git', ['config', 'user.name', 'HUD Validity Test'], { cwd: root });
  execFileSync('git', ['add', '.'], { cwd: root });
  execFileSync('git', ['commit', '-m', 'fixture'], { cwd: root });
  return root;
}

test('repository currency measures repository content, not ambient process environment', async () => {
  const root = fixture();
  const before = await repositoryCurrency(root);
  const previous = process.env.HUD_VALIDITY_PROBE;

  try {
    process.env.HUD_VALIDITY_PROBE = `changed-${Date.now()}`;
    const afterEnvironmentChange = await repositoryCurrency(root);
    assert.deepEqual(afterEnvironmentChange, before);
  } finally {
    if (previous === undefined) delete process.env.HUD_VALIDITY_PROBE;
    else process.env.HUD_VALIDITY_PROBE = previous;
  }
});

test('repository currency changes when relevant repository bytes change', async () => {
  const root = fixture();
  const before = await repositoryCurrency(root);
  writeFileSync(join(root, 'file.txt'), 'changed\n');
  const after = await repositoryCurrency(root);
  assert.equal(after.head, before.head);
  assert.notEqual(after.worktreeFingerprint, before.worktreeFingerprint);
});

test('repository currency ignores files Git itself marks ignored', async () => {
  const root = fixture();
  const before = await repositoryCurrency(root);
  mkdirSync(join(root, 'ignored'));
  writeFileSync(join(root, 'ignored', 'cache.bin'), 'irrelevant output');
  assert.deepEqual(await repositoryCurrency(root), before);
});
