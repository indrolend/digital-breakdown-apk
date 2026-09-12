import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';

const root = resolve(import.meta.dirname, '..');
const tier = resolve(root, 'tools/run-traversal-tier.mjs');
const mutations = [
  { id: 'M01', file: 'native/game/Game.cpp', critical: true, ownership: 'CHECK_OWNED', description: 'all authored slopes lose support', from: 'if(!sample.inside||sample.classification==SupportClassification::Steep)continue;', to: 'if(true)continue;' },
  { id: 'M02', file: 'native/game/Game.cpp', critical: true, ownership: 'CHECK_OWNED', description: 'slope support floats 25cm above geometry', from: 'if(!sample.inside||sample.classification==SupportClassification::Steep)continue;\n        const float candidate=sample.height+GROUND_Y;', to: 'if(!sample.inside||sample.classification==SupportClassification::Steep)continue;\n        const float candidate=sample.height+GROUND_Y+0.25f;' },
  { id: 'M03', file: 'native/game/Game.cpp', critical: true, ownership: 'CHECK_OWNED', description: 'ground jump impulse weakened below playable threshold', from: 'constexpr float JUMP_SPEED = gameplay::TRAVERSAL_CAPABILITIES.groundJumpSpeed;', to: 'constexpr float JUMP_SPEED = 2.0f;' },
  { id: 'M04', file: 'native/game/Game.cpp', critical: true, ownership: 'CHECK_OWNED', description: 'player support uses collision radius and can falsely lift', from: 'WorldSupportSample Game::getPlayerSupport(float x,float z) const {return getWorldSupport(x,z,PLAYER_SUPPORT_RADIUS);}', to: 'WorldSupportSample Game::getPlayerSupport(float x,float z) const {return getWorldSupport(x,z,PLAYER_COLLISION_RADIUS);}' },
  { id: 'M05', file: 'native/game/Game.cpp', critical: true, ownership: 'CHECK_OWNED', description: 'enemy traversal speed reduced to near-stall', from: 'constexpr float HUMAN_WALK_SPEED = 3.8f;', to: 'constexpr float HUMAN_WALK_SPEED = 0.08f;' },
  { id: 'M06', file: 'native/game/Game.cpp', critical: true, ownership: 'CHECK_OWNED', description: 'enemy support footprint collapses to a point', from: 'constexpr float HUMAN_SUPPORT_RADIUS = 0.10f;', to: 'constexpr float HUMAN_SUPPORT_RADIUS = 0.0f;' },
  { id: 'M07', file: 'native/game/FacetedRock.hpp', critical: false, ownership: 'CHECK_OWNED', description: 'walkable generated facets restricted from 35 to 18 degrees', from: 'constexpr float WalkableNormalY=0.819152044f; // cos(35 degrees)', to: 'constexpr float WalkableNormalY=0.951056516f; // mutation: cos(18 degrees)' },
  { id: 'M08', file: 'native/game/Game.cpp', critical: false, ownership: 'PROVE_ONLY_EXPECTED', description: 'production shallow-elevation surfaces are not deployed', from: 'if(early_browser_visuals::usesShallowElevation(plan,surface)&&state_.slopeSupportCount<SLOPE_SUPPORT_COUNT){', to: 'if(false&&early_browser_visuals::usesShallowElevation(plan,surface)&&state_.slopeSupportCount<SLOPE_SUPPORT_COUNT){' },
];

const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const baselines = new Map();
for (const mutation of mutations) {
  const path = resolve(root, mutation.file);
  if (!baselines.has(path)) baselines.set(path, readFileSync(path));
}

function run(mode) {
  const started = performance.now();
  const result = spawnSync(process.execPath, [tier, mode], { cwd: root, encoding: 'utf8', windowsHide: true });
  return { pass: result.status === 0, exitCode: result.status ?? 1, durationMs: Math.round(performance.now() - started), output: `${result.stdout || ''}${result.stderr || ''}` };
}

function detector(output) {
  return output.match(/(?:SLOPE_FIXTURE_FAIL|ENEMY_[A-Z_]+_FAIL|ROCK_[A-Z_]+_FAIL|GENERATED_SURFACE_FAIL|PRODUCTION_ELEVATION_FAIL|TRAVERSAL_STAGE=FAIL)[^\r\n]*/)?.[0] || null;
}

const results = [];
const experimentStarted = performance.now();
try {
  for (const mutation of mutations) {
    const path = resolve(root, mutation.file);
    const baseline = baselines.get(path);
    const source = baseline.toString('utf8');
    const occurrences = source.split(mutation.from).length - 1;
    if (occurrences !== 1) {
      results.push({ ...mutation, classification: 'INVALID_MUTATION', anomaly: `expected one mutation site, found ${occurrences}` });
      continue;
    }
    const setupStarted = performance.now();
    writeFileSync(path, source.replace(mutation.from, mutation.to));
    const setupMs = Math.round(performance.now() - setupStarted);
    let check;
    let prove = null;
    let restoreMs;
    try {
      check = run('check');
      if (check.pass) prove = run('prove');
    } finally {
      const restoreStarted = performance.now();
      writeFileSync(path, baseline);
      restoreMs = Math.round(performance.now() - restoreStarted);
      if (sha256(readFileSync(path)) !== sha256(baseline)) throw new Error(`restoration failed for ${mutation.id}`);
    }
    const classification = !check.pass ? 'FAST_CAUGHT' : prove && !prove.pass ? 'PROVE_ONLY' : 'MISSED';
    results.push({ ...mutation, classification, setupMs, restoreMs, check: { pass: check.pass, durationMs: check.durationMs, detector: detector(check.output) }, prove: prove && { pass: prove.pass, durationMs: prove.durationMs, detector: detector(prove.output) } });
    console.log(`${mutation.id} ${classification} check=${check.durationMs}ms${prove ? ` prove=${prove.durationMs}ms` : ''} detector=${detector(check.output) || detector(prove?.output || '') || 'none'}`);
  }
} finally {
  for (const [path, baseline] of baselines) writeFileSync(path, baseline);
}

const baselineVerification = run('prove');
const packet = {
  generatedAt: new Date().toISOString(),
  totalExperimentMs: Math.round(performance.now() - experimentStarted),
  baselineHashes: Object.fromEntries([...baselines].map(([path, bytes]) => [path.slice(root.length + 1).replaceAll('\\', '/'), sha256(bytes)])),
  restored: [...baselines].every(([path, bytes]) => sha256(readFileSync(path)) === sha256(bytes)),
  baselineVerification: { pass: baselineVerification.pass, durationMs: baselineVerification.durationMs, detector: detector(baselineVerification.output) },
  results,
};
const outputPath = resolve(root, 'build/traversal-mutation-results.json');
mkdirSync(dirname(outputPath), { recursive: true });
writeFileSync(outputPath, `${JSON.stringify(packet, null, 2)}\n`);
console.log(`MUTATION_BASELINE=${baselineVerification.pass ? 'PASS' : 'FAIL'} durationMs=${baselineVerification.durationMs}`);
console.log(`MUTATION_GAUNTLET=${packet.restored && baselineVerification.pass ? 'PASS' : 'FAIL'} variants=${results.length} durationMs=${packet.totalExperimentMs} evidence=${outputPath}`);
if (!packet.restored || !baselineVerification.pass || results.some(result => result.classification === 'INVALID_MUTATION' || (result.critical && result.classification === 'MISSED'))) process.exitCode = 1;
