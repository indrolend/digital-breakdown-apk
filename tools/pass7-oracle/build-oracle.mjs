import { createHash } from "node:crypto";
import { mkdir, readFile, rm, writeFile, cp } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const toolDir = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(toolDir, "../..");
const referenceDir = join(repoRoot, "reference", "browser-pass7");
const outputDir = join(repoRoot, "build", "pass7-oracle");
const runtimePath = join(referenceDir, "index_module.mjs");
const htmlPath = join(referenceDir, "index.html");

const runtime = await readFile(runtimePath, "utf8");
const html = await readFile(htmlPath, "utf8");
const sourceSha256 = createHash("sha256").update(runtime).digest("hex");

const replacements = [
  [
    "frame.now = performance.now();",
    "frame.now = window.__PASS7_ORACLE_NOW__ ?? performance.now();"
  ],
  [
    "frame.dt = Math.min(clock.getDelta(), 0.1);",
    "frame.dt = window.__PASS7_ORACLE_DT__ ?? Math.min(clock.getDelta(), 0.1);"
  ]
];

let instrumented = runtime;
for (const [from, to] of replacements) {
  const occurrences = instrumented.split(from).length - 1;
  if (occurrences !== 1) {
    throw new Error(`Expected exactly one oracle hook site for ${JSON.stringify(from)}, found ${occurrences}.`);
  }
  instrumented = instrumented.replace(from, to);
}

const bridge = String.raw`

// Generated Pass 7 executable-oracle bridge. Gameplay remains in the authoritative
// module above; this bridge only controls time/input/fixture placement and projects state.
window.__PASS7_ORACLE_STAGE__ = "bridge-enter";
document.documentElement.dataset.pass7OracleStage = "bridge-enter";
const __oracleStateNames = ["FREE", "ATTRACTED", "LATCHED", "INGESTING", "RECOILING"];
const __oracleVec = (v) => [v.x, v.y, v.z];
const __oracleQuat = (q) => [q.x, q.y, q.z, q.w];
const __oracleMatrix = (object) => {
	object.updateWorldMatrix(true, false);
	return Array.from(object.matrixWorld.elements);
};
const __oracleCrosshair = () => {
	const rotorTransform = crosshairRotor?.style.transform || "";
	const armTransform = crosshairArms.top?.style.transform || "";
	const rotation = Number((rotorTransform.match(/rotate\(([-+0-9.eE]+)deg\)/) || [0, 0])[1]);
	const topY = Number((armTransform.match(/translate\([^,]+,\s*([-+0-9.eE]+)px\)/) || [0, 0])[1]);
	return {
		visible: crosshair?.style.display !== "none",
		classes: crosshair ? Array.from(crosshair.classList) : [],
		hasAimTarget: crosshairState.hasAimTarget,
		rotationDegrees: rotation,
		spreadPixels: Math.abs(topY),
		vacuumSpinDegrees: crosshairState.vacuumSpin,
		shootJoinAgeMs: frame.now - shootState.cursorJoinTime
	};
};
const __oracleSoul = (i) => {
	const idx = targetIndex(i);
	const local = new THREE.Vector3(targetData[idx + T_X], targetData[idx + T_Y], targetData[idx + T_Z]);
	const world = local.clone();
	world.z = getNearestRepeatedWorldZ(local.z, vacuumPullPoint.z);
	const cameraToSoul = world.clone().sub(shotOrigin);
	const forwardDistance = cameraToSoul.dot(shotDir);
	const radialDistance = cameraToSoul.clone().addScaledVector(shotDir, -forwardDistance).length();
	const projected = world.clone().project(camera);
	return {
		index: i,
		alive: targetData[idx + T_ALIVE] > 0.5,
		slurpable: isTargetSlurpable(idx),
		kind: targetData[idx + T_KIND] || 0,
		state: __oracleStateNames[soulState[i]] || String(soulState[i]),
		stateValue: soulState[i],
		localPosition: __oracleVec(local),
		nearestWorldPosition: __oracleVec(world),
		velocity: [targetData[idx + T_VX], targetData[idx + T_VY], targetData[idx + T_VZ]],
		health: targetData[idx + T_HEALTH],
		ingestProgress: targetIngestProgress[i],
		latchedToScreen: !!targetLatchedToScreen[i],
		captureQueued: !!soulCaptureQueued[i],
		captureCommitted: !!soulCaptureCommitted[i],
		depositShot: !!soulDepositShot[i],
		insideCaptureCylinder: isSoulInsideScreenCaptureCylinder(world.x, world.y, world.z),
		insideAttractionOffer: isSoulInAttractionOffer(world.x, world.y, world.z),
		distanceToPullPoint: world.distanceTo(vacuumPullPoint),
		cameraForwardDistance: forwardDistance,
		cameraRayDistance: radialDistance,
		projectedNdc: __oracleVec(projected)
	};
};

window.__PASS7_ORACLE__ = Object.freeze({
	version: 1,
	source: Object.freeze({
		path: "reference/browser-pass7/index_module.mjs",
		sha256: "${sourceSha256}",
		hookCount: ${replacements.length}
	}),
	start() {
		startGame();
		game.uiPaused = false;
		return this.snapshot();
	},
	resetScenario() {
		this.start();
		window.__PASS7_ORACLE_NOW__ = 0;
		window.__PASS7_ORACLE_DT__ = 1 / 60;
		frame.time = 0;
		frame.now = 0;
		frame.dt = 0;
		vacuum.active = false;
		vacuum.power = 0;
		vacuum.pose = 0;
		vacuum.fieldStrength = 0;
		vacuum.coneTightness = 0;
		targetLock.target = -1;
		targetLock.strength = 0;
		crosshairState.hasAimTarget = false;
		crosshairState.vacuumSpin = 0;
		shootState.cursorJoinTime = -9999;
		this.clearSouls();
		this.setCursor(0, 0);
		this.step(1, 1 / 60);
		return this.snapshot();
	},
	setFixedDelta(dt) {
		if (!(dt > 0 && dt <= 0.1)) throw new RangeError("dt must be in (0, 0.1].");
		window.__PASS7_ORACLE_DT__ = dt;
		return dt;
	},
	setVacuum(active) {
		if (active) handlePrimaryActionPress(); else stopHeldActions();
		return vacuum.active;
	},
	setMainBattery(value) {
		battery.value = THREE.MathUtils.clamp(Number(value) || 0, 0, battery.max);
		return battery.value;
	},
	addFlowerStack(amount = FLOWER_PICKUP_VALUE) {
		addPowerupStack("flower", amount);
		return { ...activePowerupStacks, value: supplementalBattery.value, max: supplementalBattery.max };
	},
	spendEnergy(amount, reason = "COMPUTE") {
		return { survived:spendBattery(amount, reason), energy:this.snapshot().energy };
	},
	feedFlowerEnergy(amount, reason = "FEED") {
		return { fed:feedSupplementalBattery(amount, reason), energy:this.snapshot().energy };
	},
	setStoredSoulCount(count) {
		phoneStorage.storedCubes.length = 0;
		for (let i=0; i<THREE.MathUtils.clamp(count|0,0,PHONE_CAPACITY); i++) phoneStorage.storedCubes.push({ kind:0, scale:1, value:1 });
		syncStoredMirror();
		return getStoredCount();
	},
	storeCube(kind = 0) {
		if (phoneStorage.storedCubes.length >= PHONE_CAPACITY) return false;
		phoneStorage.storedCubes.push({ kind:kind|0, scale:(kind|0)===1?1.7:1, value:(kind|0)===1?3:1 });
		syncStoredMirror();
		return true;
	},
	fireStoredCube() {
		fireStoredCube();
		return this.snapshot();
	},
	advanceRoom() {
		enterNextRoomThroughPortal();
		return this.snapshot();
	},
	clearFlowers() {
		clearFlowerPowerups();
		clearActivePowerups();
		return true;
	},
	setCursor(yaw, pitch = cursor.pitch) {
		cursor.yaw = yaw;
		cursor.pitch = THREE.MathUtils.clamp(pitch, -cursor.maxPitch, cursor.maxPitch);
		return { yaw: cursor.yaw, pitch: cursor.pitch };
	},
	setKey(code, down) {
		setKey(code, !!down);
		return !!input.keys[code];
	},
	clearSouls() {
		for (let i = 0; i < TARGET_COUNT; i++) {
			const idx = targetIndex(i);
			for (let field = 0; field < TARGET_STRIDE; field++) targetData[idx + field] = 0;
			setSoulState(i, SOUL_FREE);
			clearSoulCaptureFlags(i);
			soulDepositShot[i] = 0;
			soulMorphTime[i] = 0;
		}
		targetLock.target = -1;
		targetLock.strength = 0;
		return true;
	},
	placeSoul(i, options = {}) {
		if (!Number.isInteger(i) || i < 0 || i >= TARGET_COUNT) throw new RangeError("invalid soul index");
		const idx = targetIndex(i);
		const position = options.position || [player.pos.x, player.pos.y + 0.5, player.pos.z - 4];
		targetData[idx + T_X] = position[0];
		targetData[idx + T_Y] = position[1];
		targetData[idx + T_Z] = position[2];
		targetData[idx + T_RENDER_Y] = position[1];
		targetData[idx + T_ALIVE] = 1;
		targetData[idx + T_SCALE] = options.scale ?? 1;
		targetData[idx + T_HEALTH] = options.health ?? 1;
		targetData[idx + T_KIND] = options.kind ?? 0;
		targetData[idx + T_SLURPABLE] = options.slurpable === false ? 0 : 1;
		targetData[idx + T_ARMOR] = options.armor ?? 0;
		soulMorphTime[i] = options.slurpable === false ? 0 : SOUL_MORPH_DURATION;
		setSoulState(i, options.state ?? SOUL_FREE);
		clearSoulCaptureFlags(i);
		return __oracleSoul(i);
	},
	placeSoulOnCameraRay(i = 0, distance = 5, options = {}) {
		camera.getWorldPosition(shotOrigin);
		camera.getWorldDirection(shotDir).normalize();
		const position = shotOrigin.clone().addScaledVector(shotDir, distance);
		return this.placeSoul(i, { ...options, position: __oracleVec(position) });
	},
	runVacuumAimAwayScenario() {
		this.resetScenario();
		this.placeSoulOnCameraRay(0, 5, { slurpable: true });
		const idle = this.snapshot();
		this.setVacuum(true);
		const charging = this.step(30, 1 / 60);
		const acquired = charging[charging.length - 1];
		this.setCursor(Math.PI, 0);
		const awayFrames = this.step(12, 1 / 60);
		const aimedAway = awayFrames[awayFrames.length - 1];
		this.setCursor(0, 0);
		const returnFrames = this.step(12, 1 / 60);
		const returned = returnFrames[returnFrames.length - 1];
		return { name: "vacuum-acquire-aim-away-return", fixedDt: 1 / 60, idle, charging, acquired, awayFrames, aimedAway, returnFrames, returned };
	},
	runFreeAimAwayScenario() {
		this.resetScenario();
		this.placeSoulOnCameraRay(0, 7, { slurpable: true });
		const centered = this.step(1, 1 / 60)[0];
		this.setCursor(Math.PI, 0);
		this.setVacuum(true);
		const awayFrames = this.step(30, 1 / 60);
		const aimedAway = awayFrames[awayFrames.length - 1];
		this.setCursor(0, 0);
		const returnFrames = this.step(12, 1 / 60);
		return { name: "free-aim-away-return", fixedDt: 1 / 60, centered, awayFrames, aimedAway, returnFrames, returned: returnFrames[returnFrames.length - 1] };
	},
	runAttractedAimAwayScenario() {
		this.resetScenario();
		this.placeSoulOnCameraRay(0, 12, { slurpable: true });
		this.setVacuum(true);
		const acquireFrames = [];
		let attracted = null;
		for (let i = 0; i < 60; i++) {
			const snapshot = this.step(1, 1 / 60)[0];
			acquireFrames.push(snapshot);
			if (snapshot.souls[0]?.state === "ATTRACTED") { attracted = snapshot; break; }
		}
		if (!attracted) throw new Error("scenario never reached ATTRACTED");
		this.setCursor(Math.PI, 0);
		const awayFrames = this.step(8, 1 / 60);
		const aimedAway = awayFrames[awayFrames.length - 1];
		this.setCursor(0, 0);
		const returnFrames = this.step(12, 1 / 60);
		return { name: "attracted-aim-away-return", fixedDt: 1 / 60, acquireFrames, attracted, awayFrames, aimedAway, returnFrames, returned: returnFrames[returnFrames.length - 1] };
	},
	runCameraPitchAlignmentScenario() {
		const samples = [];
		for (const pitch of [-1.2, -0.75, -0.35, 0, 0.35, 0.75, 1.2]) {
			this.resetScenario();
			this.setCursor(0, pitch);
			this.step(2, 1 / 60);
			this.placeSoulOnCameraRay(0, 6, { slurpable: true });
			const snapshot = this.step(1, 1 / 60)[0];
			samples.push({ pitch, snapshot });
		}
		return { name: "camera-pitch-crosshair-alignment", fixedDt: 1 / 60, samples };
	},
	runRapidTurnScenario() {
		this.resetScenario();
		this.placeSoulOnCameraRay(0, 10, { slurpable: true });
		this.setVacuum(true);
		const warmup = this.step(8, 1 / 60);
		const turnFrames = [];
		for (let i = 1; i <= 12; i++) {
			this.setCursor((Math.PI * 0.75) * (i / 12), 0.45 * Math.sin(i / 12 * Math.PI));
			turnFrames.push(this.step(1, 1 / 60)[0]);
		}
		return { name: "rapid-turn-phone-screen-divergence", fixedDt: 1 / 60, warmup, turnFrames };
	},
	runSupplementalEnergyScenario() {
		this.resetScenario();
		this.clearFlowers();
		this.setMainBattery(50);
		const base = this.snapshot();
		this.addFlowerStack();
		const afterOneStack = this.snapshot();
		this.addFlowerStack();
		const afterTwoStacks = this.snapshot();
		this.setVacuum(true);
		const vacuumFrames = this.step(60, 1 / 60);
		const afterVacuum = vacuumFrames[vacuumFrames.length - 1];
		this.setVacuum(false);
		this.clearFlowers();
		const afterClear = this.snapshot();
		return { name:"supplemental-energy-priority", fixedDt:1/60, base, afterOneStack, afterTwoStacks, vacuumFrames, afterVacuum, afterClear };
	},
	runEnergyEdgeScenario() {
		this.resetScenario();
		this.clearFlowers();
		this.setMainBattery(50);
		const inactiveFeed = this.feedFlowerEnergy(9, "SLURP");
		this.addFlowerStack(1000);
		const clampedPickup = this.snapshot();
		this.setStoredSoulCount(5);
		const efficientSpend = this.spendEnergy(7, "DISCHARGE");
		this.clearFlowers();
		this.addFlowerStack(2);
		this.setStoredSoulCount(0);
		const depletion = this.spendEnergy(3, "COMPUTE");
		this.addFlowerStack();
		const beforeRoom = this.snapshot();
		const afterRoom = this.advanceRoom();
		return { name:"energy-edge-and-room-semantics", inactiveFeed, clampedPickup, efficientSpend, depletion, beforeRoom, afterRoom };
	},
	runProjectileWeightScenario() {
		const runKind = (kind) => {
			this.resetScenario();
			this.clearSouls();
			actionPose.discharge = 0;
			actionPose.dischargeTimer = 0;
			actionPose.screenForwardTurn = 0;
			this.setMainBattery(100);
			this.storeCube(kind);
			const fired = this.fireStoredCube();
			const frames = this.step(18, 1/60);
			return { kind, fired, frames };
		};
		return { name:"projectile-weight-and-release", fixedDt:1/60, normal:runKind(0), brute:runKind(1) };
	},
	runProjectileHumanHitScenario() {
		const runKind = (kind) => {
			this.resetScenario();
			this.clearSouls();
			actionPose.discharge = 0;
			actionPose.dischargeTimer = 0;
			actionPose.screenForwardTurn = 0;
			this.placeSoul(0,{ position:[0, HUMAN_GROUND_Y, 14.25], slurpable:false, armor:4, health:1 });
			this.setMainBattery(100);
			this.storeCube(kind);
			this.fireStoredCube();
			return { kind, frames:this.step(18,1/60) };
		};
		return { name:"projectile-swept-human-hit", fixedDt:1/60, normal:runKind(0), brute:runKind(1) };
	},
	runOracleSuite() {
		return {
			schemaVersion: 1,
			source: this.source,
			scenarios: {
				latchedAimAway: this.runVacuumAimAwayScenario(),
				freeAimAway: this.runFreeAimAwayScenario(),
				attractedAimAway: this.runAttractedAimAwayScenario(),
				cameraPitchAlignment: this.runCameraPitchAlignmentScenario(),
				rapidTurn: this.runRapidTurnScenario(),
				supplementalEnergy: this.runSupplementalEnergyScenario(),
				energyEdges: this.runEnergyEdgeScenario(),
				projectileWeight: this.runProjectileWeightScenario(),
				projectileHumanHit: this.runProjectileHumanHitScenario()
			}
		};
	},
	step(count = 1, dt = window.__PASS7_ORACLE_DT__ ?? 1 / 60) {
		this.setFixedDelta(dt);
		const frames = [];
		for (let i = 0; i < count; i++) {
			window.__PASS7_ORACLE_NOW__ += dt * 1000;
			updateFrame(window.__PASS7_ORACLE_NOW__);
			renderer.render(scene, camera);
			frames.push(this.snapshot());
		}
		return frames;
	},
	snapshot() {
		camera.updateWorldMatrix(true, false);
		phone.updateWorldMatrix(true, true);
		screen.updateWorldMatrix(true, false);
		camera.getWorldPosition(shotOrigin);
		camera.getWorldDirection(shotDir).normalize();
		updateScreenPlaneBasis();
		updateFirstPersonVacuumPullPoint();
		const cameraScreenDot = THREE.MathUtils.clamp(shotDir.dot(screenPlaneNormal), -1, 1);
		const souls = [];
		for (let i = 0; i < TARGET_COUNT; i++) {
			if (targetData[targetIndex(i) + T_ALIVE] > 0.5 || soulState[i] !== SOUL_FREE) souls.push(__oracleSoul(i));
		}
		return {
			frame: { timeMs: frame.time, nowMs: frame.now, dt: frame.dt },
			game: { mode: game.mode, started: game.started, dead: game.dead, uiPaused: game.uiPaused },
			cursor: { yaw: cursor.yaw, pitch: cursor.pitch },
			camera: {
				mode: cameraMode.current,
				position: __oracleVec(camera.position),
				quaternion: __oracleQuat(camera.quaternion),
				worldDirection: __oracleVec(shotDir),
				worldMatrix: __oracleMatrix(camera)
			},
			phone: {
				position: __oracleVec(phone.position),
				quaternion: __oracleQuat(phone.quaternion),
				worldMatrix: __oracleMatrix(phone),
				screenWorldMatrix: __oracleMatrix(screen),
				screenCenter: __oracleVec(screenPlaneCenter),
				screenNormal: __oracleVec(screenPlaneNormal),
				pullPoint: __oracleVec(vacuumPullPoint),
				cameraScreenNormalDot: cameraScreenDot,
				cameraScreenNormalAngleDegrees: THREE.MathUtils.radToDeg(Math.acos(cameraScreenDot))
			},
			vacuum: {
				active: vacuum.active,
				power: vacuum.power,
				pose: vacuum.pose,
				fieldStrength: vacuum.fieldStrength,
				coneTightness: vacuum.coneTightness,
				target: targetLock.target,
				lockStrength: targetLock.strength
			},
			energy: {
				main: battery.value,
				mainMax: battery.max,
				activity: battery.activity,
				storedSoulEfficiencyMultiplier: batteryDrainMultiplier(),
				supplementalActive: supplementalBattery.active,
				supplemental: supplementalBattery.value,
				supplementalMax: supplementalBattery.max,
				flowerStacks: activePowerupStacks.flower || 0
			},
			flowers: flowerPowerups.map((powerup, index) => ({
				index,
				position: powerup.mesh ? __oracleVec(powerup.mesh.position) : [powerup.x, powerup.y, powerup.z],
				canonicalPosition: [powerup.x, powerup.y, powerup.z],
				age: powerup.age,
				value: powerup.value,
				type: powerup.type,
				rotationY: powerup.mesh?.rotation.y ?? null,
				continuationCount: powerup.continuationMeshes?.length || 0
			})),
			crosshair: __oracleCrosshair(),
			souls
		};
	}
});
window.__PASS7_ORACLE_STAGE__ = "ready";
document.documentElement.dataset.pass7OracleStage = "ready";
try {
	const suite = window.__PASS7_ORACLE__.runOracleSuite();
	const trace = suite.scenarios.latchedAimAway;
	window.__PASS7_ORACLE_TRACES__ = suite.scenarios;
	const pick = (snapshot) => ({
		frame: snapshot.frame,
		camera: snapshot.camera,
		phone: snapshot.phone,
		vacuum: snapshot.vacuum,
		crosshair: snapshot.crosshair,
		soul: snapshot.souls[0] || null
	});
	const evidence = {
		schemaVersion: 1,
		source: window.__PASS7_ORACLE__.source,
		scenario: "pass7-vacuum-crosshair-suite",
		fixedDt: trace.fixedDt,
		keyframes: {
			idle: pick(trace.idle),
			acquired: pick(trace.acquired),
			aimedAway: pick(trace.aimedAway),
			returned: pick(trace.returned)
		},
		chargingCrosshair: trace.charging.map((snapshot) => ({
			nowMs: snapshot.frame.nowMs,
			power: snapshot.vacuum.power,
			lockStrength: snapshot.vacuum.lockStrength,
			rotationDegrees: snapshot.crosshair.rotationDegrees,
			spreadPixels: snapshot.crosshair.spreadPixels
		})),
		classification: {
			freeAimAway: pick(suite.scenarios.freeAimAway.aimedAway),
			freeReturned: pick(suite.scenarios.freeAimAway.returned),
			attracted: pick(suite.scenarios.attractedAimAway.attracted),
			attractedAimAway: pick(suite.scenarios.attractedAimAway.aimedAway),
			attractedReturned: pick(suite.scenarios.attractedAimAway.returned),
			cameraPitchAlignment: suite.scenarios.cameraPitchAlignment.samples.map(({ pitch, snapshot }) => ({ pitch, camera: snapshot.camera, soul: snapshot.souls[0] })),
			rapidTurn: suite.scenarios.rapidTurn.turnFrames.map(pick)
		}
	};
	const output = document.createElement("pre");
	output.id = "pass7-oracle-output";
	output.dataset.scenario = evidence.scenario;
	output.textContent = JSON.stringify(evidence, null, 2);
	output.style.cssText = "position:fixed;inset:12px;z-index:100000;overflow:auto;padding:16px;background:#071018;color:#9eeaff;font:12px/1.4 monospace;white-space:pre-wrap;";
	document.body.appendChild(output);
	fetch("/__pass7_oracle_trace", {
		method: "POST",
		headers: { "Content-Type": "application/json" },
		body: JSON.stringify(suite, null, 2)
	}).catch(() => {});
	document.documentElement.dataset.pass7OracleStage = "scenario-complete";
} catch (error) {
	document.documentElement.dataset.pass7OracleStage = "scenario-error";
	document.documentElement.dataset.pass7OracleError = String(error?.message || error);
}
window.dispatchEvent(new CustomEvent("pass7-oracle-ready"));
`;

instrumented += bridge;

const moduleStart = html.indexOf('<script type="module">');
const moduleEnd = html.lastIndexOf("</script>");
if (moduleStart < 0 || moduleEnd <= moduleStart) {
  throw new Error("Unable to locate the authoritative inline module in index.html.");
}
const openingEnd = moduleStart + '<script type="module">'.length;
const oracleHtml = html.slice(0, moduleStart) +
  `<script>\nwindow.__PASS7_ORACLE_STAGE__ = "bootstrap";\nwindow.__PASS7_ORACLE_NOW__ = 0;\nwindow.__PASS7_ORACLE_DT__ = 0;\nwindow.__PASS7_ORACLE_RAF__ = null;\nwindow.requestAnimationFrame = (callback) => { window.__PASS7_ORACLE_RAF__ = callback; return 1; };\nwindow.cancelAnimationFrame = () => {};\n</script>\n<script type="module" src="index_module.oracle.mjs?v=${sourceSha256.slice(0, 12)}">` +
  html.slice(moduleEnd);

await rm(outputDir, { recursive: true, force: true });
await mkdir(outputDir, { recursive: true });
await writeFile(join(outputDir, "index_module.oracle.mjs"), instrumented);
await writeFile(join(outputDir, "index.html"), oracleHtml);
await cp(join(referenceDir, "assets"), join(outputDir, "assets"), { recursive: true });
await writeFile(join(outputDir, "oracle-manifest.json"), JSON.stringify({
  schemaVersion: 1,
  authoritativeRuntime: "reference/browser-pass7/index_module.mjs",
  sourceSha256,
  generatedRuntime: "build/pass7-oracle/index_module.oracle.mjs",
  hooks: replacements.map(([from, to]) => ({ from, to }))
}, null, 2) + "\n");

console.log(`Pass 7 oracle generated at ${outputDir}`);
console.log(`Authoritative runtime SHA-256: ${sourceSha256}`);
