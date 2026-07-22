import { readFile } from "node:fs/promises";
import { resolve } from "node:path";

const tracePath = resolve(process.argv[2] || "build/pass7-oracle/traces/pass7-vacuum-crosshair-suite.json");
const suite = JSON.parse(await readFile(tracePath, "utf8"));
const scenarios = suite.scenarios;
let passed = 0;

function expect(condition, message) {
  if (!condition) throw new Error(`FAIL: ${message}`);
  passed++;
  console.log(`PASS: ${message}`);
}

const soul = (snapshot) => snapshot.souls[0];
const freeAway = scenarios.freeAimAway;
expect(freeAway.aimedAway.vacuum.active, "aiming away does not deactivate the vacuum field");
expect(freeAway.aimedAway.vacuum.target === -1 && soul(freeAway.aimedAway).state === "FREE",
  "a never-acquired soul remains free while outside the offer cone");
expect(freeAway.returned.vacuum.target === 0 && soul(freeAway.returned).state === "ATTRACTED",
  "returning the crosshair reacquires a free soul without restarting vacuum");

const attractedAway = scenarios.attractedAimAway;
expect(soul(attractedAway.attracted).state === "ATTRACTED" && attractedAway.attracted.vacuum.target === 0,
  "the controlled pre-latch fixture reaches ATTRACTED");
expect(soul(attractedAway.aimedAway).state === "FREE" && attractedAway.aimedAway.vacuum.target === -1,
  "aim loss before latch releases ATTRACTED back to FREE");
expect(soul(attractedAway.returned).state === "ATTRACTED" && attractedAway.returned.vacuum.target === 0,
  "the released soul is reacquired when aim returns");

const latchedAway = scenarios.latchedAimAway;
expect(latchedAway.aimedAway.vacuum.active && latchedAway.aimedAway.vacuum.target === 0 &&
    soul(latchedAway.aimedAway).state === "INGESTING",
  "aim loss after latch preserves target ownership and ingestion");

for (const sample of scenarios.cameraPitchAlignment.samples) {
  expect(Math.abs(soul(sample.snapshot).projectedNdc[0]) < 1e-6 &&
      Math.abs(soul(sample.snapshot).projectedNdc[1]) < 1e-6 &&
      soul(sample.snapshot).cameraRayDistance < 1e-6 &&
      soul(sample.snapshot).insideAttractionOffer,
    `rendered center and offer ray coincide at pitch ${sample.pitch}`);
}

const rapid = scenarios.rapidTurn.turnFrames;
expect(!soul(rapid[1]).insideAttractionOffer && rapid[1].vacuum.target === 0 && soul(rapid[1]).state === "ATTRACTED",
  "browser update order retains the prior target for one rendered frame after rapid aim loss");
expect(rapid[2].vacuum.target === -1 && soul(rapid[2]).state === "FREE",
  "the following browser frame releases the pre-latch soul");
expect(latchedAway.acquired.crosshair.rotationDegrees !== latchedAway.aimedAway.crosshair.rotationDegrees,
  "vacuum crosshair rotor continues accumulating through aim loss");

const energy = scenarios.supplementalEnergy;
expect(energy.afterOneStack.energy.flowerStacks === 1 && energy.afterOneStack.energy.supplemental === 46 &&
    energy.afterOneStack.energy.supplementalMax === 85,
  "the first flower stack adds 46 supplemental power with an 85 maximum");
expect(energy.afterTwoStacks.energy.flowerStacks === 2 && energy.afterTwoStacks.energy.supplemental === 92 &&
    energy.afterTwoStacks.energy.supplementalMax === 117,
  "the second flower stack raises supplemental maximum by 32");
expect(energy.afterVacuum.energy.main === 50 && energy.afterVacuum.energy.supplemental < 92,
  "supplemental power absorbs vacuum drain before main battery");
expect(Math.abs(energy.afterVacuum.energy.supplemental - 90.812) < 1e-9,
  "the 60 FPS oracle records the exact one-second vacuum supplemental drain");
expect(energy.afterClear.energy.flowerStacks === 0 && energy.afterClear.energy.supplemental === 0 &&
    energy.afterClear.energy.supplementalMax === 85,
  "clearing flower power restores the empty 85-point supplemental state");

const edges = scenarios.energyEdges;
expect(!edges.inactiveFeed.fed && edges.inactiveFeed.energy.supplemental === 0,
  "flower feed is ignored while supplemental power is inactive");
expect(edges.clampedPickup.energy.supplemental === 85 && edges.clampedPickup.energy.supplementalMax === 85,
  "a first flower pickup clamps oversized stock to its 85-point maximum");
expect(Math.abs(edges.efficientSpend.energy.supplemental - (85 - 7 / 1.8)) < 1e-9,
  "five stored souls reduce a seven-point cost before supplemental consumption");
expect(edges.depletion.energy.main === 49 && edges.depletion.energy.flowerStacks === 0 &&
    edges.depletion.energy.supplemental === 0 && edges.depletion.energy.supplementalMax === 85,
  "supplemental depletion clears stacks and passes only the remainder to main battery");
expect(edges.afterRoom.energy.flowerStacks === edges.beforeRoom.energy.flowerStacks &&
    edges.afterRoom.energy.supplemental === edges.beforeRoom.energy.supplemental &&
    edges.afterRoom.energy.main === edges.beforeRoom.energy.main + 18,
  "room advancement preserves flower power and grants 18 main battery");

const projectile = scenarios.projectileWeight;
const firstShot = (run) => run.frames.findIndex(frame => frame.souls.some(item => item.depositShot));
const shotAt = (run, index) => run.frames[index].souls.find(item => item.depositShot);
const normalRelease = firstShot(projectile.normal);
const bruteRelease = firstShot(projectile.brute);
expect(normalRelease === 5 && bruteRelease === 5,
  "fresh normal and brute discharges release on the sixth 60 FPS frame");
const normalShot = shotAt(projectile.normal, normalRelease);
const bruteShot = shotAt(projectile.brute, bruteRelease);
expect(normalShot.kind === 0 && bruteShot.kind === 1 && Math.abs(normalShot.health - 1) < 1e-6 && Math.abs(bruteShot.health - 2.8) < 1e-5,
  "discharged normal and brute cubes retain their authoritative kind and health");
expect(Math.abs(normalShot.velocity[2] + 24.832477569580078) < 1e-6,
  "normal discharge applies 25 launch speed, gravity, and exponential drag");
expect(Math.abs(bruteShot.velocity[2] + 19.86598014831543) < 1e-6,
  "brute discharge applies its heavier 20 launch speed with identical drag");
expect(bruteShot.velocity[1] > normalShot.velocity[1],
  "the same vertical lift produces a heavier brute trajectory under clamped downward aim");

console.log(`Pass 7 oracle suite verified: ${passed} assertions.`);
