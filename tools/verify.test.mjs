import test from 'node:test';
import assert from 'node:assert/strict';
import { DATA_CHECKS, verifyData } from './verify.mjs';

test('DATA verification runs every authority in deterministic order before PASS', () => {
  const executed = [];
  let output = '';
  const status = verifyData({ execute: (name) => { executed.push(name); return 0; }, write: (value) => { output += value; } });
  assert.equal(status, 0);
  assert.deepEqual(executed, DATA_CHECKS);
  assert.match(output, /DATA_VERIFY=PASS checks=5/);
  assert.doesNotMatch(output, /DATA_VERIFY=FAIL/);
});

test('DATA verification stops at the first failure and never claims PASS', () => {
  const executed = [];
  let output = '';
  const status = verifyData({ execute: (name) => { executed.push(name); return name === 'lint' ? 7 : 0; }, write: (value) => { output += value; } });
  assert.equal(status, 7);
  assert.deepEqual(executed, ['hud:check', 'lint']);
  assert.match(output, /DATA_VERIFY=FAIL check=lint exit=7/);
  assert.doesNotMatch(output, /DATA_VERIFY=PASS/);
});
