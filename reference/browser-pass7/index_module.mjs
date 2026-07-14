import * as THREE from "three";
import { RectAreaLightUniformsLib } from "three/addons/lights/RectAreaLightUniformsLib.js";
import { FBXLoader } from "three/addons/loaders/FBXLoader.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { clone as cloneSkeleton } from "three/addons/utils/SkeletonUtils.js";

RectAreaLightUniformsLib.init();

const DIGITAL_BREAKDOWN_ASSETS = window.DIGITAL_BREAKDOWN_ASSETS || {};
const {
	PENTAGONAL_FLOWER_GLB_BASE64,
	IPHONE_GLB_BASE64,
	HUMAN_FBX_BASE64,
	SFX_AUDIO_BASE64 = Object.freeze({}),
	PMC_RUN_FRAME_DATA = []
} = DIGITAL_BREAKDOWN_ASSETS;


/* iPhone 17 Pro model uploaded by user, embedded as local GLB data.
   Screen geometry/materials are explicit in the asset: Screen_BG, Screen_Glass, Screen_Rim. */
// PENTAGONAL_FLOWER_GLB_BASE64 moved to assets/embedded-assets.js

// IPHONE_GLB_BASE64 moved to assets/embedded-assets.js

const IPHONE_MODEL_HEIGHT = 0.16;
const phoneScreenMaterials = [];
function setPhoneScreenEmission(color, intensity) {
	if (screen && screen.material && screen.material.emissive) {
		screen.material.emissive.set(color);
		screen.material.emissiveIntensity = intensity;
	}
	for (const material of phoneScreenMaterials) {
		if (!material || !material.emissive) continue;
		material.emissive.set(color);
		material.emissiveIntensity = intensity;
	}
}

const canvas = document.getElementById("game-canvas");
const datamoshCanvas = document.getElementById("datamosh-canvas");
const datamoshCtx = datamoshCanvas ? datamoshCanvas.getContext("2d", { alpha: true }) : null;
const datamoshFeedbackCanvas = document.createElement("canvas");
const datamoshFeedbackCtx = datamoshFeedbackCanvas.getContext("2d", { alpha: false });
const datamoshFrozenCanvas = document.createElement("canvas");
const datamoshFrozenCtx = datamoshFrozenCanvas.getContext("2d", { alpha: false });
const datamoshLastWorldCanvas = document.createElement("canvas");
const datamoshLastWorldCtx = datamoshLastWorldCanvas.getContext("2d", { alpha: false });
let datamoshFrameReady = false;
let datamoshLastWorldReady = false;
const startOverlay = document.getElementById("start-overlay");
const startButton = document.getElementById("start");
const loadingStatus = document.getElementById("loading-status");
const crosshair = document.getElementById("crosshair");
const modeLabel = document.getElementById("mode-label");
const cashLabel = document.getElementById("cash-label");
const soulWindow = document.getElementById("soul-window");
const soulWindowTitle = document.getElementById("soul-window-title");
const soulOrbit = document.getElementById("soul-orbit");
const soulCountLabel = document.getElementById("soul-count-label");
const roomHud = document.getElementById("room-hud");
const roomLabel = document.getElementById("room-label");
const roomProgressLabel = document.getElementById("room-progress-label");
const roomDoorLabel = document.getElementById("room-door-label");
const goalSlots = document.getElementById("goal-slots");
const batteryHud = document.getElementById("battery-hud");
const batteryLabel = document.getElementById("battery-label");
const batteryFill = document.getElementById("battery-fill");
const batteryNote = document.getElementById("battery-note");
const itemHud = document.getElementById("item-hud");
const itemFill = document.getElementById("item-fill");
const itemLabel = document.getElementById("item-label");
const comboPop = document.getElementById("combo-pop");
const batteryEvents = document.getElementById("battery-events");
const gameMusic = document.getElementById("game-music");
const gameOverMusic = document.getElementById("game-over-music");
const eventSoundUrls = {
	vcEnded: "audio/vc_ended.mp3",
	vcInvitation: "audio/vc_invitation.mp3",
	connectPower: "audio/connect_power.mp3",
	lowPower: "audio/low_power.mp3",
	negativeAck: "audio/negative_ack.mp3",
	receivedMessage: "audio/received_message.mp3",
	sentMessage: "audio/sent_message.mp3",
	phoneAttack: "audio/phone_attack.mp3"
,
	paymentSuccess: "audio/payment_success.mp3",
	paymentFailure: "audio/payment_failure.mp3",
	endCallTone: "audio/end_call_tone.mp3",
	slurpRingtone: "audio/slurp_ringtone.mp3"
};
// SFX_AUDIO_BASE64 moved to assets/embedded-assets.js

const EVENT_SFX_LOW_THRESHOLD = 0.24;
const EVENT_SFX_VERY_LOW_THRESHOLD = 0.14;
const EVENT_SFX_DAMAGE_COOLDOWN = 0.18;
const eventSoundState = {
	lowPowerArmed: true,
	connectPowerArmed: false,
	lastDamageAckTime: -9999
};


const captureSlotSoundUrls = [
  'audio/capture_1.mp3',
  'audio/capture_2.mp3',
  'audio/capture_3.mp3',
  'audio/capture_4.mp3',
  'audio/capture_5.mp3'
];
const captureSlotSoundAssignment = [];
const sfxSourceKeys = new Map([
	...Object.entries(eventSoundUrls).map(([key, src]) => [src, key]),
	[captureSlotSoundUrls[0], "capture_1"],
	[captureSlotSoundUrls[1], "capture_2"],
	[captureSlotSoundUrls[2], "capture_3"],
	[captureSlotSoundUrls[3], "capture_4"],
	[captureSlotSoundUrls[4], "capture_5"]
]);

function shuffleCaptureSlotSounds() {
	captureSlotSoundAssignment.length = 0;
	const deck = captureSlotSoundUrls.map((_, i) => i);
	for (let i = deck.length - 1; i > 0; i--) {
		const j = Math.floor(Math.random() * (i + 1));
		const tmp = deck[i];
		deck[i] = deck[j];
		deck[j] = tmp;
	}
	const needed = room && room.requiredSouls ? room.requiredSouls : captureSlotSoundUrls.length;
	for (let i = 0; i < needed; i++) {
		captureSlotSoundAssignment[i] = deck[i % deck.length];
	}
}

const SFX_POOL_SIZE = 4;
const SFX_POOL_MAX_ACTIVE_PER_SOUND = 2;
const SFX_GLOBAL_MAX_ACTIVE = 8;
const sfxPools = new Map();
const sfxRecentPlays = new Map();

let sfxAudioContext = null;
const sfxBufferCache = new Map();
const sfxDecodePromises = new Map();
const sfxActiveVoices = new Set();
let slurpRingtoneSource = null;
let slurpRingtoneGain = null;
let slurpRingtonePlaying = false;
let slurpRingtoneStarting = false;

function getSfxAudioContext() {
	if (!sfxAudioContext) {
		const Ctx = window.AudioContext || window.webkitAudioContext;
		if (!Ctx) return null;
		sfxAudioContext = new Ctx({ latencyHint: "interactive" });
	}
	if (sfxAudioContext.state === "suspended") {
		const resumeAttempt = sfxAudioContext.resume();
		if (resumeAttempt && typeof resumeAttempt.catch === "function") resumeAttempt.catch(() => {});
	}
	return sfxAudioContext;
}

function sfxBase64ToArrayBuffer(base64) {
	const binary = atob(base64);
	const len = binary.length;
	const bytes = new Uint8Array(len);
	for (let i = 0; i < len; i++) bytes[i] = binary.charCodeAt(i);
	return bytes.buffer;
}

function getSfxKeyForSource(src) {
	return sfxSourceKeys.get(src) || src;
}

function decodeSfxBufferForKey(key) {
	if (sfxBufferCache.has(key)) return Promise.resolve(sfxBufferCache.get(key));
	if (sfxDecodePromises.has(key)) return sfxDecodePromises.get(key);
	const ctx = getSfxAudioContext();
	const base64 = SFX_AUDIO_BASE64[key];
	if (!ctx || !base64) return Promise.resolve(null);
	const promise = ctx.decodeAudioData(sfxBase64ToArrayBuffer(base64).slice(0))
		.then((buffer) => {
			sfxBufferCache.set(key, buffer);
			sfxDecodePromises.delete(key);
			return buffer;
		})
		.catch(() => {
			sfxDecodePromises.delete(key);
			return null;
		});
	sfxDecodePromises.set(key, promise);
	return promise;
}

function warmWebAudioSfx() {
	getSfxAudioContext();
	for (const key of Object.keys(SFX_AUDIO_BASE64)) decodeSfxBufferForKey(key);
}

function playDecodedSfxBuffer(buffer, volume = 0.55, options = {}) {
	const ctx = getSfxAudioContext();
	if (!ctx || !buffer || getSfxLevel() <= 0) return;
	if (sfxActiveVoices.size >= (options.globalMaxActive ?? SFX_GLOBAL_MAX_ACTIVE)) return;
	const source = ctx.createBufferSource();
	const gain = ctx.createGain();
	source.buffer = buffer;
	source.playbackRate.value = options.playbackRate ?? 1;
	gain.gain.value = THREE.MathUtils.clamp(volume * getSfxLevel(), 0, 1);
	source.connect(gain);
	gain.connect(ctx.destination);
	sfxActiveVoices.add(source);
	source.onended = () => {
		sfxActiveVoices.delete(source);
		try { source.disconnect(); } catch (err) {}
		try { gain.disconnect(); } catch (err) {}
	};
	try { source.start(0); } catch (err) { sfxActiveVoices.delete(source); }
}

function getSfxPool(src) {
	if (!src) return null;
	let pool = sfxPools.get(src);
	if (!pool) {
		pool = { cursor: 0, voices: [] };
		for (let i = 0; i < SFX_POOL_SIZE; i++) {
			const audio = new Audio(src);
			audio.preload = "auto";
			audio.loop = false;
			pool.voices.push(audio);
		}
		sfxPools.set(src, pool);
	}
	return pool;
}

function getActiveSfxCount(pool) {
	if (!pool) return 0;
	let active = 0;
	for (const audio of pool.voices) {
		if (!audio.paused && !audio.ended) active++;
	}
	return active;
}

function getGlobalActiveSfxCount() {
	let active = 0;
	for (const pool of sfxPools.values()) active += getActiveSfxCount(pool);
	return active;
}

function getAvailableSfxVoice(pool) {
	if (!pool) return null;
	for (let offset = 0; offset < pool.voices.length; offset++) {
		const index = (pool.cursor + offset) % pool.voices.length;
		const audio = pool.voices[index];
		if (audio.paused || audio.ended || audio.currentTime <= 0) {
			pool.cursor = (index + 1) % pool.voices.length;
			return audio;
		}
	}
	return null;
}

function playPooledSfx(src, volume = 0.55, options = {}) {
	if (!src || getSfxLevel() <= 0) return;
	const now = performance.now ? performance.now() : Date.now();
	const minInterval = options.minInterval ?? 0.04;
	const last = sfxRecentPlays.get(src) ?? -999999;
	if ((now - last) / 1000 < minInterval) return;
	if (sfxActiveVoices.size >= (options.globalMaxActive ?? SFX_GLOBAL_MAX_ACTIVE)) return;
	const key = getSfxKeyForSource(src);
	const cached = sfxBufferCache.get(key);
	if (cached) {
		sfxRecentPlays.set(src, now);
		playDecodedSfxBuffer(cached, volume, options);
		return;
	}
	if (SFX_AUDIO_BASE64[key]) {
		decodeSfxBufferForKey(key).then((buffer) => {
			if (!buffer) return;
			sfxRecentPlays.set(src, performance.now ? performance.now() : Date.now());
			playDecodedSfxBuffer(buffer, volume, options);
		});
		return;
	}
	const pool = getSfxPool(src);
	if (!pool) return;
	const perSoundCap = options.maxActive ?? SFX_POOL_MAX_ACTIVE_PER_SOUND;
	if (getActiveSfxCount(pool) >= perSoundCap) return;
	if (getGlobalActiveSfxCount() >= (options.globalMaxActive ?? SFX_GLOBAL_MAX_ACTIVE)) return;
	const audio = getAvailableSfxVoice(pool);
	if (!audio) return;
	sfxRecentPlays.set(src, now);
	try { audio.currentTime = 0; } catch (err) {}
	audio.volume = THREE.MathUtils.clamp(volume * getSfxLevel(), 0, 1);
	audio.playbackRate = options.playbackRate ?? 1;
	const playAttempt = audio.play();
	if (playAttempt && typeof playAttempt.catch === "function") {
		playAttempt.catch(() => {});
	}
}

function stopAllPooledSfx() {
	for (const source of Array.from(sfxActiveVoices)) {
		try { source.stop(0); } catch (err) {}
	}
	sfxActiveVoices.clear();
	for (const pool of sfxPools.values()) {
		for (const audio of pool.voices) {
			try { audio.pause(); audio.currentTime = 0; } catch (err) {}
		}
	}
}

function warmSfxPools() {
	warmWebAudioSfx();
}

function playCaptureSlotSound(slotIndex) {
	if (!game.started || game.dead) return;
	if (!captureSlotSoundAssignment.length) shuffleCaptureSlotSounds();
	const soundIndex = captureSlotSoundAssignment[slotIndex % captureSlotSoundAssignment.length];
	const src = captureSlotSoundUrls[soundIndex];
	duckAudioForCue("capture");
	playPooledSfx(src, 0.58, { playbackRate: 1, minInterval: 0.08, maxActive: 1 });
}

const controlsPanel = document.getElementById("controls-panel");
const controlsToggle = document.getElementById("controls-toggle");
const controlsBody = document.getElementById("controls-body");
const soundPanel = document.getElementById("sound-panel");
const soundToggle = document.getElementById("sound-toggle");
const graphicsPanel = document.getElementById("graphics-panel");
const graphicsToggle = document.getElementById("graphics-toggle");
const graphicsScaleSlider = document.getElementById("graphics-scale-slider");
const graphicsScaleFill = document.getElementById("graphics-scale-fill");
const graphicsScaleValue = document.getElementById("graphics-scale-value");
const graphicsShadowsToggle = document.getElementById("graphics-shadows-toggle");
const graphicsShadowsValue = document.getElementById("graphics-shadows-value");
const graphicsPortalToggle = document.getElementById("graphics-portal-toggle");
const graphicsPortalValue = document.getElementById("graphics-portal-value");
const graphicsParticlesToggle = document.getElementById("graphics-particles-toggle");
const graphicsParticlesValue = document.getElementById("graphics-particles-value");
const graphicsFpsToggle = document.getElementById("graphics-fps-toggle");
const graphicsFpsValue = document.getElementById("graphics-fps-value");
const graphicsFpsCounter = document.getElementById("graphics-fps-counter");
const graphicsDebugToggle = document.getElementById("graphics-debug-toggle");
const graphicsDebugValue = document.getElementById("graphics-debug-value");
const runtimeDebugOverlay = document.getElementById("runtime-debug-overlay");
const graphicsPresetValue = document.getElementById("graphics-preset-value");
const graphicsPresetLegacy = document.getElementById("graphics-preset-legacy");
const graphicsPresetNormal = document.getElementById("graphics-preset-normal");
const graphicsPresetPretty = document.getElementById("graphics-preset-pretty");
const musicVolumeSlider = document.getElementById("music-volume-slider");
const sfxVolumeSlider = document.getElementById("sfx-volume-slider");
const musicVolumeFill = document.getElementById("music-volume-fill");
const sfxVolumeFill = document.getElementById("sfx-volume-fill");
const musicVolumeValue = document.getElementById("music-volume-value");
const sfxVolumeValue = document.getElementById("sfx-volume-value");
const musicMuteToggle = document.getElementById("music-mute-toggle");
const sfxMuteToggle = document.getElementById("sfx-mute-toggle");
const doorCrossingMask = document.getElementById("door-transition-mask");
const audioSettings = { music: 0.70, sfx: 0.55, musicMuted: false, sfxMuted: false, lastMusic: 0.70, lastSfx: 0.55 };
const GRAPHICS_PRESETS = Object.freeze({
	legacy: Object.freeze({ renderScale: 0.75, shadows: false, portalWindow: false, particles: false }),
	normal: Object.freeze({ renderScale: 1.0, shadows: true, portalWindow: true, particles: true }),
	pretty: Object.freeze({ renderScale: 1.5, shadows: true, portalWindow: true, particles: true })
});
const GRAPHICS_RENDER_SCALE_MIN = 0.5;
const GRAPHICS_RENDER_SCALE_MAX = 2.0;
const graphicsSettings = { preset: "normal", renderScale: GRAPHICS_PRESETS.normal.renderScale, shadows: GRAPHICS_PRESETS.normal.shadows, portalWindow: GRAPHICS_PRESETS.normal.portalWindow, particles: GRAPHICS_PRESETS.normal.particles, fpsCounter: false, runtimeDebug: false };
const graphicsFpsState = { frames: 0, accum: 0, value: 0 };
const runtimeDebugState = { accum: 0, text: "" };

function setToggleButtonState(button, value, valueEl) {
	if (!button) return;
	button.setAttribute("aria-pressed", value ? "true" : "false");
	button.textContent = value ? "ON" : "OFF";
	if (valueEl) valueEl.textContent = value ? "ON" : "OFF";
}

function updateGraphicsPanel() {
	const pct = Math.round(graphicsSettings.renderScale * 100);
	if (graphicsScaleSlider) graphicsScaleSlider.value = String(pct);
	if (graphicsScaleFill) graphicsScaleFill.style.width = `${THREE.MathUtils.clamp((pct - 50) / 150 * 100, 0, 100)}%`;
	if (graphicsScaleValue) graphicsScaleValue.textContent = `${pct}%`;
	if (graphicsPresetValue) graphicsPresetValue.textContent = String(graphicsSettings.preset || "custom").toUpperCase();
	for (const [name, button] of [["legacy", graphicsPresetLegacy], ["normal", graphicsPresetNormal], ["pretty", graphicsPresetPretty]]) {
		if (button) button.classList.toggle("is-active", graphicsSettings.preset === name);
	}
	setToggleButtonState(graphicsShadowsToggle, graphicsSettings.shadows, graphicsShadowsValue);
	setToggleButtonState(graphicsPortalToggle, graphicsSettings.portalWindow, graphicsPortalValue);
	setToggleButtonState(graphicsParticlesToggle, graphicsSettings.particles, graphicsParticlesValue);
	setToggleButtonState(graphicsFpsToggle, graphicsSettings.fpsCounter, graphicsFpsValue);
	setToggleButtonState(graphicsDebugToggle, graphicsSettings.runtimeDebug, graphicsDebugValue);
	if (graphicsFpsCounter) graphicsFpsCounter.classList.toggle("visible", !!graphicsSettings.fpsCounter);
	if (runtimeDebugOverlay) runtimeDebugOverlay.classList.toggle("visible", !!graphicsSettings.runtimeDebug);
}

function getGraphicsPreset(name) {
	return GRAPHICS_PRESETS[String(name || "normal")] || GRAPHICS_PRESETS.normal;
}

function getClampedRenderScale(value) {
	return THREE.MathUtils.clamp(Number(value) || 1, GRAPHICS_RENDER_SCALE_MIN, GRAPHICS_RENDER_SCALE_MAX);
}

function isLegacyGraphicsMode() {
	return graphicsSettings.preset === "legacy";
}

function particlesRuntimeEnabled() {
	return !!graphicsSettings.particles;
}

function applyPortalWindowMaterialContract(plane) {
	if (!plane || !plane.material) return;
	plane.material.map = null;
	plane.material.color.setHex(0x0d1820);
	plane.material.transparent = true;
	plane.material.opacity = 0.42;
	plane.material.needsUpdate = true;
}

function applyGraphicsSettings() {
	graphicsSettings.renderScale = getClampedRenderScale(graphicsSettings.renderScale);
	updateGraphicsPanel();
	if (typeof renderer !== "undefined") {
		renderer.setPixelRatio(graphicsSettings.renderScale);
		renderer.shadowMap.enabled = !!graphicsSettings.shadows;
		renderer.shadowMap.needsUpdate = true;
	}
	if (typeof sun !== "undefined") sun.castShadow = !!graphicsSettings.shadows;
}

const audioMix = {
	musicDuckUntil: 0,
	musicDuckGain: 1,
	slurpDuckUntil: 0,
	slurpDuckGain: 1,
	restoreTimer: 0
};
const AUDIO_BUS = Object.freeze({
	combat: new Set(["phoneAttack", "negativeAck"]),
	system: new Set(["receivedMessage", "sentMessage", "paymentSuccess", "paymentFailure", "vcInvitation", "vcEnded", "lowPower", "connectPower", "endCallTone"]),
	capture: new Set(["captureSlot"])
});

function clampAudioLevel(value) {
	const n = Number(value);
	if (!Number.isFinite(n)) return 0;
	return THREE.MathUtils.clamp(n, 0, 1);
}

function getMusicLevel() {
	return audioSettings.musicMuted ? 0 : clampAudioLevel(audioSettings.music);
}

function getSfxLevel() {
	return audioSettings.sfxMuted ? 0 : clampAudioLevel(audioSettings.sfx);
}

function getAudioNowSeconds() {
	return (performance && typeof performance.now === "function") ? performance.now() / 1000 : Date.now() / 1000;
}

function getMusicDuckGain() {
	return getAudioNowSeconds() < audioMix.musicDuckUntil ? audioMix.musicDuckGain : 1;
}

function getSlurpDuckGain() {
	return getAudioNowSeconds() < audioMix.slurpDuckUntil ? audioMix.slurpDuckGain : 1;
}

function scheduleAudioMixRestore(seconds = 0.35) {
	if (audioMix.restoreTimer) clearTimeout(audioMix.restoreTimer);
	audioMix.restoreTimer = setTimeout(() => {
		audioMix.restoreTimer = 0;
		applyAudioLevels();
	}, Math.max(40, seconds * 1000 + 40));
}

function duckAudioForCue(kind) {
	// Stable mix pass: avoid rapidly rewriting music volume for every tiny SFX.
	// HTMLAudioElement volume thrashing can sound like glitches/cutouts in Chrome.
	// Keep music steady; only let important one-shots sit on top at controlled volume.
	return;
}

function releaseExpiredAudioDucks() {
	const now = getAudioNowSeconds();
	if (now >= audioMix.musicDuckUntil) audioMix.musicDuckGain = 1;
	if (now >= audioMix.slurpDuckUntil) audioMix.slurpDuckGain = 1;
}

function setMutedSliderState(kind, muted) {
	const slider = kind === "music" ? musicVolumeSlider : sfxVolumeSlider;
	const fill = kind === "music" ? musicVolumeFill : sfxVolumeFill;
	const value = kind === "music" ? musicVolumeValue : sfxVolumeValue;
	const button = kind === "music" ? musicMuteToggle : sfxMuteToggle;
	const meter = slider ? slider.closest(".sound-meter") : null;
	const stored = kind === "music" ? audioSettings.music : audioSettings.sfx;
	const percent = Math.round(clampAudioLevel(stored) * 100);
	if (slider) {
		slider.disabled = muted;
		slider.value = String(muted ? 100 : percent);
	}
	if (fill) fill.style.width = `${muted ? 100 : percent}%`;
	if (value) value.textContent = muted ? "MUTED" : `${percent}%`;
	if (meter) meter.classList.toggle("is-muted", muted);
	if (button) {
		button.classList.toggle("is-muted", muted);
		button.setAttribute("aria-pressed", muted ? "true" : "false");
		button.textContent = muted ? "ON" : "MUTE";
	}
}

function applyAudioLevels() {
	audioSettings.music = clampAudioLevel(audioSettings.music);
	audioSettings.sfx = clampAudioLevel(audioSettings.sfx);
	if (audioSettings.music > 0) audioSettings.lastMusic = audioSettings.music;
	if (audioSettings.sfx > 0) audioSettings.lastSfx = audioSettings.sfx;
	setMutedSliderState("music", audioSettings.musicMuted);
	setMutedSliderState("sfx", audioSettings.sfxMuted);
	releaseExpiredAudioDucks();
	if (gameMusic) gameMusic.volume = 0.52 * getMusicLevel() * getMusicDuckGain();
	if (gameOverMusic) gameOverMusic.volume = 0.62 * getMusicLevel();
	if (slurpRingtoneGain) slurpRingtoneGain.gain.value = 0.13 * getSfxLevel() * getSlurpDuckGain();
}


const crosshairArms = {
	top: crosshair ? crosshair.querySelector(".arm-top") : null,
	right: crosshair ? crosshair.querySelector(".arm-right") : null,
	bottom: crosshair ? crosshair.querySelector(".arm-bottom") : null,
	left: crosshair ? crosshair.querySelector(".arm-left") : null
};

const crosshairRotor = document.createElement("div");
crosshairRotor.id = "crosshair-rotor";
if (crosshair) {
	crosshair.appendChild(crosshairRotor);
	for (const arm of Object.values(crosshairArms)) {
		if (arm) crosshairRotor.appendChild(arm);
	}
}

if (crosshair) {
	crosshair.classList.add("vacuum-mode");
}
const INPUT_CONTRACT = [
	["WASD", "move"],
	["Shift", "sprint"],
	["Space", "jump / double jump"],
	["Mouse", "camera / crosshair"],
	["C", "toggle first / third person camera"],
	["Left Click", "vacuum"],
	["F", "melee combo"],
	["Q", "shoot stored soul"]
];
const CORE_RUNTIME_CONTRACT = Object.freeze({
	player: "phone",
	world: "lab",
	loop: "room -> humans -> slurpable cubes -> vacuum -> stored -> shoot into wall slots -> open door -> next room",
	truth: "runtime code owns behavior; HUD renders from contracts"
});

function renderControlsHud() {
	if (!controlsBody) return;
	const controls = INPUT_CONTRACT
		.map(([key, action]) => `<div class="control-row"><span class="control-key">${key}</span><span class="control-action">${action}</span></div>`)
		.join("");
	controlsBody.innerHTML = `
		<div class="control-section">
			<div class="control-section-title">Goal</div>
			<div class="control-section-text">Break enemies into soul cubes. Vacuum them. Shoot stored souls into the air goals. Fill every goal to open the mirror door.</div>
		</div>
		<div class="control-section">
			<div class="control-section-title">Battery</div>
			<div class="control-section-text">Battery is health and stamina. Moving, jumping, attacking, vacuuming, shooting, and enemy hits drain it. Idle restores it fast. Stored souls reduce drain.</div>
		</div>
		${controls}
	`;
}

renderControlsHud();

function hasOpenHudPanel() {
	return !!(
		(controlsPanel && !controlsPanel.classList.contains("collapsed")) ||
		(soundPanel && !soundPanel.classList.contains("collapsed")) ||
		(graphicsPanel && !graphicsPanel.classList.contains("collapsed"))
	);
}

function closeHudPanels() {
	if (controlsPanel) controlsPanel.classList.add("collapsed");
	if (soundPanel) soundPanel.classList.add("collapsed");
	if (graphicsPanel) graphicsPanel.classList.add("collapsed");
}

function enterHudPause() {
	if (!game.started || game.dead) return;
	game.uiPaused = true;
	vacuum.active = false;
	input.keys = {};
	if (document.pointerLockElement === renderer.domElement && document.exitPointerLock) {
		document.exitPointerLock();
	}
}

function exitHudPause(options = {}) {
	if (!game.started || game.dead) return;
	game.uiPaused = false;
	frame.lastTime = performance.now();
	if (options.relock) {
		try {
			renderer.domElement.requestPointerLock();
		} catch (err) {}
	}
}

function synchronizeHudPause(options = {}) {
	if (hasOpenHudPanel()) {
		enterHudPause();
	} else {
		exitHudPause({ relock: !!options.relock });
	}
}

function leaveHudPauseAndRelock() {
	if (!game.started || game.dead) return;
	closeHudPanels();
	exitHudPause({ relock: true });
}

function toggleHudPanel(panelToToggle, panelsToClose = []) {
	if (!panelToToggle) return;
	const willOpen = panelToToggle.classList.contains("collapsed");
	panelToToggle.classList.toggle("collapsed");
	for (const panel of panelsToClose) {
		if (willOpen && panel) panel.classList.add("collapsed");
	}
	synchronizeHudPause({ relock: true });
}

if (controlsToggle && controlsPanel) {
	controlsToggle.addEventListener("click", function (event) {
		event.stopPropagation();
		toggleHudPanel(controlsPanel, [soundPanel, graphicsPanel]);
	});
}

if (soundToggle && soundPanel) {
	soundToggle.addEventListener("click", function (event) {
		event.stopPropagation();
		toggleHudPanel(soundPanel, [controlsPanel, graphicsPanel]);
	});
}

if (graphicsToggle && graphicsPanel) {
	graphicsToggle.addEventListener("click", function (event) {
		event.stopPropagation();
		toggleHudPanel(graphicsPanel, [controlsPanel, soundPanel]);
	});
}
for (const hudPanel of [controlsPanel, soundPanel, graphicsPanel]) {
	if (hudPanel) {
		hudPanel.addEventListener("mousedown", function (event) { event.stopPropagation(); });
		hudPanel.addEventListener("mouseup", function (event) { event.stopPropagation(); });
		hudPanel.addEventListener("click", function (event) { event.stopPropagation(); });
	}
}

function applyGraphicsPreset(name) {
	const presetName = GRAPHICS_PRESETS[String(name || "normal")] ? String(name || "normal") : "normal";
	const preset = getGraphicsPreset(presetName);
	graphicsSettings.preset = presetName;
	graphicsSettings.renderScale = preset.renderScale;
	graphicsSettings.shadows = preset.shadows;
	graphicsSettings.portalWindow = preset.portalWindow;
	graphicsSettings.particles = preset.particles;
	applyGraphicsSettings();
}
if (graphicsPresetLegacy) graphicsPresetLegacy.addEventListener("click", function (event) { event.stopPropagation(); applyGraphicsPreset("legacy"); });
if (graphicsPresetNormal) graphicsPresetNormal.addEventListener("click", function (event) { event.stopPropagation(); applyGraphicsPreset("normal"); });
if (graphicsPresetPretty) graphicsPresetPretty.addEventListener("click", function (event) { event.stopPropagation(); applyGraphicsPreset("pretty"); });
if (graphicsScaleSlider) {
	graphicsScaleSlider.addEventListener("input", function () {
		graphicsSettings.renderScale = getClampedRenderScale(Number(graphicsScaleSlider.value) / 100);
		graphicsSettings.preset = "custom";
		applyGraphicsSettings();
	});
}
if (graphicsShadowsToggle) {
	graphicsShadowsToggle.addEventListener("click", function (event) {
		event.stopPropagation();
		graphicsSettings.shadows = !graphicsSettings.shadows;
		graphicsSettings.preset = "custom";
		applyGraphicsSettings();
	});
}
if (graphicsPortalToggle) {
	graphicsPortalToggle.addEventListener("click", function (event) {
		event.stopPropagation();
		graphicsSettings.portalWindow = !graphicsSettings.portalWindow;
		graphicsSettings.preset = "custom";
		applyGraphicsSettings();
	});
}
if (graphicsParticlesToggle) {
	graphicsParticlesToggle.addEventListener("click", function (event) {
		event.stopPropagation();
		graphicsSettings.particles = !graphicsSettings.particles;
		graphicsSettings.preset = "custom";
		applyGraphicsSettings();
	});
}
if (graphicsFpsToggle) {
	graphicsFpsToggle.addEventListener("click", function (event) {
		event.stopPropagation();
		graphicsSettings.fpsCounter = !graphicsSettings.fpsCounter;
		applyGraphicsSettings();
	});
}
if (graphicsDebugToggle) {
	graphicsDebugToggle.addEventListener("click", function (event) {
		event.stopPropagation();
		graphicsSettings.runtimeDebug = !graphicsSettings.runtimeDebug;
		applyGraphicsSettings();
	});
}
updateGraphicsPanel();
if (musicVolumeSlider) {
	musicVolumeSlider.addEventListener("input", function () {
		audioSettings.music = Number(musicVolumeSlider.value) / 100;
		if (audioSettings.music > 0) audioSettings.lastMusic = audioSettings.music;
		applyAudioLevels();
	});
}
if (sfxVolumeSlider) {
	sfxVolumeSlider.addEventListener("input", function () {
		audioSettings.sfx = Number(sfxVolumeSlider.value) / 100;
		if (audioSettings.sfx > 0) audioSettings.lastSfx = audioSettings.sfx;
		applyAudioLevels();
	});
}
if (musicMuteToggle) {
	musicMuteToggle.addEventListener("click", function (event) {
		event.stopPropagation();
		audioSettings.musicMuted = !audioSettings.musicMuted;
		if (!audioSettings.musicMuted && audioSettings.music <= 0) audioSettings.music = audioSettings.lastMusic || 0.70;
		applyAudioLevels();
	});
}
if (sfxMuteToggle) {
	sfxMuteToggle.addEventListener("click", function (event) {
		event.stopPropagation();
		audioSettings.sfxMuted = !audioSettings.sfxMuted;
		if (!audioSettings.sfxMuted && audioSettings.sfx <= 0) audioSettings.sfx = audioSettings.lastSfx || 0.65;
		applyAudioLevels();
	});
}


const game = {
	mode: "vacuum",
	started: false,
	dead: false,
	uiPaused: false,
	screenFlashTime: -9999
};
const depositZone = new THREE.Vector3(0, 0, 0);
const depositRadius = 5;


const TARGET_COUNT = 32;
const TARGET_STRIDE = 26;
const T_X = 0;
const T_Y = 1;
const T_Z = 2;
const T_FLOAT = 3;
const T_SPIN = 4;
const T_ALIVE = 5;
const T_SCALE = 6;
const T_HITFLASH = 7;
const T_CHAR = 8;
const T_RENDER_Y = 9;
const T_HEALTH = 10;
const T_VX = 11;
const T_VY = 12;
const T_VZ = 13;
const T_KIND = 14;
const T_ARMOR = 15;
const T_SLURPABLE = 16;
const T_WALK_TX = 17;
const T_WALK_TZ = 18;
const T_WALK_YAW = 19;
const T_WALK_PHASE = 20;
const T_WALK_FACE = 21;
const T_ATTACK_TIMER = 22;
const T_ATTACK_COOLDOWN = 23;
const T_ATTACK_VARIANT = 24;
const T_ATTACK_HIT = 25;

const PARTICLE_COUNT = 256;
const PARTICLE_STRIDE = 8;
const P_X = 0;
const P_Y = 1;
const P_Z = 2;
const P_VX = 3;
const P_VY = 4;
const P_VZ = 5;
const P_LIFE = 6;
const P_MAXLIFE = 7;

const GRASS_COUNT = 2000;

const phoneCharacter = {
	movement: {
		walkAccel: 16,
		runAccel: 42,
		walkMaxSpeed: 18.0,
		runMaxSpeed: 42.0,
		friction: 0.88,
		gravity: 14
	},
	jump: {
		groundVelocity: 4.5,
		airVelocity: 4.25,
		airJumps: 1
	}
};

const PARKOUR = {
	coyoteTime: 0.12,
	jumpBuffer: 0.12,
	airAccelMultiplier: 0.62,
	airMaxSpeedMultiplier: 1.08,
	airFriction: 0.985,
	wallSlideRetention: 0.94,
	landingMomentumBoost: 1.04,
	wallClimbSpeed: 3.15,
	wallClimbGrip: 0.64,
	wallClimbMaxHeight: 1.25,
	wallClimbPushDot: -0.18,
	ceilingClearance: 0.42,
	playerCeilingBodyClearance: 0.42
};

const cursor = {
	yaw: 0,
	pitch: 0,
	sensitivity: 0.003,
	maxPitch: Math.PI * 0.48
};

const CAMERA_MODE_THIRD = "third";
const CAMERA_MODE_FIRST = "first";
const cameraMode = { current: CAMERA_MODE_THIRD };

const input = { keys: {} };

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x081018);
scene.fog = new THREE.FogExp2(0x081018, 0.026);

const camera = new THREE.PerspectiveCamera(75, innerWidth / innerHeight, 0.1, 1000);

const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setSize(innerWidth, innerHeight);
renderer.setPixelRatio(getClampedRenderScale(graphicsSettings.renderScale));
renderer.shadowMap.enabled = !!graphicsSettings.shadows;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;

const clock = new THREE.Clock();
const frame = { time: 0, now: performance.now(), dt: 0 };
function pulseTime() { return frame.now || performance.now(); }
function flashScreen() { game.screenFlashTime = pulseTime(); }

scene.add(new THREE.HemisphereLight(0xbfe9ff, 0x315c38, 1.1));

const SUN_BASE_POSITION = new THREE.Vector3(30, 60, 25);
const SUN_TARGET_BASE = new THREE.Vector3(0, 1.0, 0);
const sunTarget = new THREE.Object3D();
sunTarget.position.copy(SUN_TARGET_BASE);
scene.add(sunTarget);

const sun = new THREE.DirectionalLight(0xffffff, 1.0);
sun.position.copy(SUN_BASE_POSITION);
sun.target = sunTarget;
sun.castShadow = true;
sun.shadow.mapSize.set(2048, 2048);
sun.shadow.camera.left = -95;
sun.shadow.camera.right = 95;
sun.shadow.camera.top = 95;
sun.shadow.camera.bottom = -95;
sun.shadow.camera.near = 1;
sun.shadow.camera.far = 185;
sun.shadow.bias = -0.00018;
scene.add(sun);
applyGraphicsSettings();

const fill = new THREE.DirectionalLight(0x90caf9, 0.35);
fill.position.set(-20, 25, -30);
scene.add(fill);

const floor = new THREE.Mesh(
	new THREE.PlaneGeometry(120, 120),
	new THREE.MeshStandardMaterial({ color: 0x3f7448, roughness: 0.85 })
);
floor.rotation.x = -Math.PI / 2;
floor.receiveShadow = true;
floor.visible = false;
scene.add(floor);


const PHONE_HEIGHT = 0.16;
const LAB_FLOOR_TOP_Y = 0;
const GROUND_Y = LAB_FLOOR_TOP_Y + PHONE_HEIGHT / 2;

const vacuum = {
	active: false,
	power: 0,
	pose: 0,
	fieldStrength: 0,
	coneTightness: 0
};
const VACUUM_CHARGE_SPEED = 3.5;
const VACUUM_DECAY_SPEED = 6.0;
const VACUUM_DAMAGE = 0.28;
const VACUUM_MOVE_MULT = 0.35;

const PROX_ATTACK_RANGE = 2.85;
const PROX_ATTACK_CONE_DOT = 0.10;
const PROX_ATTACK_DAMAGE = 1.0;
const PROX_ATTACK_COOLDOWN = 0.34;
const PROX_ATTACK_VISUAL_DURATION = 0.26;
const PROX_ATTACK_DASH_DURATION = 0.20;
const PROX_ATTACK_DASH_SPEED = 13.5;
const PROX_ATTACK_HIT_RADIUS = 0.88;
const PROX_ATTACK_LUNGE_DISTANCE = 0.22;
const PROX_ATTACK_RECOIL_DISTANCE = 0.16;
const PROX_ATTACK_RECOIL_SPEED = 2.2;
const PROX_ATTACK_VARIANTS = [
	{ name: "front-right-corner", side: 1, roll: -0.72, yaw: 0.62, pitch: -0.32, lift: 0.012 },
	{ name: "front-left-corner", side: -1, roll: 0.72, yaw: -0.62, pitch: -0.32, lift: 0.012 },
	{ name: "low-right-kick", side: 1, roll: -0.42, yaw: 0.42, pitch: 0.42, lift: -0.006 },
	{ name: "low-left-kick", side: -1, roll: 0.42, yaw: -0.42, pitch: 0.42, lift: -0.006 }
];
const PROX_ATTACK_COMBO_WINDOW = 720;
const PROX_ATTACK_COMBOS = [
	{ name: "jab", variant: 0, range: 2.35, damage: 0.82, hitRadius: 0.78, visual: 0.20, dash: 0.13, dashSpeed: 12.5, cooldown: 0.22, recoilDistance: 0.08, recoilSpeed: 1.25, lunge: 0.15, cost: 2.8 },
	{ name: "cross", variant: 1, range: 2.85, damage: 1.08, hitRadius: 0.90, visual: 0.25, dash: 0.18, dashSpeed: 14.0, cooldown: 0.27, recoilDistance: 0.12, recoilSpeed: 1.75, lunge: 0.22, cost: 3.6 },
	{ name: "low-kick", variant: 2, range: 3.18, damage: 1.48, hitRadius: 1.02, visual: 0.31, dash: 0.23, dashSpeed: 15.2, cooldown: 0.38, recoilDistance: 0.15, recoilSpeed: 2.1, lunge: 0.29, cost: 5.0 },
	{ name: "reverse-kick", variant: 3, range: 3.00, damage: 1.22, hitRadius: 0.96, visual: 0.29, dash: 0.20, dashSpeed: 13.8, cooldown: 0.34, recoilDistance: 0.12, recoilSpeed: 1.8, lunge: 0.25, cost: 4.2 }
];
function getProximityAttackCombo() {
	return PROX_ATTACK_COMBOS[proximityAttack.comboIndex] || PROX_ATTACK_COMBOS[0];
}
const SOUL_ARMOR_NORMAL = 2.0;
const SOUL_ARMOR_BRUTE = 4.0;
const HUMAN_WALK_SPEED = 0.72;
const HUMAN_WALK_TARGET_RADIUS = 0.55;
const HUMAN_GROUND_Y = 0.65;
const HUMAN_VISUAL_FOOT_Y = LAB_FLOOR_TOP_Y;
const HUMAN_WALK_RANGE = 5.5;
const HUMAN_ATTACK_NOTICE_RANGE = 5.6;
const HUMAN_ATTACK_START_RANGE = 1.55;
const HUMAN_ATTACK_HIT_RANGE = 1.85;
const HUMAN_ATTACK_DURATION = 0.48;
const HUMAN_ATTACK_ACTIVE_TIME = 0.26;
const HUMAN_ATTACK_COOLDOWN = 1.15;
const HUMAN_ATTACK_KNOCKBACK = 3.0;
const HUMAN_ATTACK_PLAYER_FLASH = 0.22;
const proximityAttack = {
	cooldown: 0,
	pose: 0,
	visualTimer: 0,
	visualDuration: PROX_ATTACK_VISUAL_DURATION,
	dashTimer: 0,
	travel: 0,
	visualHit: false,
	variant: 0,
	comboIndex: 0,
	lastTriggerTime: -9999
};
const proximityAttackHitTargets = new Set();
const INGEST_DISTANCE = 0.45;
const CAPTURE_DRAW_START = 0.25;
const CAPTURE_COMMIT_START = 0.70;
const CAPTURE_DECAY_FAST = 0.75;
const CAPTURE_DECAY_SLOW = 0.28;

const PHONE_CAPACITY = 30;
const ROOM_REQUIRED_SOULS = 5;
const ACTIVE_HUMAN_TARGET = 5;
const ACTIVE_HUMAN_TARGET_CAP = 20;
const HUMAN_RESPAWN_DELAY_MIN = 1.45;
const HUMAN_RESPAWN_DELAY_MAX = 2.35;
const HUMAN_RESPAWN_MIN_PLAYER_DIST = 8.0;
const HUMAN_RESPAWN_MIN_OLD_SOUL_DIST = 6.0;
const phoneStorage = {
	storedCubes: [],
	isFull: false,
	lastCapturePulseTime: -9999,
	lastDischargePulseTime: -9999
};
const shootState = {
	impulseTime: -9999,
	cursorJoinTime: -9999
};
const battery = {
	value: 100,
	max: 100,
	lastCostTime: -9999,
	lastGainTime: -9999,
	activity: "IDLE",
	comboHits: 0,
	comboMultiplier: 1,
	lastComboHitTime: -9999,
	lastEventTime: -9999,
	lastEventReason: ""
};
const supplementalBattery = {
	active: false,
	value: 0,
	max: 85
};
const activePowerupStacks = {
	flower: 0
};
const flowerPowerups = [];
let flowerPowerupPrototype = null;
const flowerPowerupPool = [];
const flowerPowerupMirrorPool = [];
const FLOWER_POWERUP_POOL_TARGET = 10;
let flowerPowerupPoolWarmupQueued = false;
const POWERUP_STOCK_BASE_MAX = 85;
const POWERUP_STOCK_PER_STACK = 32;
const FLOWER_PICKUP_VALUE = 46;
const FLOWER_DROP_CHANCE = 0.26;
const FLOWER_PICKUP_RADIUS = 1.05;
const FLOWER_BOB_HEIGHT = 0.16;
const FLOWER_POWERUP_GROUND_Y = 0.38;
const FLOWER_DROP_SEPARATION_RADIUS = 1.15;
const FLOWER_DROP_MIN_SPACING = 1.05;
const FLOWER_ATTACK_FEED = 2.8;
const FLOWER_SLURP_FEED = 9.0;
const BATTERY_IDLE_REGEN = 22.0;
const BATTERY_ACTIVE_REGEN = 3.0;
const BATTERY_WALK_DRAIN = 0.45;
const BATTERY_SPRINT_DRAIN = 3.0;
const BATTERY_AIR_DRAIN = 0.9;
const BATTERY_WALL_CLIMB_DRAIN = 4.2;
const BATTERY_VACUUM_DRAIN = 1.35;
const BATTERY_MELEE_COST = 4.0;
const BATTERY_JUMP_COST = 3.0;
const BATTERY_DOUBLE_JUMP_COST = 6.0;
const BATTERY_SHOOT_COST = 7.0;
const BATTERY_ENEMY_HIT_COST = 26.0;
const BATTERY_CAPTURE_GAIN = 18.0;
const BATTERY_SOUL_EFFICIENCY = 0.16;
const BATTERY_MELEE_HIT_GAIN = 12.0;
const BATTERY_COMBO_GROWTH = 1.22;
const BATTERY_COMBO_TIMEOUT = 1800;
const MULTI_HIT_BONUS_GROWTH = 1.12;
const MULTI_HIT_BATTERY_BONUS = 4.5;
const CHAIN_FLAME_COUNT = 36;
const chainFxState = {
	lastMultiHitTime: -9999,
	lastMultiHitCount: 0
};
const energyImpact = {
	plus: 0,
	minus: 0,
	mult: 1,
	active: "",
	lastTime: -9999
};


function formatBatteryNumber(value) {
	const abs = Math.abs(Number(value) || 0);
	const formatted = abs >= 10 ? String(Math.round(abs)) : abs.toFixed(1);
	return formatted.replace(/\.0$/, "");
}

function ensureEnergySymbolStrip() {
	// Sequential ticker is now the only battery behavior display.
}

function updateEnergySymbolStrip() {
	// Sequential ticker is now the only battery behavior display.
}

function setEnergyImpact(kind, value) {
	// Sequential ticker is now the only battery behavior display.
}


function batteryReasonLabel(reason) {
	return String(reason || "compute")
		.replace("IDLE RECHARGE", "idle")
		.replace("SOUL INGESTED", "ingest")
		.replace("DOUBLE JUMP", "jump")
		.replace("NEXT ROOM", "room")
		.replace("COMPUTE", "compute")
		.replace("RECHARGE", "recharge")
		.replace("RECOVER", "recover")
		.replace("SPRINT", "sprint")
		.replace("VACUUM", "vacuum")
		.replace("MOVE", "move")
		.replace("MELEE", "melee")
		.replace("HIT", "hit")
		.replace("CHAIN", "chain")
		.replace("COMBO", "combo")
		.replace("DISCHARGE", "shoot")
		.replace("JUMP", "jump")
		.replace("IDLE", "idle")
		.toLowerCase();
}

function pushBatteryEvent(text, type = "gain") {
	if (!batteryEvents || !game.started || game.dead) return;
	const raw = String(text || "").trim();
	let label = raw;
	if (!label) label = type === "cost" ? "drain" : "gain";
	label = label
		.replace("DISCHARGE", "shoot")
		.replace("SOUL INGESTED", "ingest")
		.replace("NEXT ROOM", "room")
		.replace("DOUBLE JUMP", "jump")
		.replace("MELEE", "melee")
		.replace("CHAIN", "chain")
		.replace("COMBO", "combo")
		.replace("HIT", "hit")
		.replace("JUMP", "jump")
		.replace("CLIMB", "climb")
		.toLowerCase();

	batteryEvents.innerHTML = "";
	const node = document.createElement("div");
	node.className = "energy-ticker " + type;
	node.textContent = label;
	batteryEvents.appendChild(node);
}

function resetBatteryCombo() {
	battery.comboHits = 0;
	battery.comboMultiplier = 1;
	battery.lastComboHitTime = -9999;
}

function registerMeleeBatteryHit(hitCount = 1) {
	const clampedHits = Math.max(1, hitCount | 0);
	const multiBonus = clampedHits > 1 ? (clampedHits - 1) * MULTI_HIT_BATTERY_BONUS : 0;
	const base = BATTERY_MELEE_HIT_GAIN + multiBonus;
	const multiplierBefore = battery.comboMultiplier;
	const gain = base * multiplierBefore;
	battery.comboHits += clampedHits;
	battery.lastComboHitTime = pulseTime();
	gainBattery(gain, clampedHits > 1 ? "CHAIN" : "COMBO");
	pushBatteryEvent(
		"+" + formatBatteryNumber(gain) + " ×" + multiplierBefore.toFixed(multiplierBefore >= 10 ? 1 : 2),
		"combo"
	);
	battery.comboMultiplier *= BATTERY_COMBO_GROWTH * Math.pow(MULTI_HIT_BONUS_GROWTH, Math.max(0, clampedHits - 1));
	if (clampedHits > 1) showComboPop(clampedHits);
}
function batteryComboActive() {
	return battery.comboHits > 0 && pulseTime() - battery.lastComboHitTime <= BATTERY_COMBO_TIMEOUT;
}

function batteryDrainMultiplier() {
	return 1 / (1 + getStoredCount() * BATTERY_SOUL_EFFICIENCY);
}

function getTotalPowerupStacks() {
	return Object.values(activePowerupStacks).reduce((sum, value) => sum + Math.max(0, value | 0), 0);
}

function getPowerupStockMax() {
	const stacks = getTotalPowerupStacks();
	return stacks > 0 ? POWERUP_STOCK_BASE_MAX + Math.max(0, stacks - 1) * POWERUP_STOCK_PER_STACK : POWERUP_STOCK_BASE_MAX;
}

function clearActivePowerups() {
	for (const key of Object.keys(activePowerupStacks)) activePowerupStacks[key] = 0;
	supplementalBattery.active = false;
	supplementalBattery.value = 0;
	supplementalBattery.max = POWERUP_STOCK_BASE_MAX;
}

function updateSupplementalBatteryHud() {
	if (!itemHud || !itemFill) return;
	const stacks = getTotalPowerupStacks();
	const fill = supplementalBattery.active && stacks > 0 ? THREE.MathUtils.clamp(supplementalBattery.value / supplementalBattery.max, 0, 1) : 0;
	itemFill.style.transform = `scaleX(${fill})`;
	itemHud.classList.toggle("empty", fill <= 0.001);
	if (itemLabel) itemLabel.textContent = fill > 0.001 ? `POWER ×${stacks}` : "POWER EMPTY";
}

function addPowerupStack(type = "flower", amount = FLOWER_PICKUP_VALUE) {
	if (!activePowerupStacks[type]) activePowerupStacks[type] = 0;
	activePowerupStacks[type] += 1;
	supplementalBattery.max = getPowerupStockMax();
	supplementalBattery.value = THREE.MathUtils.clamp(supplementalBattery.value + Math.max(0, amount), 0, supplementalBattery.max);
	supplementalBattery.active = supplementalBattery.value > 0;
	phoneStorage.lastCapturePulseTime = pulseTime();
	updateSupplementalBatteryHud();
}

function activateSupplementalBattery(amount = FLOWER_PICKUP_VALUE) {
	addPowerupStack("flower", amount);
}

function feedSupplementalBattery(amount = 0, reason = "FEED") {
	if (!supplementalBattery.active || supplementalBattery.max <= 0) return false;
	const before = supplementalBattery.value;
	supplementalBattery.value = THREE.MathUtils.clamp(supplementalBattery.value + Math.max(0, amount), 0, supplementalBattery.max);
	supplementalBattery.active = supplementalBattery.value > 0;
	if (supplementalBattery.value > before + 0.001) {
		phoneStorage.lastCapturePulseTime = pulseTime();
		updateSupplementalBatteryHud();
		return true;
	}
	updateSupplementalBatteryHud();
	return false;
}

function consumeSupplementalBattery(cost) {
	if (!supplementalBattery.active || supplementalBattery.value <= 0 || cost <= 0) return cost;
	const absorbed = Math.min(cost, supplementalBattery.value);
	supplementalBattery.value = Math.max(0, supplementalBattery.value - absorbed);
	if (supplementalBattery.value <= 0.001) {
		clearActivePowerups();
	}
	updateSupplementalBatteryHud();
	return Math.max(0, cost - absorbed);
}

function spendBattery(amount, reason = "COMPUTE") {
	if (game.dead) return false;
	const multiplier = batteryDrainMultiplier();
	const cost = amount * multiplier;
	const before = battery.value;
	const remainingCost = consumeSupplementalBattery(cost);
	battery.value = THREE.MathUtils.clamp(battery.value - remainingCost, 0, battery.max);
	battery.lastCostTime = pulseTime();
	battery.activity = reason;
	const discreteCost =
		reason === "MELEE" ||
		reason === "DISCHARGE" ||
		reason === "HIT" ||
		reason === "JUMP" ||
		reason === "DOUBLE JUMP" ||
		reason === "CLIMB";
	if (discreteCost && before > battery.value) {
		pushBatteryEvent("-" + formatBatteryNumber(before - battery.value) + " " + batteryReasonLabel(reason), "cost");
	}
	if (discreteCost && remainingCost <= 0 && cost > 0) {
		pushBatteryEvent("flower " + batteryReasonLabel(reason), "gain");
	}
	if (reason === "HIT" && (before > battery.value || cost > remainingCost)) {
		playDamageAckSound();
	}
	updateBatteryEventSounds(before);
	if (battery.value <= 0) {
		battery.value = 0;
		triggerRunDeath(reason);
		return false;
	}
	return true;
}
function gainBattery(amount, reason = "RECHARGE") {
	if (game.dead) return;
	const before = battery.value;
	battery.value = THREE.MathUtils.clamp(battery.value + amount, 0, battery.max);
	battery.lastGainTime = pulseTime();
	battery.activity = reason;
	const delta = battery.value - before;
	const importantGain =
		reason === "COMBO" ||
		reason === "CHAIN" ||
		reason === "SOUL INGESTED" ||
		reason === "NEXT ROOM";
	if (importantGain && (delta > 0 || amount > 0)) {
		pushBatteryEvent("+" + formatBatteryNumber(delta > 0 ? delta : amount) + " " + batteryReasonLabel(reason), reason === "COMBO" || reason === "CHAIN" ? "combo" : "gain");
	}
	updateBatteryEventSounds(before);
}
function batteryPower() {
	return THREE.MathUtils.clamp(battery.value / 18, 0.35, 1);
}

function updateBatteryHud() {
	if (!batteryHud || !batteryFill || !batteryNote) return;
	const fill = THREE.MathUtils.clamp(battery.value / battery.max, 0, 1);
	batteryFill.style.transform = `scaleX(${fill})`;
	updateSupplementalBatteryHud();
	batteryHud.classList.toggle("low", battery.value < 24);
	batteryHud.classList.toggle("chain", batteryComboActive());
	if (game.dead) {
		batteryNote.textContent = "DEAD";
		return;
	}
	if (batteryComboActive()) {
		const mult = battery.comboMultiplier.toFixed(battery.comboMultiplier >= 10 ? 1 : 2);
		batteryNote.textContent = "×" + mult;
	} else {
		batteryNote.textContent = "";
	}
}
function updateBattery(dt) {
	if (!game.started || game.dead) return;
	if (battery.comboHits > 0 && !batteryComboActive()) resetBatteryCombo();
	if (battery.value <= 0) {
		triggerRunDeath("EMPTY");
		return;
	}
	const axes = getMoveAxes();
	const moving = Math.abs(axes.forward) + Math.abs(axes.strafe) > 0;
	const running = moving && isRunningInput();
	let drain = 0;
	let active = false;
	if (moving) {
		drain += running ? BATTERY_SPRINT_DRAIN : BATTERY_WALK_DRAIN;
		active = true;
	}
	if (!player.grounded) {
		drain += BATTERY_AIR_DRAIN;
		active = true;
	}
	if (vacuum.active) {
		drain += BATTERY_VACUUM_DRAIN * Math.max(0.35, vacuum.power);
		active = true;
	}
	if (proximityAttack.visualTimer > 0 || actionPose.dischargeTimer > 0) active = true;
	if (drain > 0) spendBattery(drain * dt, running ? "SPRINT" : (vacuum.active ? "VACUUM" : "MOVE"));
	else gainBattery((active ? BATTERY_ACTIVE_REGEN : BATTERY_IDLE_REGEN) * dt, active ? "RECOVER" : "IDLE");
	if (battery.value <= 0) triggerRunDeath("EMPTY");
	updateBatteryHud();
}

const actionPose = {
	discharge: 0,
	dischargeTimer: 0,
	doubleJumpTimer: 0,
	doubleJump: 0,
	doubleJumpFlipYaw: 0,
	doubleJumpVacuumPause: 0,
	screenForwardTurn: 0
};
const DISCHARGE_POSE_HOLD = 0.34;
const DOUBLE_JUMP_POSE_HOLD = 0.30;
const pendingDischargedSouls = [];
const roomHumanRespawnQueue = [];
const DISCHARGE_POSE_RELEASE_DELAY = 0.09;
const DISCHARGE_LAUNCH_SPEED = 25.0;
const DISCHARGE_BRUTE_LAUNCH_SPEED = 20.0;
const DISCHARGE_VERTICAL_LIFT = 1.15;
const DISCHARGE_MAX_UP_AIM = 0.30;
const DISCHARGE_MAX_DOWN_AIM = -0.10;
const DISCHARGE_GRAVITY = 11.5;
const DISCHARGE_AIR_DRAG_PER_SECOND = 0.72;
const DISCHARGE_LOST_Y = -3.5;
const DISCHARGE_MAX_LIFETIME = 3.25;

function getStoredCount() {
	return phoneStorage.storedCubes.length;
}

function syncStoredMirror() {
	phoneStorage.isFull = getStoredCount() >= PHONE_CAPACITY;
}

function updateStoredSoulDisplays() {
	updateHumanHud();
	updateSoulWindowHud();
}

function storeCubeFromTarget(i) {
	const idx = targetIndex(i);
	if (soulDepositShot[i]) return false;
	if (!isTargetSlurpable(idx)) return false;
	if (phoneStorage.storedCubes.length >= PHONE_CAPACITY) return false;
	const kind = getTargetKind(i);
	phoneStorage.storedCubes.push({
		kind,
		scale: kind === 1 ? 1.7 : 1,
		value: kind === 1 ? 3 : 1
	});
	syncStoredMirror();
	updateStoredSoulDisplays();
	return true;
}
function getTargetKind(i) {
	const idx = targetIndex(i);
	return targetData[idx + T_KIND] || 0;
}

function completeTargetCapture(i, x, y, z) {
	if (soulCaptureCommitted[i]) return;
	const idx = targetIndex(i);
	if (soulDepositShot[i]) return;
	if (!isTargetSlurpable(idx)) return;
	if (phoneStorage.storedCubes.length >= PHONE_CAPACITY) {
		phoneStorage.isFull = true;
		updateStoredSoulDisplays();
		return;
	}
	soulCaptureCommitted[i] = 1;
	if (!storeCubeFromTarget(i)) {
		soulCaptureCommitted[i] = 0;
		return;
	}
	phoneStorage.lastCapturePulseTime = pulseTime();
	gainBattery(BATTERY_CAPTURE_GAIN, "SOUL INGESTED");
	feedSupplementalBattery(FLOWER_SLURP_FEED, "SLURP");
	playEventSound("receivedMessage", 0.58);
	updateStoredSoulDisplays();

	const capturedIndex = Math.min(getStoredCount() - 1, humanTiles.length - 1);
	if (capturedIndex >= 0) {
		const tile = humanTiles[capturedIndex];
		tile.classList.remove("capture-pulse");
		void tile.offsetWidth;
		tile.classList.add("capture-pulse");
	}

	spawnRoomParticleBurst(x, y, z);
	syncStoredMirror();
	clearTargetSlot(i);
	queueHumanRespawnFromSoul(i, x, y, z);
	targetLock.target = -1;
	targetLock.strength = 0;
	shootState.impulseTime = -9999;
	shootState.cursorJoinTime = -9999;
}

const humanHud = document.getElementById("human-hud");
const humanTiles = [];
for (let i = 0; i < PHONE_CAPACITY; i++) {
	const tile = document.createElement("div");
	tile.className = "human-tile";
	humanHud.appendChild(tile);
	humanTiles.push(tile);
}
function updateHumanHud() {
	for (let i = 0; i < PHONE_CAPACITY; i++) {
		if (i < getStoredCount()) {
			humanTiles[i].className = "human-tile filled";
		} else {
			humanTiles[i].className = "human-tile";
		}
	}
}
updateHumanHud();

const SOUL_HUD_VISUAL_SLOTS = 18;
let soulHudLastRenderedCount = -1;
const soulHudInertia = { x: 0, y: 0 };

function ensureSoulWindowHud() {
	if (!soulOrbit) return;
	if (soulOrbit.children.length === SOUL_HUD_VISUAL_SLOTS) return;
	soulOrbit.innerHTML = "";
	for (let i = 0; i < SOUL_HUD_VISUAL_SLOTS; i++) {
		const ghost = document.createElement("div");
		ghost.className = "soul-ghost";
		const angle = (i / SOUL_HUD_VISUAL_SLOTS) * Math.PI * 2;
		const ring = 0.38 + ((i * 7) % 10) / 26;
		const rx = 34 * ring;
		const ry = 14 * ring;
		const sx = Math.cos(angle) * rx + (((i * 13) % 7) - 3);
		const sy = Math.sin(angle) * ry + (((i * 17) % 5) - 2);
		const dx = (((i * 11) % 9) - 4) * 0.9;
		const dy = (((i * 19) % 7) - 3) * 0.8;
		ghost.style.setProperty("--sx", `${sx.toFixed(1)}px`);
		ghost.style.setProperty("--sy", `${sy.toFixed(1)}px`);
		ghost.style.setProperty("--dx", `${dx.toFixed(1)}px`);
		ghost.style.setProperty("--dy", `${dy.toFixed(1)}px`);
		ghost.style.setProperty("--dur", `${(2.1 + (i % 6) * 0.24).toFixed(2)}s`);
		ghost.style.setProperty("--delay", `${(-i * 0.13).toFixed(2)}s`);
		soulOrbit.appendChild(ghost);
	}
	soulHudLastRenderedCount = -1;
}

function updateSoulWindowHud() {
	if (!soulWindow || !soulOrbit) return;
	ensureSoulWindowHud();
	const count = getStoredCount();
	if (count === soulHudLastRenderedCount) return;
	soulHudLastRenderedCount = count;
	soulWindow.classList.toggle("empty", count <= 0);
	if (soulWindowTitle) soulWindowTitle.textContent = `SOULS ${count}`;
	const filledSlots = count <= 0 ? 0 : Math.max(1, Math.ceil((count / PHONE_CAPACITY) * SOUL_HUD_VISUAL_SLOTS));
	for (let i = 0; i < soulOrbit.children.length; i++) {
		const ghost = soulOrbit.children[i];
		ghost.style.display = "block";
		ghost.classList.toggle("filled", i < filledSlots);
	}
}

function updateSoulWindowMotion() {
	if (!soulWindow) return;
	const now = frame.now;
	const captureActive = now - phoneStorage.lastCapturePulseTime < 180;
	const dischargeActive = now - phoneStorage.lastDischargePulseTime < 160;
	soulWindow.classList.toggle("capture-pulse", captureActive);
	soulWindow.classList.toggle("discharge-pulse", dischargeActive);

	// Make the HUD souls feel like loose pixels inside the phone: they lag opposite
	// player motion using one cheap CSS transform on the dot container.
	let lateral = player && player.vel ? player.vel.x : 0;
	let forward = player && player.vel ? player.vel.z : 0;
	try {
		if (typeof vRight !== "undefined" && typeof vForward !== "undefined" && player && player.vel) {
			lateral = player.vel.dot(vRight);
			forward = player.vel.dot(vForward);
		}
	} catch (err) {}
	const vertical = player && Number.isFinite(player.jumpVel) ? player.jumpVel : 0;
	const targetX = THREE.MathUtils.clamp(-lateral * 1.35, -7, 7);
	const targetY = THREE.MathUtils.clamp(forward * 0.75 - vertical * 0.12, -5, 5);
	const follow = 1 - Math.pow(0.001, Math.min(frame.dt || 0.016, 0.05));
	soulHudInertia.x += (targetX - soulHudInertia.x) * follow;
	soulHudInertia.y += (targetY - soulHudInertia.y) * follow;
	soulWindow.style.setProperty("--soul-inertia-x", `${soulHudInertia.x.toFixed(1)}px`);
	soulWindow.style.setProperty("--soul-inertia-y", `${soulHudInertia.y.toFixed(1)}px`);
}

updateStoredSoulDisplays();
const targetLock = {
	target: -1,
	strength: 0
};
const player = {
	pos: new THREE.Vector3(0, GROUND_Y, 0),
	vel: new THREE.Vector3(0, 0, 0),
	grounded: true,
	jumpVel: 0,
	airJumpsRemaining: phoneCharacter.jump.airJumps,
	coyoteTimer: 0,
	jumpBufferTimer: 0
};

const targetData = new Float32Array(TARGET_COUNT * TARGET_STRIDE);
const particleData = new Float32Array(PARTICLE_COUNT * PARTICLE_STRIDE);
const particleState = {
	next: 0
};

const raycaster = new THREE.Raycaster();
const screenCenter = new THREE.Vector2(0, 0);

const ROOM_WIDTH = 30;
const ROOM_DEPTH = 42;
const ROOM_WALL_HEIGHT = 7.2;
const ROOM_EXIT_Z = -ROOM_DEPTH / 2 + 1.15;
const ROOM_START_Z = ROOM_DEPTH / 2 - 5.5;
const ROOM_GRID_Z = ROOM_EXIT_Z + 0.42;
const VISIBLE_ROOM_TILE_INDEX_OFFSETS = [-1, 0, 1];
const ROOM_CONTINUATION_TILE_OFFSETS = [-1, 1];
const ROOM_GRID_RADIUS = 2.3;
const ROOM_GOAL_Y = 3.05;
const ROOM_DEPOSIT_HIT_RADIUS = 1.65;
const ROOM_EXIT_RADIUS = 2.35;
const ROOM_MIN_SPAWN_Z = -ROOM_DEPTH / 2 + 9;
const ROOM_MAX_SPAWN_Z = ROOM_DEPTH / 2 - 7;
const roomGroup = new THREE.Group();
scene.add(roomGroup);
const visibleRoomTileGroups = VISIBLE_ROOM_TILE_INDEX_OFFSETS.map(() => {
	const group = new THREE.Group();
	roomGroup.add(group);
	return group;
});
const roomCenterTileGroup = visibleRoomTileGroups[VISIBLE_ROOM_TILE_INDEX_OFFSETS.indexOf(0)];
const roomGridCells = [];
const roomGridSoulCubes = [];
const roomColliders = [];
let roomDoorMesh = null;
let roomMirrorPlane = null;
let roomMirrorFrame = null;
let roomEntranceMirrorPlane = null;
let roomEntranceMirrorFrame = null;
let roomFloorMesh = null;
const roomContinuationGridCells = ROOM_CONTINUATION_TILE_OFFSETS.map(() => []);
const roomContinuationGridSoulCubes = ROOM_CONTINUATION_TILE_OFFSETS.map(() => []);

function getRuntimeResourceSnapshot() {
	return {
		geometries: renderer.info && renderer.info.memory ? renderer.info.memory.geometries : 0,
		textures: renderer.info && renderer.info.memory ? renderer.info.memory.textures : 0,
		roomChildren: roomGroup ? roomGroup.children.length : 0,
		storedSouls: phoneStorage ? phoneStorage.storedCubes.length : 0,
		pendingShots: pendingDischargedSouls ? pendingDischargedSouls.length : 0,
		flowerPowerups: flowerPowerups ? flowerPowerups.length : 0,
		supplementalBattery: supplementalBattery ? supplementalBattery.value : 0,
		activePowerupStacks: { ...activePowerupStacks }
	};
}

window.digitalBreakdownMemory = getRuntimeResourceSnapshot;
const ROOM_IDENTITY_LAB = "lab";

function chooseRoomIdentity() {
	return ROOM_IDENTITY_LAB;
}

function getRoomEnemySpecies() {
	return "humans";
}

const room = {
	index: 1,
	seed: Math.floor(Math.random() * 1000000),
	identity: ROOM_IDENTITY_LAB,
	requiredSouls: ROOM_REQUIRED_SOULS,
	depositedSouls: 0,
	doorOpen: false,
	depositCooldown: 0
};
// roomTopology tracks wrapped room-space traversal. `currentTileIndex` and
// `previousTileIndex` are the player's current and previous discrete tile
// indices, while `mode` reports whether doorway crossings currently loop or
// advance.
const roomTopology = {
	currentTileIndex: 0,
	previousTileIndex: 0,
	mode: "loop"
};

const fixedPortal = {
	sourceZ: ROOM_EXIT_Z + 0.18,
	destinationZ: ROOM_DEPTH / 2 - 0.74,
	halfWidth: 2.1,
	// Doorway topology dimensions. These describe the passable opening, not a
	// transition trigger. Crossing only resolves when the body is actually inside
	// the doorway aperture.
	bottomY: GROUND_Y,
	topY: 3.72,
	seamEpsilon: 0.018,
	entryBuffer: 0.10,
	cameraEyeHeight: 0.72,
	cameraForwardOffset: 0.12,
	minCameraDistance: 0.16,
	maxCameraDistance: ROOM_DEPTH - 5.0
};

function getRoomTileIndex(z) {
	return Math.floor((z + ROOM_DEPTH * 0.5) / ROOM_DEPTH);
}

function getRoomTileOriginZ(tileIndex) {
	return tileIndex * ROOM_DEPTH;
}

function wrapZ(z) {
	return z - getRoomTileOriginZ(getRoomTileIndex(z));
}

function getRoomLocalPosition(worldPosition, target = new THREE.Vector3()) {
	target.copy(worldPosition);
	target.z = wrapZ(worldPosition.z);
	return target;
}

function getPlayerLocalZ() {
	return wrapZ(player.pos.z);
}

const RUN_RULE_DEFS = Object.freeze([
	Object.freeze({
		id: "required_slots",
		label: "slots+",
		description: "Future rooms require one more deposited soul.",
		maxStacks: 3,
		effects: Object.freeze({ requiredSouls: 1 })
	}),
	Object.freeze({
		id: "crowded_room",
		label: "enemies+",
		description: "Future rooms keep one more active enemy in circulation.",
		maxStacks: 4,
		effects: Object.freeze({ extraEnemies: 1 })
	}),
	Object.freeze({
		id: "faster_slurp",
		label: "slurp+",
		description: "Vacuum ingestion progresses faster.",
		maxStacks: 4,
		effects: Object.freeze({ slurpRate: 0.18 })
	})
]);

const runRules = {
	active: [],
	history: [],
	nextId: 1,
	lastAdded: null
};

function getRunRuleDef(id) {
	return RUN_RULE_DEFS.find((def) => def.id === id) || null;
}

function getRunRuleStack(id) {
	return runRules.active.reduce((count, rule) => count + (rule.id === id ? 1 : 0), 0);
}

function getRunScalar(name, base = 1) {
	let value = base;
	for (const rule of runRules.active) {
		const def = getRunRuleDef(rule.id);
		if (!def || !def.effects || !(name in def.effects)) continue;
		if (name === "slurpRate") value *= 1 + def.effects[name];
		else value += def.effects[name];
	}
	return value;
}

function canAddRunRule(def) {
	return !!def && getRunRuleStack(def.id) < (def.maxStacks ?? 1);
}

function chooseNextRunRule() {
	const candidates = RUN_RULE_DEFS.filter(canAddRunRule);
	if (candidates.length === 0) return null;
	const roll = seededRoomValue(runRules.nextId * 19.13 + room.index * 7.91);
	return candidates[Math.floor(roll * candidates.length) % candidates.length];
}

function addRunRule(def, source = "room-clear") {
	if (!canAddRunRule(def)) return null;
	const entry = {
		runId: runRules.nextId++,
		id: def.id,
		label: def.label || def.id,
		source,
		roomIndex: room ? room.index : 0
	};
	runRules.active.push(entry);
	runRules.history.push({ ...entry });
	runRules.lastAdded = entry;
	return entry;
}

function advanceRunRulesForRoom() {
	return addRunRule(chooseNextRunRule(), "room-clear");
}

function resetRunRules() {
	runRules.active.length = 0;
	runRules.history.length = 0;
	runRules.nextId = 1;
	runRules.lastAdded = null;
}

function getRunRuleSummary(limit = 3) {
	if (runRules.active.length === 0) return "none";
	const parts = [];
	for (const def of RUN_RULE_DEFS) {
		const stack = getRunRuleStack(def.id);
		if (stack > 0) parts.push(`${def.label || def.id}x${stack}`);
	}
	return parts.slice(0, limit).join(" ") || "none";
}

function getPlayerRoomTileOriginZ() {
	return getRoomTileOriginZ(roomTopology.currentTileIndex);
}

function getContinuationTileOriginZ(tileOffset) {
	return getRoomTileOriginZ(roomTopology.currentTileIndex + tileOffset);
}

function getPlayerTileLocalZFromWorld(worldZ) {
	return worldZ - getPlayerRoomTileOriginZ();
}

function getNearestRepeatedWorldZ(localZ, referenceWorldZ) {
	const centerTile = getRoomTileIndex(referenceWorldZ);
	let bestZ = localZ + getRoomTileOriginZ(centerTile);
	let bestDist = Math.abs(bestZ - referenceWorldZ);
	for (let offset = -1; offset <= 1; offset += 2) {
		const candidateZ = localZ + getRoomTileOriginZ(centerTile + offset);
		const dist = Math.abs(candidateZ - referenceWorldZ);
		if (dist < bestDist) {
			bestZ = candidateZ;
			bestDist = dist;
		}
	}
	return bestZ;
}

function getTargetWorldZInPlayerTile(idx) {
	return getNearestRepeatedWorldZ(targetData[idx + T_Z], player.pos.z);
}

function writeTargetWorldZInPlayerTile(idx, worldZ) {
	targetData[idx + T_Z] = getPlayerTileLocalZFromWorld(worldZ);
}

function writeTargetWorldZCanonical(idx, worldZ) {
	targetData[idx + T_Z] = wrapZ(worldZ);
}

function isInsideDoorAperture(position, pad = 0) {
	return Math.abs(position.x - roomExitPos.x) <= fixedPortal.halfWidth + pad &&
		position.y >= fixedPortal.bottomY - 0.12 &&
		position.y <= fixedPortal.topY + 0.22;
}

function smoothDoorStep(t) {
	t = THREE.MathUtils.clamp(t, 0, 1);
	return t * t * (3 - 2 * t);
}

function getActiveHumanTarget() {
	const extraEnemies = Math.max(0, Math.round(getRunScalar("extraEnemies", 0)));
	return Math.min(
		TARGET_COUNT,
		ACTIVE_HUMAN_TARGET_CAP,
		ACTIVE_HUMAN_TARGET + Math.max(0, room.index - 1) + extraEnemies
	);
}
const roomDepositPos = new THREE.Vector3(0, ROOM_GOAL_Y, ROOM_GRID_Z);
const roomExitPos = new THREE.Vector3(0, HUMAN_VISUAL_FOOT_Y, ROOM_EXIT_Z);
const shotPrevPos = new THREE.Vector3();
const shotCurrPos = new THREE.Vector3();
const roomLocalPositionTmp = new THREE.Vector3();

function seededRoomValue(offset = 0) {
	const x = Math.sin((room.seed + offset) * 12.9898) * 43758.5453;
	return x - Math.floor(x);
}

function disposeMaterialResource(material, disposed = new Set(), preservedTextures = new Set()) {
	if (!material) return;
	if (Array.isArray(material)) {
		for (const entry of material) disposeMaterialResource(entry, disposed, preservedTextures);
		return;
	}
	if (disposed.has(material)) return;
	disposed.add(material);
	for (const key of Object.keys(material)) {
		const value = material[key];
		if (value && value.isTexture && !preservedTextures.has(value) && typeof value.dispose === "function") {
			value.dispose();
		}
	}
	if (typeof material.dispose === "function") material.dispose();
}

function disposeObjectTree(root, options = {}) {
	if (!root) return;
	const disposedGeometries = new Set();
	const disposedMaterials = new Set();
	const preservedTextures = options.preservedTextures || new Set();
	root.traverse((node) => {
		if (node.geometry && !disposedGeometries.has(node.geometry)) {
			disposedGeometries.add(node.geometry);
			node.geometry.dispose();
		}
		disposeMaterialResource(node.material, disposedMaterials, preservedTextures);
	});
	if (root.parent) root.parent.remove(root);
}

function clearThreeGroup(group, options = {}) {
	while (group.children.length) {
		disposeObjectTree(group.children[group.children.length - 1], options);
	}
}

function clearRoomEnvironment() {
	for (const tileGroup of visibleRoomTileGroups) {
		clearThreeGroup(tileGroup);
	}
	roomGridCells.length = 0;
	roomGridSoulCubes.length = 0;
	for (let i = 0; i < ROOM_CONTINUATION_TILE_OFFSETS.length; i++) {
		roomContinuationGridCells[i].length = 0;
		roomContinuationGridSoulCubes[i].length = 0;
	}
	roomColliders.length = 0;
	roomDoorMesh = null;
	roomMirrorPlane = null;
	roomMirrorFrame = null;
	roomEntranceMirrorPlane = null;
	roomEntranceMirrorFrame = null;
	roomFloorMesh = null;
}

function getVisibleRoomTileGroup(tileOffset = 0) {
	const groupIndex = tileOffset + 1;
	if (groupIndex < 0) throw new Error(`Negative room tile group index: ${tileOffset}`);
	if (groupIndex >= visibleRoomTileGroups.length || VISIBLE_ROOM_TILE_INDEX_OFFSETS[groupIndex] !== tileOffset) {
		throw new Error(`Invalid room tile group offset: ${tileOffset}`);
	}
	return visibleRoomTileGroups[groupIndex];
}

function addRoomBox(width, height, depth, x, y, z, material, parent = roomCenterTileGroup) {
	const mesh = new THREE.Mesh(new THREE.BoxGeometry(width, height, depth), material);
	mesh.position.set(x, y, z);
	mesh.castShadow = true;
	mesh.receiveShadow = true;
	parent.add(mesh);
	return mesh;
}

function addRoomCollider(width, height, depth, x, y, z) {
	roomColliders.push({
		minX: x - width / 2,
		maxX: x + width / 2,
		minZ: z - depth / 2,
		maxZ: z + depth / 2,
		bottomY: y - height / 2,
		topY: y + height / 2,
		x, z, width, depth, height
	});
}



/* ── Same-room door topology pipeline ───────────────────────────────────────
 *  The doorway is only an opening. Gameplay owns the topology:
 *    locked door crossing             → keep looping the same room tile
 *    open source door crossing        → advance to the next room
 *  There is no transition camera, no mask, and no room rebuild while locked.
 * ─────────────────────────────────────────────────────────────────────── */
function buildRoomEnvironment() {
	clearRoomEnvironment();
	const hue = seededRoomValue(1);
	const floorMat = new THREE.MeshStandardMaterial({ color: 0x9aa7ad, roughness: 0.86, metalness: 0.02 });
	const wallMat = new THREE.MeshStandardMaterial({ color: new THREE.Color().setHSL(0.58 + hue * 0.03, 0.08, 0.60), roughness: 0.82, metalness: 0.04 });
	const doorMat = new THREE.MeshStandardMaterial({ color: 0x010203, emissive: 0x000000, emissiveIntensity: 0.0, roughness: 0.88, transparent: true, opacity: 0.035, depthWrite: false });
	const goalFrameMat = new THREE.MeshStandardMaterial({ color: 0x8ff7ff, emissive: 0x2bdcff, emissiveIntensity: 0.65, roughness: 0.5, metalness: 0.04 });
	const goalHoleMat = new THREE.MeshBasicMaterial({ color: 0x000000 });
	const depositedSoulMat = new THREE.MeshStandardMaterial({ color: 0x8ff7ff, emissive: 0x36d8ff, emissiveIntensity: 0.95, roughness: 0.42, metalness: 0.0 });
	roomFloorMesh = addRoomBox(ROOM_WIDTH, 0.08, ROOM_DEPTH, 0, LAB_FLOOR_TOP_Y - 0.04, 0, floorMat);
	roomFloorMesh.receiveShadow = true;
	const ceiling = addRoomBox(ROOM_WIDTH, 0.16, ROOM_DEPTH, 0, ROOM_WALL_HEIGHT + 0.08, 0, wallMat);
	ceiling.castShadow = false;
	ceiling.receiveShadow = true;
	const doorOpeningWidth = 5.35;
	const doorOpeningHeight = 3.95;
	const sideWallWidth = (ROOM_WIDTH - doorOpeningWidth) / 2;
	const sideWallX = doorOpeningWidth / 2 + sideWallWidth / 2;
	const topWallHeight = ROOM_WALL_HEIGHT - doorOpeningHeight;
	const topWallY = doorOpeningHeight + topWallHeight / 2;
	// Front/back walls are segmented so the portal-door pair is a real visual opening,
	// not a freestanding mirror panel in front of a large wall.
	addRoomBox(sideWallWidth, ROOM_WALL_HEIGHT, 0.5, -sideWallX, ROOM_WALL_HEIGHT / 2, ROOM_DEPTH / 2, wallMat);
	addRoomBox(sideWallWidth, ROOM_WALL_HEIGHT, 0.5, sideWallX, ROOM_WALL_HEIGHT / 2, ROOM_DEPTH / 2, wallMat);
	addRoomBox(doorOpeningWidth, topWallHeight, 0.5, 0, topWallY, ROOM_DEPTH / 2, wallMat);
	addRoomBox(sideWallWidth, ROOM_WALL_HEIGHT, 0.5, -sideWallX, ROOM_WALL_HEIGHT / 2, -ROOM_DEPTH / 2, wallMat);
	addRoomBox(sideWallWidth, ROOM_WALL_HEIGHT, 0.5, sideWallX, ROOM_WALL_HEIGHT / 2, -ROOM_DEPTH / 2, wallMat);
	addRoomBox(doorOpeningWidth, topWallHeight, 0.5, 0, topWallY, -ROOM_DEPTH / 2, wallMat);
	addRoomBox(0.5, ROOM_WALL_HEIGHT, ROOM_DEPTH, -ROOM_WIDTH / 2, ROOM_WALL_HEIGHT / 2, 0, wallMat);
	addRoomBox(0.5, ROOM_WALL_HEIGHT, ROOM_DEPTH, ROOM_WIDTH / 2, ROOM_WALL_HEIGHT / 2, 0, wallMat);
	// Wrapped-space visual contract: the door is an aperture into real repeated
	// room geometry, not a picture, render texture, or blocker. This helper builds
	// the continuation shell only; non-authoritative gameplay visuals are mirrored
	// separately so the repeated rooms show the same visible state without owning it.
	function addVisualContinuationShell(tileOffset) {
		const tileGroup = getVisibleRoomTileGroup(tileOffset);
		const copyFloor = addRoomBox(ROOM_WIDTH, 0.08, ROOM_DEPTH, 0, LAB_FLOOR_TOP_Y - 0.04, 0, floorMat.clone(), tileGroup);
		copyFloor.castShadow = false;
		copyFloor.receiveShadow = true;
		const copyCeiling = addRoomBox(ROOM_WIDTH, 0.16, ROOM_DEPTH, 0, ROOM_WALL_HEIGHT + 0.08, 0, wallMat.clone(), tileGroup);
		copyCeiling.castShadow = false;
		copyCeiling.receiveShadow = true;
		const copyWallMat = wallMat.clone();
		copyWallMat.color.multiplyScalar(0.96);
		addRoomBox(sideWallWidth, ROOM_WALL_HEIGHT, 0.5, -sideWallX, ROOM_WALL_HEIGHT / 2, ROOM_DEPTH / 2, copyWallMat, tileGroup);
		addRoomBox(sideWallWidth, ROOM_WALL_HEIGHT, 0.5, sideWallX, ROOM_WALL_HEIGHT / 2, ROOM_DEPTH / 2, copyWallMat, tileGroup);
		addRoomBox(doorOpeningWidth, topWallHeight, 0.5, 0, topWallY, ROOM_DEPTH / 2, copyWallMat, tileGroup);
		addRoomBox(sideWallWidth, ROOM_WALL_HEIGHT, 0.5, -sideWallX, ROOM_WALL_HEIGHT / 2, -ROOM_DEPTH / 2, copyWallMat, tileGroup);
		addRoomBox(sideWallWidth, ROOM_WALL_HEIGHT, 0.5, sideWallX, ROOM_WALL_HEIGHT / 2, -ROOM_DEPTH / 2, copyWallMat, tileGroup);
		addRoomBox(doorOpeningWidth, topWallHeight, 0.5, 0, topWallY, -ROOM_DEPTH / 2, copyWallMat, tileGroup);
		addRoomBox(0.5, ROOM_WALL_HEIGHT, ROOM_DEPTH, -ROOM_WIDTH / 2, ROOM_WALL_HEIGHT / 2, 0, copyWallMat, tileGroup);
		addRoomBox(0.5, ROOM_WALL_HEIGHT, ROOM_DEPTH, ROOM_WIDTH / 2, ROOM_WALL_HEIGHT / 2, 0, copyWallMat, tileGroup);
	}
	for (const tileOffset of ROOM_CONTINUATION_TILE_OFFSETS) addVisualContinuationShell(tileOffset);
	roomDoorMesh = null;
	roomMirrorPlane = null;
	roomMirrorFrame = null;
	roomEntranceMirrorPlane = null;
	roomEntranceMirrorFrame = null;
	const cols = room.requiredSouls;
	const startX = -((cols - 1) * 0.82) / 2;
	for (let i = 0; i < room.requiredSouls; i++) {
		const x = startX + i * 0.82;
		const frame = new THREE.Mesh(new THREE.BoxGeometry(0.72, 0.72, 0.06), goalFrameMat.clone());
		frame.position.set(x, ROOM_GOAL_Y, ROOM_GRID_Z - 0.04);
		frame.castShadow = false;
		frame.receiveShadow = false;
		roomCenterTileGroup.add(frame);

		const hole = new THREE.Mesh(new THREE.BoxGeometry(0.52, 0.52, 0.08), goalHoleMat.clone());
		hole.position.set(x, ROOM_GOAL_Y, ROOM_GRID_Z);
		hole.castShadow = false;
		hole.receiveShadow = false;
		roomCenterTileGroup.add(hole);

		roomGridCells.push({ rim: frame, hole, pole: null, backboard: null, x, y: ROOM_GOAL_Y, z: ROOM_GRID_Z });

		const cube = new THREE.Mesh(new THREE.BoxGeometry(0.36, 0.36, 0.36), depositedSoulMat.clone());
		cube.position.set(x, -999, ROOM_GRID_Z + 0.12);
		cube.castShadow = true;
		cube.receiveShadow = true;
		roomCenterTileGroup.add(cube);
		roomGridSoulCubes.push(cube);
		for (let c = 0; c < ROOM_CONTINUATION_TILE_OFFSETS.length; c++) {
			const tileGroup = getVisibleRoomTileGroup(ROOM_CONTINUATION_TILE_OFFSETS[c]);
			const copyFrame = new THREE.Mesh(new THREE.BoxGeometry(0.72, 0.72, 0.06), goalFrameMat.clone());
			copyFrame.position.set(x, ROOM_GOAL_Y, ROOM_GRID_Z - 0.04);
			copyFrame.castShadow = false;
			copyFrame.receiveShadow = false;
			tileGroup.add(copyFrame);

			const copyHole = new THREE.Mesh(new THREE.BoxGeometry(0.52, 0.52, 0.08), goalHoleMat.clone());
			copyHole.position.set(x, ROOM_GOAL_Y, ROOM_GRID_Z);
			copyHole.castShadow = false;
			copyHole.receiveShadow = false;
			tileGroup.add(copyHole);

			const copyCube = new THREE.Mesh(new THREE.BoxGeometry(0.36, 0.36, 0.36), depositedSoulMat.clone());
			copyCube.position.set(x, -999, ROOM_GRID_Z + 0.12);
			copyCube.castShadow = true;
			copyCube.receiveShadow = true;
			tileGroup.add(copyCube);
			roomContinuationGridCells[c].push({ rim: copyFrame, hole: copyHole, x, y: ROOM_GOAL_Y, z: ROOM_GRID_Z });
			roomContinuationGridSoulCubes[c].push(copyCube);
		}
	}
	const obstacleMat = new THREE.MeshStandardMaterial({ color: 0x6f7d86, roughness: 0.74, metalness: 0.06 });
	const obstacleCount = 8 + Math.min(room.index, 7);
	for (let i = 0; i < obstacleCount; i++) {
		let px = (seededRoomValue(20 + i) - 0.5) * (ROOM_WIDTH - 8);
		let pz = ROOM_MIN_SPAWN_Z + seededRoomValue(60 + i) * (ROOM_MAX_SPAWN_Z - ROOM_MIN_SPAWN_Z);
		const keepStartClear = Math.abs(px) < 4.5 && pz > ROOM_START_Z - 4.5;
		const keepGoalClear = Math.abs(px) < 5.5 && Math.abs(pz - ROOM_GRID_Z) < 4.5;
		if (keepStartClear) px += px < 0 ? -5 : 5;
		if (keepGoalClear) pz += 5.0;
		const w = 1.0 + seededRoomValue(120 + i) * 1.7;
		const d = 1.0 + seededRoomValue(150 + i) * 1.7;
		const h = 0.55 + seededRoomValue(90 + i) * 1.45;
		addRoomBox(w, h, d, px, h / 2, pz, obstacleMat);
		for (const tileOffset of ROOM_CONTINUATION_TILE_OFFSETS) {
			const copy = addRoomBox(w, h, d, px, h / 2, pz, obstacleMat.clone(), getVisibleRoomTileGroup(tileOffset));
			copy.castShadow = true;
			copy.receiveShadow = true;
		}
		addRoomCollider(w, h, d, px, h / 2, pz);
	}
	updateRoomVisuals();
}

function renderGoalSlots() {
	if (!goalSlots) return;
	if (goalSlots.children.length !== room.requiredSouls) {
		goalSlots.innerHTML = "";
		for (let i = 0; i < room.requiredSouls; i++) {
			const slot = document.createElement("div");
			slot.className = "goal-slot";
			goalSlots.appendChild(slot);
		}
	}
	for (let i = 0; i < goalSlots.children.length; i++) {
		goalSlots.children[i].classList.toggle("filled", i < room.depositedSouls);
	}
}

function updateVisibleRoomTilePositions() {
	roomGroup.position.z = 0;
	for (let i = 0; i < visibleRoomTileGroups.length; i++) {
		visibleRoomTileGroups[i].position.z = getRoomTileOriginZ(roomTopology.currentTileIndex + VISIBLE_ROOM_TILE_INDEX_OFFSETS[i]);
	}
	updateShadowContinuity();
}

function updateShadowContinuity() {
	if (!sun || !sunTarget) return;
	// Follow continuous player/world Z, not discrete tile Z. Moving both light and
	// target by the same continuous offset preserves shadow direction while keeping
	// the shadow camera centered across doorway seams. Discrete tile following made
	// shadows pop direction/coverage when roomTopology.currentTileIndex changed.
	const shadowFollowZ = player ? player.pos.z : getRoomTileOriginZ(roomTopology.currentTileIndex);
	sun.position.set(
		SUN_BASE_POSITION.x,
		SUN_BASE_POSITION.y,
		SUN_BASE_POSITION.z + shadowFollowZ
	);
	sunTarget.position.set(
		SUN_TARGET_BASE.x,
		SUN_TARGET_BASE.y,
		SUN_TARGET_BASE.z + shadowFollowZ
	);
	sunTarget.updateMatrixWorld();
	sun.updateMatrixWorld();
}

function updateRoomVisuals() {
	updateVisibleRoomTilePositions();
	renderGoalSlots();
	for (let i = 0; i < roomGridCells.length; i++) {
		const cell = roomGridCells[i];
		const cube = roomGridSoulCubes[i];
		const filled = i < room.depositedSouls;
		const pulse = Math.sin(frame.now * 0.006 + i) * 0.025;
		cell.hole.position.set(cell.x, cell.y, cell.z);
		cell.hole.scale.setScalar(1);
		cell.rim.rotation.set(0, 0, 0);
		cell.rim.scale.setScalar(filled ? 1.05 + pulse : 1.0);
		cell.rim.material.emissiveIntensity = filled ? 1.15 : 0.45;
		if (cube) {
			cube.visible = filled;
			cube.position.set(cell.x, filled ? cell.y + pulse : -999, cell.z + 0.38);
			cube.rotation.set(frame.now * 0.0015 + i, frame.now * 0.002 + i * 0.7, frame.now * 0.001);
			cube.scale.setScalar(filled ? 1.0 : 0.01);
		}
	}
	for (let c = 0; c < ROOM_CONTINUATION_TILE_OFFSETS.length; c++) {
		for (let i = 0; i < roomContinuationGridCells[c].length; i++) {
			const cell = roomContinuationGridCells[c][i];
			const cube = roomContinuationGridSoulCubes[c][i];
			const filled = i < room.depositedSouls;
			const pulse = Math.sin(frame.now * 0.006 + i) * 0.025;
			cell.hole.position.set(cell.x, cell.y, cell.z);
			cell.hole.scale.setScalar(1);
			cell.rim.rotation.set(0, 0, 0);
			cell.rim.scale.setScalar(filled ? 1.05 + pulse : 1.0);
			cell.rim.material.emissiveIntensity = filled ? 1.15 : 0.45;
			if (cube) {
				cube.visible = filled;
				cube.position.set(cell.x, filled ? cell.y + pulse : -999, cell.z + 0.38);
				cube.rotation.set(frame.now * 0.0015 + i, frame.now * 0.002 + i * 0.7, frame.now * 0.001);
				cube.scale.setScalar(filled ? 1.0 : 0.01);
			}
		}
	}
	if (roomDoorMesh) {
		roomDoorMesh.visible = false;
		roomDoorMesh.material.opacity = 0.0;
	}
	if (roomMirrorPlane) {
		roomMirrorPlane.visible = true;
		applyPortalWindowMaterialContract(roomMirrorPlane);
	}
	if (roomMirrorFrame) {
		roomMirrorFrame.visible = true;
		roomMirrorFrame.material.emissiveIntensity += ((room.doorOpen ? 1.15 : 0.32) - roomMirrorFrame.material.emissiveIntensity) * 0.14;
	}
	if (roomEntranceMirrorPlane) {
		roomEntranceMirrorPlane.visible = true;
		applyPortalWindowMaterialContract(roomEntranceMirrorPlane);
	}
	if (roomEntranceMirrorFrame) {
		roomEntranceMirrorFrame.visible = true;
		roomEntranceMirrorFrame.material.emissiveIntensity += ((room.doorOpen ? 1.15 : 0.32) - roomEntranceMirrorFrame.material.emissiveIntensity) * 0.14;
	}
	if (roomHud) roomHud.classList.toggle("open", room.doorOpen);
	if (roomLabel) roomLabel.textContent = "Room: " + room.index;
	if (roomDoorLabel) roomDoorLabel.textContent = room.doorOpen ? "Door: Open" : "Door: Loop";
}

function randomRoomSpawnPoint(slot = 0) {
	for (let attempt = 0; attempt < 18; attempt++) {
		const x = (seededRoomValue(room.index * 100 + slot * 17 + attempt * 3 + 1) - 0.5) * (ROOM_WIDTH - 7);
		const z = ROOM_MIN_SPAWN_Z + seededRoomValue(room.index * 100 + slot * 17 + attempt * 3 + 2) * (ROOM_MAX_SPAWN_Z - ROOM_MIN_SPAWN_Z);
		if (!isRoomPointBlocked(x, z, 0.7)) return { x, z };
	}
	return { x: 0, z: ROOM_START_Z - 4.5 };
}

function clampHumanWalkTargetX(v) {
	return THREE.MathUtils.clamp(v, -ROOM_WIDTH / 2 + 2.0, ROOM_WIDTH / 2 - 2.0);
}
function clampHumanWalkTargetZ(v) {
	return THREE.MathUtils.clamp(v, -ROOM_DEPTH / 2 + 5.0, ROOM_DEPTH / 2 - 3.0);
}

function isRoomPointBlocked(x, z, radius = 0.45) {
	const localZ = wrapZ(z);
	for (const c of roomColliders) {
		if (
			x > c.minX - radius && x < c.maxX + radius &&
			localZ > c.minZ - radius && localZ < c.maxZ + radius
		) return true;
	}
	return false;
}

function getPlayerSupportY(x, z) {
	let supportY = GROUND_Y;
	const radius = 0.34;
	const localZ = wrapZ(z);
	for (const c of roomColliders) {
		if (
			x > c.minX - radius && x < c.maxX + radius &&
			localZ > c.minZ - radius && localZ < c.maxZ + radius
		) {
			supportY = Math.max(supportY, c.topY + GROUND_Y);
		}
	}
	return Math.min(supportY, getPlayerCeilingLimit());
}

function resolvePlayerObstacleCollisions() {
	const radius = 0.34;
	// Use the player's actual post-movement tile, not roomTopology.currentTileIndex,
	// because topology is updated after collision. Using the stale previous tile can
	// momentarily push the player back at the doorway seam, especially when walking
	// backward through repeated rooms.
	let tileOriginZ = getRoomTileOriginZ(getRoomTileIndex(player.pos.z));
	let localPlayerZ = player.pos.z - tileOriginZ;
	for (const c of roomColliders) {
		const onTop = player.pos.y >= c.topY + GROUND_Y - 0.08;
		if (onTop) continue;
		if (player.pos.y < c.bottomY - 0.4 || player.pos.y > c.topY + GROUND_Y + 0.4) continue;
		if (
			player.pos.x > c.minX - radius && player.pos.x < c.maxX + radius &&
			localPlayerZ > c.minZ - radius && localPlayerZ < c.maxZ + radius
		) {
			const pushLeft = Math.abs(player.pos.x - (c.minX - radius));
			const pushRight = Math.abs((c.maxX + radius) - player.pos.x);
			const pushBack = Math.abs(localPlayerZ - (c.minZ - radius));
			const pushForward = Math.abs((c.maxZ + radius) - localPlayerZ);
			const minPush = Math.min(pushLeft, pushRight, pushBack, pushForward);
			if (minPush === pushLeft) { player.pos.x = c.minX - radius; if (player.vel.x > 0) player.vel.x = 0; }
			else if (minPush === pushRight) { player.pos.x = c.maxX + radius; if (player.vel.x < 0) player.vel.x = 0; }
			else if (minPush === pushBack) { localPlayerZ = c.minZ - radius; player.pos.z = tileOriginZ + localPlayerZ; if (player.vel.z > 0) player.vel.z = 0; }
			else { localPlayerZ = c.maxZ + radius; player.pos.z = tileOriginZ + localPlayerZ; if (player.vel.z < 0) player.vel.z = 0; }
		}
	}
}
function isWallClimbInputActive() {
	if (!input.keys.Space) return false;
	const axes = getMoveAxes();
	return Math.abs(axes.forward) + Math.abs(axes.strafe) > 0.05;
}

function getMoveIntentVector(out) {
	const axes = getMoveAxes();
	writeCursorBasis(vForward, vRight);
	out.set(0, 0, 0);
	if (axes.forward > 0) out.add(vForward);
	if (axes.forward < 0) out.sub(vForward);
	if (axes.strafe < 0) out.sub(vRight);
	if (axes.strafe > 0) out.add(vRight);
	if (out.lengthSq() > 0.0001) out.normalize();
	return out;
}

function isPushingIntoWallContact(contact) {
	getMoveIntentVector(vMove);
	if (vMove.lengthSq() <= 0.0001) return false;
	return vMove.x * contact.x + vMove.z * contact.z < PARKOUR.wallClimbPushDot;
}

function getPlayerCeilingLimit() {
	// player.pos.y is the phone/player center/support height, not the top of the visual model.
	// Keep enough body clearance under the lab ceiling so jumping from boxes cannot escape.
	const bodyClearance = Number.isFinite(PARKOUR.playerCeilingBodyClearance) ? PARKOUR.playerCeilingBodyClearance : 0.72;
	return ROOM_WALL_HEIGHT - GROUND_Y - PARKOUR.ceilingClearance - bodyClearance;
}

function clampPlayerToRoomHeight() {
	const maxY = getPlayerCeilingLimit();
	if (player.pos.y > maxY) {
		player.pos.y = maxY;
		if (player.jumpVel > 0) player.jumpVel = 0;
		player.vel.y = Math.min(player.vel.y || 0, 0);
	}
}

function getPlayerWallContact() {
	const radius = 0.34;
	const climbGap = 0.08;
	const minX = -ROOM_WIDTH / 2 + 1.1;
	const maxX = ROOM_WIDTH / 2 - 1.1;
	const minZ = -ROOM_DEPTH / 2 + 0.8;
	const maxZ = ROOM_DEPTH / 2 - 0.72;
	const localPlayerZ = getPlayerLocalZ();
	if (player.pos.x <= minX + climbGap) return { x: 1, z: 0, topY: getPlayerCeilingLimit() };
	if (player.pos.x >= maxX - climbGap) return { x: -1, z: 0, topY: getPlayerCeilingLimit() };
	if (localPlayerZ <= minZ + climbGap) return { x: 0, z: 1, topY: getPlayerCeilingLimit() };
	if (localPlayerZ >= maxZ - climbGap) return { x: 0, z: -1, topY: getPlayerCeilingLimit() };

	for (const c of roomColliders) {
		if (player.pos.y < c.bottomY - 0.2 || player.pos.y > c.topY + 1.1) continue;
		const inZ = localPlayerZ > c.minZ - radius && localPlayerZ < c.maxZ + radius;
		const inX = player.pos.x > c.minX - radius && player.pos.x < c.maxX + radius;
		if (inZ && Math.abs(player.pos.x - (c.minX - radius)) < climbGap + 0.05) return { x: -1, z: 0, topY: c.topY };
		if (inZ && Math.abs(player.pos.x - (c.maxX + radius)) < climbGap + 0.05) return { x: 1, z: 0, topY: c.topY };
		if (inX && Math.abs(localPlayerZ - (c.minZ - radius)) < climbGap + 0.05) return { x: 0, z: -1, topY: c.topY };
		if (inX && Math.abs(localPlayerZ - (c.maxZ + radius)) < climbGap + 0.05) return { x: 0, z: 1, topY: c.topY };
	}
	return null;
}

function applyWallClimb(dt) {
	if (player.grounded || !isWallClimbInputActive()) return;
	const contact = getPlayerWallContact();
	if (!contact || !isPushingIntoWallContact(contact)) return;
	const climbLimit = Math.min(getPlayerCeilingLimit(), contact.topY + GROUND_Y + PARKOUR.wallClimbMaxHeight);
	if (player.pos.y >= climbLimit) {
		player.pos.y = climbLimit;
		if (player.jumpVel > 0) player.jumpVel = 0;
		return;
	}
	spendBattery(BATTERY_WALL_CLIMB_DRAIN * dt, "CLIMB");
	player.jumpVel = Math.max(player.jumpVel, PARKOUR.wallClimbSpeed * batteryPower());
	player.vel.x *= PARKOUR.wallClimbGrip;
	player.vel.z *= PARKOUR.wallClimbGrip;
}


function depositShotSoulToRoom(slot) {
	if (room.doorOpen) return false;
	if (room.depositedSouls >= room.requiredSouls) return false;
	const idx = targetIndex(slot);
	const fillIndex = room.depositedSouls;
	room.depositedSouls = Math.min(room.requiredSouls, room.depositedSouls + 1);
	phoneStorage.lastDischargePulseTime = pulseTime();
	spawnRoomParticleBurst(targetData[idx + T_X], targetData[idx + T_Y], targetData[idx + T_Z]);
	const filledCell = roomGridCells[fillIndex];
	spawnRoomParticleBurst(filledCell ? filledCell.x : roomDepositPos.x, filledCell ? filledCell.y : roomDepositPos.y, filledCell ? filledCell.z : roomDepositPos.z);
	playCaptureSlotSound(fillIndex);
	soulDepositShot[slot] = 0;
	clearTargetSlot(slot);
	if (targetLock.target === slot) {
		targetLock.target = -1;
		targetLock.strength = 0;
	}
	if (room.depositedSouls >= room.requiredSouls) {
		room.doorOpen = true;
		playEventSound("paymentSuccess", 0.68);
	}
	updateRoomVisuals();
	return true;
}

function pointSegmentDistanceSq(px, py, pz, ax, ay, az, bx, by, bz) {
	const abx = bx - ax;
	const aby = by - ay;
	const abz = bz - az;
	const apx = px - ax;
	const apy = py - ay;
	const apz = pz - az;
	const denom = abx * abx + aby * aby + abz * abz || 0.0001;
	const t = THREE.MathUtils.clamp((apx * abx + apy * aby + apz * abz) / denom, 0, 1);
	const cx = ax + abx * t;
	const cy = ay + aby * t;
	const cz = az + abz * t;
	const dx = px - cx;
	const dy = py - cy;
	const dz = pz - cz;
	return dx * dx + dy * dy + dz * dz;
}


function damageHumansAlongDepositShot(projectileSlot, prevX, prevY, prevZ) {
	if (!soulDepositShot[projectileSlot]) return false;
	const pidx = targetIndex(projectileSlot);
	const cx = targetData[pidx + T_X];
	const cy = targetData[pidx + T_Y];
	const cz = targetData[pidx + T_Z];
	const kind = targetData[pidx + T_KIND] || 0;
	const damage = kind === 1 ? 1.65 : 0.9;
	const radius = kind === 1 ? 0.95 : 0.72;

	for (let h = 0; h < TARGET_COUNT; h++) {
		if (h === projectileSlot) continue;
		if (soulDepositShot[h]) continue;
		const hidx = targetIndex(h);
		if (targetData[hidx + T_ALIVE] <= 0) continue;
		if (isSoulUnavailable(h)) continue;
		if (isTargetSlurpable(hidx)) continue;

		const hx = targetData[hidx + T_X];
		const hy = targetData[hidx + T_RENDER_Y] || targetData[hidx + T_Y];
		const hz = targetData[hidx + T_Z];
		const distSq = pointSegmentDistanceSq(hx, hy, hz, prevX, prevY, prevZ, cx, cy, cz);
		if (distSq > radius * radius) continue;

		damageSoulShell(h, damage, hx, hy, hz);
		targetData[hidx + T_VX] += targetData[pidx + T_VX] * 0.08;
		targetData[hidx + T_VY] = Math.max(targetData[hidx + T_VY], 1.0);
		targetData[hidx + T_VZ] += targetData[pidx + T_VZ] * 0.08;
		spawnFlameBurst(hx, hy, hz, 0.55);
		spawnRoomParticleBurst(cx, cy, cz);
		return true;
	}
	return false;
}

function shotSoulHitsRoomGrid(slot, prevX = null, prevY = null, prevZ = null) {
	if (room.doorOpen || room.depositedSouls >= room.requiredSouls) return false;
	const idx = targetIndex(slot);
	if (!soulDepositShot[slot]) return false;
	const cx = targetData[idx + T_X];
	const cy = targetData[idx + T_Y];
	const cz = targetData[idx + T_Z];
	const hitRadius = ROOM_DEPOSIT_HIT_RADIUS * 1.35;
	const missRadius = hitRadius * 1.85;

	// Score against the next required slot, but also tolerate hitting any visible unfilled wall slot.
	for (let n = room.depositedSouls; n < room.requiredSouls; n++) {
		const cell = roomGridCells[n];
		const hx = cell ? cell.x : roomDepositPos.x;
		const hy = cell ? cell.y : roomDepositPos.y;
		const hz = cell ? cell.z : roomDepositPos.z;
		const dx = cx - hx;
		const dy = cy - hy;
		const dz = cz - hz;
		if (dx * dx + dy * dy + dz * dz < hitRadius * hitRadius) return true;
		if (prevX !== null && pointSegmentDistanceSq(hx, hy, hz, prevX, prevY, prevZ, cx, cy, cz) < hitRadius * hitRadius) return true;
	}

	// Extra wall-plane tolerance: if the shot crosses the slot wall near the slot row, accept it.
	// If it crosses near a slot but outside the capture box, play one quiet payment-failure cue.
	if (prevX !== null) {
		const wallZ = ROOM_GRID_Z;
		const crossedWall = (prevZ - wallZ) * (cz - wallZ) <= 0;
		if (crossedWall) {
			const dzSpan = cz - prevZ || 0.0001;
			const t = THREE.MathUtils.clamp((wallZ - prevZ) / dzSpan, 0, 1);
			const crossX = THREE.MathUtils.lerp(prevX, cx, t);
			const crossY = THREE.MathUtils.lerp(prevY, cy, t);
			let nearestMissSq = Infinity;
			for (let n = room.depositedSouls; n < room.requiredSouls; n++) {
				const cell = roomGridCells[n];
				const hx = cell ? cell.x : roomDepositPos.x;
				const hy = cell ? cell.y : roomDepositPos.y;
				if (Math.abs(crossX - hx) < hitRadius * 0.95 && Math.abs(crossY - hy) < hitRadius * 0.95) return true;
				const dx = crossX - hx;
				const dy = crossY - hy;
				nearestMissSq = Math.min(nearestMissSq, dx * dx + dy * dy);
			}
			if (!soulDepositNearMissPlayed[slot] && nearestMissSq < missRadius * missRadius) {
				soulDepositNearMissPlayed[slot] = 1;
				playEventSound("paymentFailure", 0.46);
			}
		}
	}
	return false;
}

function resetRoomTargets() {
	roomHumanRespawnQueue.length = 0;
	for (let i = 0; i < TARGET_COUNT; i++) {
		if (i < getActiveHumanTarget()) resetTarget(i);
		else clearTargetSlot(i);
	}
}


const DOOR_DATAMOSH_DISTANCE = 3.0;
const DOOR_DATAMOSH_MIN_STRENGTH = 0.018;
const DOOR_DATAMOSH_FRAME_SCALE = 0.58;
const doorDataMoshState = {
	active: false,
	progress: 0,
	strength: 0,
	distanceTravelled: 0,
	crossingWorldZ: 0,
	lastPlayerX: 0,
	lastPlayerY: 0,
	lastPlayerZ: 0,
	lastYaw: 0,
	lastPitch: 0,
	motionX: 0,
	motionY: 0,
	motionZ: 0
};

function setDoorDataMoshVisualProgress(progress) {
	const strength = THREE.MathUtils.clamp(progress, 0, 1);
	const active = strength > DOOR_DATAMOSH_MIN_STRENGTH;
	if (doorCrossingMask) {
		doorCrossingMask.style.setProperty("--dm-progress", strength.toFixed(3));
		doorCrossingMask.style.setProperty("--dm-opacity", "0");
		doorCrossingMask.classList.toggle("datamosh-progress", false);
		doorCrossingMask.classList.toggle("datamosh", false);
	}
	document.body.style.setProperty("--dm-progress", strength.toFixed(3));
	document.body.classList.toggle("door-datamosh-active", active);
}

function stopDoorDataMoshTransition() {
	doorDataMoshState.active = false;
	doorDataMoshState.progress = 0;
	doorDataMoshState.strength = 0;
	doorDataMoshState.distanceTravelled = 0;
	setDoorDataMoshVisualProgress(0);
	if (datamoshCanvas && datamoshCtx) {
		datamoshCanvas.style.opacity = "0";
		datamoshCtx.clearRect(0, 0, datamoshCanvas.width || 1, datamoshCanvas.height || 1);
	}
}

function updateDoorDataMoshTransition(dt) {
	if (!doorDataMoshState.active) {
		setDoorDataMoshVisualProgress(0);
		return;
	}
	const dx = player.pos.x - doorDataMoshState.lastPlayerX;
	const dy = player.pos.y - doorDataMoshState.lastPlayerY;
	const dz = player.pos.z - doorDataMoshState.lastPlayerZ;
	doorDataMoshState.motionX = dx;
	doorDataMoshState.motionY = dy;
	doorDataMoshState.motionZ = dz;
	doorDataMoshState.distanceTravelled += Math.hypot(dx, dy, dz);
	doorDataMoshState.lastPlayerX = player.pos.x;
	doorDataMoshState.lastPlayerY = player.pos.y;
	doorDataMoshState.lastPlayerZ = player.pos.z;
	const t = THREE.MathUtils.clamp(doorDataMoshState.distanceTravelled / DOOR_DATAMOSH_DISTANCE, 0, 1);
	// Distance is the only decay owner: if the player stops moving, the visual update
	// failure stays suspended, like duplicated P-frames with no new motion to resolve it.
	doorDataMoshState.progress = 1 - THREE.MathUtils.smoothstep(t, 0, 1);
	doorDataMoshState.strength = doorDataMoshState.progress;
	if (doorDataMoshState.progress <= DOOR_DATAMOSH_MIN_STRENGTH) {
		stopDoorDataMoshTransition();
		return;
	}
	setDoorDataMoshVisualProgress(doorDataMoshState.progress);
}

function triggerDoorDataMoshTransition() {
	if (!ensureDataMoshCanvasSize()) return;
	if (!datamoshLastWorldReady || !datamoshLastWorldCtx || !datamoshFrozenCtx || !datamoshFeedbackCtx) return;
	// Use a stable previously rendered world frame. Sampling the WebGL canvas right
	// during room ownership swap can return a cleared/undefined buffer, which reads
	// as a black flash. The HUD is not part of this source; this is game-canvas only.
	datamoshFrozenCtx.globalAlpha = 1;
	datamoshFrozenCtx.globalCompositeOperation = "copy";
	datamoshFrozenCtx.drawImage(datamoshLastWorldCanvas, 0, 0, datamoshFrozenCanvas.width, datamoshFrozenCanvas.height);
	datamoshFeedbackCtx.globalAlpha = 1;
	datamoshFeedbackCtx.globalCompositeOperation = "copy";
	datamoshFeedbackCtx.drawImage(datamoshFrozenCanvas, 0, 0);
	datamoshFrameReady = true;
	doorDataMoshState.active = true;
	doorDataMoshState.progress = 1;
	doorDataMoshState.strength = 1;
	doorDataMoshState.distanceTravelled = 0;
	doorDataMoshState.crossingWorldZ = player.pos.z;
	doorDataMoshState.lastPlayerX = player.pos.x;
	doorDataMoshState.lastPlayerY = player.pos.y;
	doorDataMoshState.lastPlayerZ = player.pos.z;
	doorDataMoshState.lastYaw = cursor.yaw;
	doorDataMoshState.lastPitch = cursor.pitch;
	doorDataMoshState.motionX = 0;
	doorDataMoshState.motionY = 0;
	doorDataMoshState.motionZ = 0;
	setDoorDataMoshVisualProgress(1);
}

function triggerDebugDataMosh() {
	if (!game.started || game.dead) return false;
	// Keyboard-triggered debug mosh uses the same doorway transition renderer.
	// If the cached world frame is not ready yet, capture the current WebGL frame first.
	if (!datamoshLastWorldReady) captureLastWorldDatamoshFrame();
	triggerDoorDataMoshTransition();
	return doorDataMoshState.active;
}

function ensureDataMoshCanvasSize() {
	if (!datamoshCanvas || !datamoshCtx || !datamoshFeedbackCtx || !datamoshFrozenCtx) return false;
	const scale = DOOR_DATAMOSH_FRAME_SCALE;
	const w = Math.max(2, Math.floor(innerWidth * scale));
	const h = Math.max(2, Math.floor(innerHeight * scale));
	if (datamoshCanvas.width !== w || datamoshCanvas.height !== h) {
		datamoshCanvas.width = w;
		datamoshCanvas.height = h;
		datamoshFeedbackCanvas.width = w;
		datamoshFeedbackCanvas.height = h;
		datamoshFrozenCanvas.width = w;
		datamoshFrozenCanvas.height = h;
		datamoshLastWorldCanvas.width = w;
		datamoshLastWorldCanvas.height = h;
		datamoshFrameReady = false;
		datamoshLastWorldReady = false;
	}
	return true;
}

function captureLastWorldDatamoshFrame() {
	if (!ensureDataMoshCanvasSize()) return;
	if (doorDataMoshState.active) return;
	const sourceCanvas = renderer && renderer.domElement ? renderer.domElement : canvas;
	if (!sourceCanvas || !datamoshLastWorldCtx) return;
	datamoshLastWorldCtx.globalAlpha = 1;
	datamoshLastWorldCtx.globalCompositeOperation = "copy";
	datamoshLastWorldCtx.drawImage(sourceCanvas, 0, 0, datamoshLastWorldCanvas.width, datamoshLastWorldCanvas.height);
	datamoshLastWorldReady = true;
}

function renderDoorDataMoshFeedback() {
	if (!ensureDataMoshCanvasSize()) return;
	const sourceCanvas = renderer && renderer.domElement ? renderer.domElement : canvas;
	if (!sourceCanvas) return;
	if (!doorDataMoshState.active || doorDataMoshState.strength <= DOOR_DATAMOSH_MIN_STRENGTH) {
		datamoshCanvas.style.opacity = "0";
		datamoshCtx.clearRect(0, 0, datamoshCanvas.width || 1, datamoshCanvas.height || 1);
		return;
	}
	const w = datamoshCanvas.width;
	const h = datamoshCanvas.height;
	const strength = THREE.MathUtils.clamp(doorDataMoshState.strength, 0, 1);
	const yawDelta = THREE.MathUtils.clamp(cursor.yaw - doorDataMoshState.lastYaw, -0.35, 0.35);
	const pitchDelta = THREE.MathUtils.clamp(cursor.pitch - doorDataMoshState.lastPitch, -0.25, 0.25);
	doorDataMoshState.lastYaw = cursor.yaw;
	doorDataMoshState.lastPitch = cursor.pitch;

	// Simulated duplicated P-frame motion: reuse the previous feedback frame and
	// push its pixels along player/camera motion. No grain, VHS, tint, scanlines,
	// or artificial damage; only repeated visual data being moved incorrectly.
	const walkVectorX = THREE.MathUtils.clamp(doorDataMoshState.motionX * w * 0.045, -20, 20);
	const walkVectorY = THREE.MathUtils.clamp(-doorDataMoshState.motionZ * h * 0.030, -24, 24);
	const cameraVectorX = THREE.MathUtils.clamp(-yawDelta * w * 0.92, -72, 72);
	const cameraVectorY = THREE.MathUtils.clamp(pitchDelta * h * 0.85, -48, 48);
	const mvX = (walkVectorX + cameraVectorX) * (0.42 + strength * 0.92);
	const mvY = (walkVectorY + cameraVectorY) * (0.42 + strength * 0.92);
	const stillPulse = Math.abs(mvX) + Math.abs(mvY) < 0.06;
	const driftX = stillPulse ? Math.sin(frame.time * 1.7) * 0.18 : 0;
	const driftY = stillPulse ? Math.cos(frame.time * 1.3) * 0.14 : 0;

	datamoshCtx.clearRect(0, 0, w, h);
	datamoshCtx.globalCompositeOperation = "source-over";
	datamoshCtx.imageSmoothingEnabled = true;
	// Repeated predicted frames: multiple copies along the same motion vector.
	// This creates the “old frame moved by new motion” feel without a stylized overlay.
	const passes = 5;
	for (let i = passes; i >= 1; i--) {
		const k = i / passes;
		datamoshCtx.globalAlpha = (0.10 + 0.14 * k) * strength;
		datamoshCtx.drawImage(
			datamoshFeedbackCanvas,
			Math.round((mvX + driftX) * k),
			Math.round((mvY + driftY) * k),
			w,
			h
		);
	}
	// Keep a strong copy of the latest predicted frame so the effect reads as
	// frozen visual information being dragged, not a fade or color filter.
	datamoshCtx.globalAlpha = THREE.MathUtils.clamp(0.52 + strength * 0.38, 0.52, 0.90);
	datamoshCtx.drawImage(datamoshFeedbackCanvas, Math.round(mvX), Math.round(mvY), w, h);
	// Let the real current room win only as traversal distance accumulates.
	const reveal = THREE.MathUtils.clamp(1 - strength, 0, 1);
	datamoshCtx.globalAlpha = 0.035 + reveal * 0.26;
	datamoshCtx.drawImage(sourceCanvas, 0, 0, w, h);
	datamoshCtx.globalAlpha = 1;
	datamoshCanvas.style.opacity = THREE.MathUtils.clamp(0.34 + strength * 0.56, 0.34, 0.90).toFixed(3);

	// Feed the predicted output back into itself: this is the P-frame duplication
	// loop. The new room is only injected lightly, so gameplay is current while
	// the visual information lags until movement resolves it.
	datamoshFeedbackCtx.globalCompositeOperation = "copy";
	datamoshFeedbackCtx.globalAlpha = 1;
	datamoshFeedbackCtx.drawImage(datamoshCanvas, 0, 0, w, h);
	datamoshFeedbackCtx.globalCompositeOperation = "source-over";
	datamoshFeedbackCtx.globalAlpha = 0.025 + reveal * 0.16;
	datamoshFeedbackCtx.drawImage(sourceCanvas, 0, 0, w, h);
	datamoshFeedbackCtx.globalAlpha = 1;
}

function setDoorCrossingMaskActive(active) {
	// The old loop mask remains in the DOM for CSS compatibility. The only
	// doorway transition now is the open-door data-mosh burst triggered on room advance.
	if (!doorCrossingMask) return;
	doorCrossingMask.classList.toggle("active", false);
}

/* Shared room teardown/rebuild called by both enterNextRoomThroughPortal and resetRunState. */
function resetRoomInventory() {
	room.requiredSouls = THREE.MathUtils.clamp(
		Math.round(getRunScalar("requiredSouls", ROOM_REQUIRED_SOULS)),
		1,
		9
	);
	room.depositedSouls = 0;
	room.doorOpen = false;
	room.depositCooldown = 0;
	phoneStorage.storedCubes.length = 0;
	if (phoneStorage.cubeProjectiles) phoneStorage.cubeProjectiles.length = 0;
	pendingDischargedSouls.length = 0;
	roomHumanRespawnQueue.length = 0;
	shuffleCaptureSlotSounds();
	syncStoredMirror();
	updateStoredSoulDisplays();
	buildRoomEnvironment();
	resetRoomTargets();
}

function enterNextRoomThroughPortal(direction = "sourceToDestination") {
	// Open-door contract in wrapped space: keep continuous player/camera state and
	// only advance room content ownership. The data-mosh burst is visual-only.
	triggerDoorDataMoshTransition();
	room.index++;
	room.seed = Math.floor(Math.random() * 1000000) + room.index * 9973;
	advanceRunRulesForRoom();
	room.identity = chooseRoomIdentity(room.seed, room.index);
	resetRoomInventory();
	syncRoomIdentityPresentation();
	roomTopology.mode = "advance";
	gainBattery(18, "NEXT ROOM");
}


window.graphicsDebug = function graphicsDebug() { return { ...graphicsSettings, legacyMode: isLegacyGraphicsMode(), particlesRuntimeEnabled: particlesRuntimeEnabled(), portalWindowRuntimeEnabled: false, presetContract: getGraphicsPreset(graphicsSettings.preset), pixelRatio: renderer.getPixelRatio(), shadowsEnabled: renderer.shadowMap.enabled, particleMeshVisible: !!(particleMesh && particleMesh.visible) }; };
window.triggerDataMosh = function triggerDataMosh() { return triggerDebugDataMosh(); };
window.portalDoorDebug = function portalDoorDebug() {
	return {
		roomIndex: room.index,
		doorOpen: room.doorOpen,
		depositedSouls: room.depositedSouls,
		requiredSouls: room.requiredSouls,
		sourceZ: fixedPortal.sourceZ,
		destinationZ: fixedPortal.destinationZ,
		preClampCrossing: true,
		doorLikeSeam: true,
		openingOnlyDoor: true,
		aperture: { halfWidth: fixedPortal.halfWidth, bottomY: fixedPortal.bottomY, topY: fixedPortal.topY, epsilon: fixedPortal.seamEpsilon },
		player: { x: player.pos.x, y: player.pos.y, z: player.pos.z },
		velocity: { x: player.vel.x, y: player.vel.y, z: player.vel.z },
		tileIndex: getRoomTileIndex(player.pos.z)
	};
};
window.roomTopologyDebug = function roomTopologyDebug() {
	return {
		playerWorldZ: player.pos.z,
		localZ: wrapZ(player.pos.z),
		previousTile: roomTopology.previousTileIndex,
		currentTile: roomTopology.currentTileIndex,
		roomIndex: room.index,
		doorOpen: room.doorOpen,
		mode: roomTopology.mode
	};
};

function updateRoomTopology(previousPlayerZ = player.pos.z, currentPlayerZ = player.pos.z) {
	const previousTile = getRoomTileIndex(previousPlayerZ);
	const currentTile = getRoomTileIndex(currentPlayerZ);
	let advanced = false;
	roomTopology.previousTileIndex = previousTile;
	roomTopology.currentTileIndex = currentTile;
	if (previousTile === currentTile) return false;
	const localPlayerPos = getRoomLocalPosition(player.pos, roomLocalPositionTmp);
	if (!isInsideDoorAperture(localPlayerPos, 0.04)) return false;
	if (room.doorOpen && currentTile < previousTile) {
		enterNextRoomThroughPortal("sourceToDestination");
		roomTopology.previousTileIndex = previousTile;
		roomTopology.currentTileIndex = getRoomTileIndex(player.pos.z);
		advanced = true;
	}
	roomTopology.mode = room.doorOpen ? "advance" : "loop";
	return advanced;
}

function updateRoomLoop(dt) {
	room.depositCooldown = Math.max(0, room.depositCooldown - dt);
	roomTopology.mode = room.doorOpen ? "advance" : "loop";
	updateDoorDataMoshTransition(dt);
	updateRoomVisuals();
}

buildRoomEnvironment();

const dummy = new THREE.Object3D();
const vForward = new THREE.Vector3();
const vRight = new THREE.Vector3();
const vMove = new THREE.Vector3();
const humanScreenRight = new THREE.Vector3();
const vCamPos = new THREE.Vector3();
const vLookAt = new THREE.Vector3();
const phoneTargetPos = new THREE.Vector3();
const phoneAimTarget = new THREE.Vector3();
const phoneTargetQuat = new THREE.Quaternion();
const phoneBaseQuat = new THREE.Quaternion();
const phoneAimQuat = new THREE.Quaternion();
const phoneLeanQuat = new THREE.Quaternion();
const phoneLeanEuler = new THREE.Euler(0, 0, 0, "XYZ");
const phoneUp = new THREE.Vector3(0, 1, 0);
const PHONE_GAIT_WALK_SPEED = phoneCharacter.movement.walkMaxSpeed;
const PHONE_GAIT_RUN_SPEED = phoneCharacter.movement.runMaxSpeed;
const PHONE_GAIT_WALK_PITCH = 0.34;
const PHONE_GAIT_RUN_PITCH = 0.88;
const PHONE_GAIT_WALK_ROLL = 0.24;
const PHONE_GAIT_RUN_ROLL = 0.66;
const PHONE_GAIT_WALK_YAW = 0.18;
const PHONE_GAIT_RUN_YAW = 0.52;
const PHONE_GAIT_FORWARD_OFFSET = 0.052;
const PHONE_GAIT_SIDE_OFFSET = 0.062;
const PHONE_GAIT_LIFT = 0.065;
const PHONE_GAIT_OBLIQUE_SWEEP = 0.58;
const PHONE_GAIT_CONE_TWIST = 0.34;
const PHONE_GAIT_OLOID_MEANDER = 0.045;
const PHONE_GAIT_CYLINDER_RADIUS = 0.36;


const PHONE_POSE_STATE = {
	IDLE: 0,
	LOCOMOTION: 1,
	VACUUMING: 2,
	DISCHARGING: 3,
	PROXIMITY_ATTACK: 4,
	DOUBLE_JUMP_FLIP: 5,
	FULL_WARNING: 6
};
const phonePose = {
	position: new THREE.Vector3(),
	quaternion: new THREE.Quaternion(),
	suppressGait: 0,
	state: PHONE_POSE_STATE.IDLE
};
const phonePoseRotQuat = new THREE.Quaternion();
const X_AXIS = new THREE.Vector3(1, 0, 0);
const Y_AXIS = new THREE.Vector3(0, 1, 0);
const Z_AXIS = new THREE.Vector3(0, 0, 1);
const phonePoseBasePos = new THREE.Vector3();

const phoneGait = {
	phase: 0,
	rollEnergy: 0,
	energy: 0,
	pitch: 0,
	roll: 0,
	yaw: 0,
	lift: 0,
	forward: 0,
	side: 0
};
function resetPhoneGait() {
	phoneGait.energy = 0;
	phoneGait.pitch = 0;
	phoneGait.roll = 0;
	phoneGait.yaw = 0;
	phoneGait.lift = 0;
	phoneGait.forward = 0;
	phoneGait.side = 0;
}
function resetPhonePoseFromCurrent() {
	phonePose.position.copy(phone.position);
	phonePose.quaternion.copy(phone.quaternion);
	phonePose.suppressGait = 0;
	phonePose.state = PHONE_POSE_STATE.IDLE;
}
function phonePoseRotateX(angle) {
	phonePoseRotQuat.setFromAxisAngle(X_AXIS, angle);
	phonePose.quaternion.multiply(phonePoseRotQuat);
}
function phonePoseRotateY(angle) {
	phonePoseRotQuat.setFromAxisAngle(Y_AXIS, angle);
	phonePose.quaternion.multiply(phonePoseRotQuat);
}
function phonePoseRotateZ(angle) {
	phonePoseRotQuat.setFromAxisAngle(Z_AXIS, angle);
	phonePose.quaternion.multiply(phonePoseRotQuat);
}
function applyPhonePose() {
	phone.position.copy(phonePose.position);
	phone.quaternion.copy(phonePose.quaternion);
}
const screenForward = new THREE.Vector3();
const proximityAttackOrigin = new THREE.Vector3();
const proximityAttackDir = new THREE.Vector3(0, 0, -1);
const proximityAttackImpact = new THREE.Vector3();
const proximityAttackSide = new THREE.Vector3(1, 0, 0);
const yAxis = new THREE.Vector3(0, 1, 0);

const proximitySlashMaterial = new THREE.MeshBasicMaterial({
	color: 0x8ff7ff,
	transparent: true,
	opacity: 0,
	depthWrite: false
});
const proximitySlashMesh = new THREE.Mesh(
	new THREE.TorusGeometry(0.52, 0.026, 8, 48, Math.PI * 1.35),
	proximitySlashMaterial
);
proximitySlashMesh.visible = false;
scene.add(proximitySlashMesh);

const proximityImpactMaterial = new THREE.MeshBasicMaterial({
	color: 0xffffff,
	transparent: true,
	opacity: 0,
	depthWrite: false
});
const proximityImpactRing = new THREE.Mesh(
	new THREE.TorusGeometry(0.34, 0.022, 8, 48),
	proximityImpactMaterial
);
proximityImpactRing.visible = false;
scene.add(proximityImpactRing);

const proximityStreakMaterial = new THREE.MeshBasicMaterial({
	color: 0x55d6ff,
	transparent: true,
	opacity: 0,
	depthWrite: false
});
const proximityStreakMesh = new THREE.Mesh(
	new THREE.CylinderGeometry(0.035, 0.11, 1, 12, 1, true),
	proximityStreakMaterial
);
proximityStreakMesh.visible = false;
scene.add(proximityStreakMesh);

const vacuumPullPoint = new THREE.Vector3();
const targetToVacuum = new THREE.Vector3();
const targetVisualPos = new THREE.Vector3();


const phone = new THREE.Group();
scene.add(phone);

const fallbackPhoneBody = new THREE.Mesh(
	new THREE.BoxGeometry(0.08, 0.16, 0.012),
	new THREE.MeshStandardMaterial({ color: 0xd0d0d0, metalness: 0.55, roughness: 0.25 })
);
fallbackPhoneBody.castShadow = true;
fallbackPhoneBody.receiveShadow = true;
phone.add(fallbackPhoneBody);

const screen = new THREE.Mesh(
	new THREE.PlaneGeometry(0.07, 0.125),
	new THREE.MeshStandardMaterial({
		color: 0x16202a,
		emissive: 0x12304a,
		emissiveIntensity: 0.75,
		transparent: true,
		opacity: 0.18,
		depthWrite: false
	})
);
screen.position.z = 0.007;
screen.receiveShadow = true;
phone.add(screen);

const screenLight = new THREE.RectAreaLight(0xffffff, 0, 0.08, 0.16);
screenLight.position.set(0, 0, 0.02);
screen.add(screenLight);

const cameraBump = new THREE.Mesh(
	new THREE.CylinderGeometry(0.009, 0.009, 0.004, 20),
	new THREE.MeshStandardMaterial({ color: 0x222222, metalness: 0.7, roughness: 0.25 })
);
cameraBump.rotation.x = Math.PI / 2;
cameraBump.position.set(-0.02, 0.055, -0.008);
cameraBump.castShadow = true;
phone.add(cameraBump);

const PLAYER_AVATAR_PHONE = "phone";
const playerAvatar = { type: PLAYER_AVATAR_PHONE };

function syncRoomIdentityPresentation() {
	playerAvatar.type = PLAYER_AVATAR_PHONE;
	updatePlayerAvatarVisibility();
}

function updatePlayerAvatarVisibility() {
	phone.visible = cameraMode.current !== CAMERA_MODE_FIRST;
}

function loadPhoneModel() {
	new GLTFLoader().parse(
		base64ToArrayBuffer(IPHONE_GLB_BASE64),
		"",
		(gltf) => {
			const model = gltf.scene;
			const box = new THREE.Box3().setFromObject(model);
			const size = new THREE.Vector3();
			const center = new THREE.Vector3();
			box.getSize(size);
			box.getCenter(center);
			const scale = IPHONE_MODEL_HEIGHT / Math.max(size.y, 0.0001);
			model.position.copy(center).multiplyScalar(-scale);
			model.scale.setScalar(scale);
			model.traverse((node) => {
				if (!node.isMesh) return;
				node.castShadow = true;
				node.receiveShadow = true;
				if (node.material) {
					node.material = node.material.clone();
					const name = node.material.name || "";
					if (name.includes("Screen_BG") || name.includes("Screen_Glass") || name.includes("Screen_Rim")) {
						phoneScreenMaterials.push(node.material);
						node.material.emissive = node.material.emissive || new THREE.Color(0x12304a);
						node.material.emissive.set(0x12304a);
						node.material.emissiveIntensity = name.includes("Screen_BG") ? 0.95 : 0.18;
					}
				}
			});
			phone.add(model);
			fallbackPhoneBody.visible = false;
			cameraBump.visible = false;
			setPhoneScreenEmission(0x12304a, 0.75);
		},
		(error) => {
			console.warn("iPhone GLB failed to load; using procedural fallback phone", error);
		}
	);
}

loadPhoneModel();
syncRoomIdentityPresentation();

function prepareFlowerPowerupModel(rawModel) {
	const model = rawModel.clone(true);
	const box = new THREE.Box3().setFromObject(model);
	const size = new THREE.Vector3();
	const center = new THREE.Vector3();
	box.getSize(size);
	box.getCenter(center);
	const scale = 0.72 / Math.max(size.x, size.y, size.z, 0.0001);
	model.position.copy(center).multiplyScalar(-scale);
	model.scale.setScalar(scale);
	model.traverse((node) => {
		if (!node.isMesh) return;
		node.castShadow = true;
		node.receiveShadow = true;
		if (node.material) {
			node.material = node.material.clone();
			node.material.emissive = node.material.emissive || new THREE.Color(0x152c20);
			node.material.emissive.set(0x16452e);
			node.material.emissiveIntensity = 0.18;
		}
	});
	return model;
}

function createFallbackFlowerPowerupModel() {
	const group = new THREE.Group();
	const material = new THREE.MeshStandardMaterial({ color: 0xcfffe8, emissive: 0x1f5c3a, emissiveIntensity: 0.22, roughness: 0.42, metalness: 0.12 });
	for (let i = 0; i < 5; i++) {
		const petal = new THREE.Mesh(new THREE.ConeGeometry(0.14, 0.38, 5), material);
		petal.position.set(Math.cos(i * Math.PI * 2 / 5) * 0.18, 0, Math.sin(i * Math.PI * 2 / 5) * 0.18);
		petal.rotation.z = Math.PI / 2;
		petal.rotation.y = -i * Math.PI * 2 / 5;
		petal.castShadow = true;
		petal.receiveShadow = true;
		group.add(petal);
	}
	const core = new THREE.Mesh(new THREE.DodecahedronGeometry(0.13), material);
	core.castShadow = true;
	core.receiveShadow = true;
	group.add(core);
	return group;
}

function loadFlowerPowerupModel() {
	new GLTFLoader().parse(
		base64ToArrayBuffer(PENTAGONAL_FLOWER_GLB_BASE64),
		"",
		(gltf) => {
			flowerPowerupPrototype = prepareFlowerPowerupModel(gltf.scene);
			scheduleFlowerPowerupPoolWarmup();
		},
		(error) => {
			console.warn("Flower powerup GLB failed to load; using fallback", error);
			flowerPowerupPrototype = createFallbackFlowerPowerupModel();
			scheduleFlowerPowerupPoolWarmup();
		}
	);
}

loadFlowerPowerupModel();

function createFlowerPowerupMesh() {
	if (!flowerPowerupPrototype) flowerPowerupPrototype = createFallbackFlowerPowerupModel();
	const mesh = flowerPowerupPrototype.clone(true);
	mesh.visible = false;
	mesh.position.set(0, -999, 0);
	return mesh;
}

function resetFlowerPowerupMesh(mesh) {
	if (!mesh) return;
	mesh.visible = false;
	mesh.position.set(0, -999, 0);
	mesh.rotation.set(0, 0, 0);
}

function warmOneFlowerPowerupMesh() {
	const mesh = createFlowerPowerupMesh();
	resetFlowerPowerupMesh(mesh);
	scene.add(mesh);
	flowerPowerupPool.push(mesh);
}

function scheduleFlowerPowerupPoolWarmup() {
	if (flowerPowerupPoolWarmupQueued) return;
	flowerPowerupPoolWarmupQueued = true;
	const warmStep = () => {
		let total = flowerPowerupPool.length + flowerPowerups.length;
		if (total < FLOWER_POWERUP_POOL_TARGET) {
			warmOneFlowerPowerupMesh();
			const schedule = window.requestIdleCallback || window.requestAnimationFrame;
			schedule(warmStep);
			return;
		}
		flowerPowerupPoolWarmupQueued = false;
	};
	const schedule = window.requestIdleCallback || window.requestAnimationFrame;
	schedule(warmStep);
}

function acquireFlowerPowerupMesh() {
	if (flowerPowerupPool.length) {
		const mesh = flowerPowerupPool.pop();
		mesh.visible = true;
		return mesh;
	}
	// Last-resort fallback: create one mesh if the pool has not warmed yet.
	const mesh = createFlowerPowerupMesh();
	scene.add(mesh);
	mesh.visible = true;
	return mesh;
}

function releaseFlowerPowerupMesh(mesh) {
	if (!mesh) return;
	resetFlowerPowerupMesh(mesh);
	if (!mesh.parent) scene.add(mesh);
	if (!flowerPowerupPool.includes(mesh)) flowerPowerupPool.push(mesh);
}

function acquireFlowerPowerupMirrorMesh() {
	if (flowerPowerupMirrorPool.length) {
		const mesh = flowerPowerupMirrorPool.pop();
		mesh.visible = true;
		return mesh;
	}
	const mesh = createFlowerPowerupMesh();
	scene.add(mesh);
	mesh.visible = true;
	return mesh;
}

function releaseFlowerPowerupMirrorMesh(mesh) {
	if (!mesh) return;
	resetFlowerPowerupMesh(mesh);
	if (!mesh.parent) scene.add(mesh);
	if (!flowerPowerupMirrorPool.includes(mesh)) flowerPowerupMirrorPool.push(mesh);
}

function isDropPositionClear(x, z, sourceX, sourceZ) {
	const sourceDx = x - sourceX;
	const sourceDz = z - sourceZ;
	if (sourceDx * sourceDx + sourceDz * sourceDz < FLOWER_DROP_MIN_SPACING * FLOWER_DROP_MIN_SPACING) return false;
	for (const powerup of flowerPowerups) {
		const dx = x - powerup.x;
		const dz = z - powerup.z;
		if (dx * dx + dz * dz < FLOWER_DROP_MIN_SPACING * FLOWER_DROP_MIN_SPACING) return false;
	}
	return true;
}

function resolveSeparatedDropPosition(x, z) {
	const baseAngle = Math.atan2(z - getPlayerLocalZ(), x - player.pos.x);
	const offsets = [0, 0.72, -0.72, 1.45, -1.45, Math.PI];
	for (let i = 0; i < offsets.length; i++) {
		const angle = baseAngle + offsets[i];
		const radius = FLOWER_DROP_SEPARATION_RADIUS + i * 0.12;
		const px = x + Math.cos(angle) * radius;
		const pz = z + Math.sin(angle) * radius;
		if (isDropPositionClear(px, pz, x, z)) return { x: px, z: pz };
	}
	return {
		x: x + FLOWER_DROP_SEPARATION_RADIUS,
		z
	};
}

function spawnFlowerPowerup(x, y, z) {
	if (!game.started || game.dead) return;
	const drop = resolveSeparatedDropPosition(x, z);
	const mesh = acquireFlowerPowerupMesh();
	mesh.position.set(drop.x, Math.max(FLOWER_POWERUP_GROUND_Y, y || FLOWER_POWERUP_GROUND_Y), drop.z);
	mesh.rotation.y = Math.random() * Math.PI * 2;
	mesh.visible = true;
	const continuationMeshes = ROOM_CONTINUATION_TILE_OFFSETS.map(() => acquireFlowerPowerupMirrorMesh());
	flowerPowerups.push({ mesh, continuationMeshes, x: drop.x, y: mesh.position.y, z: drop.z, age: 0, value: FLOWER_PICKUP_VALUE, type: "flower" });
	scheduleFlowerPowerupPoolWarmup();
}

function clearFlowerPowerups() {
	for (const powerup of flowerPowerups) {
		releaseFlowerPowerupMesh(powerup.mesh);
		if (powerup.continuationMeshes) {
			for (const mesh of powerup.continuationMeshes) releaseFlowerPowerupMirrorMesh(mesh);
		}
	}
	flowerPowerups.length = 0;
	scheduleFlowerPowerupPoolWarmup();
}

function updateFlowerPowerups(dt) {
	const tileOriginZ = getPlayerRoomTileOriginZ();
	const playerLocalZ = getPlayerLocalZ();
	for (let i = flowerPowerups.length - 1; i >= 0; i--) {
		const powerup = flowerPowerups[i];
		powerup.age += dt;
		if (powerup.mesh) {
			powerup.mesh.position.y = powerup.y + Math.sin(powerup.age * 3.2) * FLOWER_BOB_HEIGHT;
			powerup.mesh.position.z = powerup.z + tileOriginZ;
			powerup.mesh.rotation.y += dt * 1.35;
		}
		if (powerup.continuationMeshes) {
			for (let c = 0; c < powerup.continuationMeshes.length; c++) {
				const continuationMesh = powerup.continuationMeshes[c];
				if (!continuationMesh) continue;
				continuationMesh.visible = true;
				continuationMesh.position.set(
					powerup.x,
					powerup.mesh ? powerup.mesh.position.y : powerup.y,
					powerup.z + getContinuationTileOriginZ(ROOM_CONTINUATION_TILE_OFFSETS[c])
				);
				continuationMesh.rotation.y = powerup.mesh ? powerup.mesh.rotation.y : continuationMesh.rotation.y;
			}
		}
		const dx = powerup.x - player.pos.x;
		const dz = powerup.z - playerLocalZ;
		const dy = (powerup.mesh ? powerup.mesh.position.y : powerup.y) - player.pos.y;
		if (dx * dx + dz * dz + dy * dy <= FLOWER_PICKUP_RADIUS * FLOWER_PICKUP_RADIUS) {
			addPowerupStack(powerup.type || "flower", powerup.value);
			spawnParticleBurst(powerup.x, powerup.y + 0.2, powerup.z + tileOriginZ);
			releaseFlowerPowerupMesh(powerup.mesh);
			if (powerup.continuationMeshes) {
				for (const mesh of powerup.continuationMeshes) releaseFlowerPowerupMirrorMesh(mesh);
			}
			flowerPowerups.splice(i, 1);
			scheduleFlowerPowerupPoolWarmup();
		}
	}
}



const targetGeometry = new THREE.BoxGeometry(0.72, 0.72, 0.72);
const targetMaterial = new THREE.MeshStandardMaterial({
	color: 0x8ff7ff,
	emissive: 0x1d9cff,
	emissiveIntensity: 0.42,
	transparent: true,
	opacity: 0.68,
	roughness: 0.9,
	metalness: 0.0,
	depthWrite: false
});

const targetMesh = new THREE.InstancedMesh(targetGeometry, targetMaterial, TARGET_COUNT);
targetMesh.castShadow = true;
targetMesh.receiveShadow = true;
targetMesh.visible = true;
scene.add(targetMesh);



const strandGeometry = new THREE.BoxGeometry(0.13, 0.13, 1);
const strandMaterial = new THREE.MeshStandardMaterial({
	color: 0xffb7a6,
	emissive: 0x3a0d0a,
	emissiveIntensity: 0.16,
	transparent: true,
	opacity: 0.34,
	roughness: 0.62,
	metalness: 0.0,
	depthWrite: false
});
const strandMesh = new THREE.InstancedMesh(strandGeometry, strandMaterial, TARGET_COUNT);
scene.add(strandMesh);


const humanMaterial = new THREE.MeshStandardMaterial({
	color: 0x222a30,
	emissive: 0x05080a,
	emissiveIntensity: 0.02,
	roughness: 0.82,
	metalness: 0.0
});
const humanHeadMaterial = new THREE.MeshStandardMaterial({
	color: 0xd6b29e,
	emissive: 0x120706,
	emissiveIntensity: 0.025,
	roughness: 0.84,
	metalness: 0.0
});
const humanHeadMesh = new THREE.InstancedMesh(new THREE.SphereGeometry(0.14, 10, 8), humanHeadMaterial, TARGET_COUNT);
const humanTorsoMesh = new THREE.InstancedMesh(new THREE.BoxGeometry(0.26, 0.46, 0.14), humanMaterial, TARGET_COUNT);
const humanArmMesh = new THREE.InstancedMesh(new THREE.BoxGeometry(0.055, 0.36, 0.07), humanMaterial, TARGET_COUNT * 2);
const humanLegMesh = new THREE.InstancedMesh(new THREE.BoxGeometry(0.07, 0.40, 0.075), humanMaterial, TARGET_COUNT * 2);
humanHeadMesh.visible = false;
humanTorsoMesh.visible = false;
humanArmMesh.visible = false;
humanLegMesh.visible = false;
scene.add(humanHeadMesh, humanTorsoMesh, humanArmMesh, humanLegMesh);

const CONTINUATION_INSTANCE_COUNT = TARGET_COUNT * ROOM_CONTINUATION_TILE_OFFSETS.length;
const continuationTargetMesh = new THREE.InstancedMesh(targetGeometry, targetMaterial.clone(), CONTINUATION_INSTANCE_COUNT);
continuationTargetMesh.castShadow = true;
continuationTargetMesh.receiveShadow = true;
scene.add(continuationTargetMesh);
const continuationHumanHeadMesh = new THREE.InstancedMesh(new THREE.SphereGeometry(0.14, 10, 8), humanHeadMaterial.clone(), CONTINUATION_INSTANCE_COUNT);
const continuationHumanTorsoMesh = new THREE.InstancedMesh(new THREE.BoxGeometry(0.26, 0.46, 0.14), humanMaterial.clone(), CONTINUATION_INSTANCE_COUNT);
const continuationHumanArmMesh = new THREE.InstancedMesh(new THREE.BoxGeometry(0.055, 0.36, 0.07), humanMaterial.clone(), CONTINUATION_INSTANCE_COUNT * 2);
const continuationHumanLegMesh = new THREE.InstancedMesh(new THREE.BoxGeometry(0.07, 0.40, 0.075), humanMaterial.clone(), CONTINUATION_INSTANCE_COUNT * 2);
for (const mesh of [continuationHumanHeadMesh, continuationHumanTorsoMesh, continuationHumanArmMesh, continuationHumanLegMesh]) {
	mesh.castShadow = true;
	mesh.receiveShadow = true;
	scene.add(mesh);
}

const continuationMatrix = new THREE.Matrix4();

function setHiddenContinuationInstance(mesh, instance) {
	dummy.position.set(0, -999, 0);
	dummy.rotation.set(0, 0, 0);
	dummy.scale.setScalar(0);
	dummy.updateMatrix();
	mesh.setMatrixAt(instance, dummy.matrix);
}

function setContinuationHumanMirror(instance, x, z, yaw, scale, walkPhase) {
	const y = HUMAN_GROUND_Y;
	const swing = Math.sin(walkPhase * 0.16) * 0.46;
	setHumanPart(continuationHumanHeadMesh, instance, x, y, z, yaw, 1.0 * scale, 1.0 * scale, 1.0 * scale, 0, 1.08 * scale, 0, 0, 1);
	setHumanPart(continuationHumanTorsoMesh, instance, x, y, z, yaw, 1.0 * scale, 1.0 * scale, 1.0 * scale, 0, 0.68 * scale, 0, 0, 1);
	const limbBase = instance * 2;
	setHumanPart(continuationHumanArmMesh, limbBase, x, y, z, yaw, 1.0 * scale, 1.0 * scale, 1.0 * scale, -0.20, 0.68 * scale, 0.02, swing, -1);
	setHumanPart(continuationHumanArmMesh, limbBase + 1, x, y, z, yaw, 1.0 * scale, 1.0 * scale, 1.0 * scale, 0.20, 0.68 * scale, 0.02, -swing, 1);
	setHumanPart(continuationHumanLegMesh, limbBase, x, y, z, yaw, 1.0 * scale, 1.0 * scale, 1.0 * scale, -0.08, 0.22 * scale, 0.00, -swing * 0.72, -1);
	setHumanPart(continuationHumanLegMesh, limbBase + 1, x, y, z, yaw, 1.0 * scale, 1.0 * scale, 1.0 * scale, 0.08, 0.22 * scale, 0.00, swing * 0.72, 1);
}

function hideContinuationHumanMirror(instance) {
	setHiddenContinuationInstance(continuationHumanHeadMesh, instance);
	setHiddenContinuationInstance(continuationHumanTorsoMesh, instance);
	setHiddenContinuationInstance(continuationHumanArmMesh, instance * 2);
	setHiddenContinuationInstance(continuationHumanArmMesh, instance * 2 + 1);
	setHiddenContinuationInstance(continuationHumanLegMesh, instance * 2);
	setHiddenContinuationInstance(continuationHumanLegMesh, instance * 2 + 1);
}
function setHumanPart(mesh, instance, x, y, z, yaw, sx, sy, sz, ox, oy, oz, swing, side) {
	dummy.position.set(x, y + oy, z);
	dummy.rotation.set(0, yaw, 0);
	dummy.translateX(ox * side);
	dummy.translateZ(oz);
	dummy.rotateZ(swing * side);
	dummy.scale.set(sx, sy, sz);
	dummy.updateMatrix();
	mesh.setMatrixAt(instance, dummy.matrix);
}


/* Walking low-poly human model from Sketchfab: "Walk Cycle" by Niraj Ekaant, CC Attribution.
   Source: https://sketchfab.com/3d-models/walk-cycle-05c7560e49c1441aa0c70d3dc7bc710b */
// HUMAN_FBX_BASE64 moved to assets/embedded-assets.js

const HUMAN_MODEL_FORWARD_YAW_OFFSET = Math.PI;
const HUMAN_MODEL_HEIGHT = 1.16;
const HUMAN_MODEL_SCALE_MULT = 1.0;
const HUMAN_MODEL_MIXERS = [];
const HUMAN_MODEL_ROOTS = [];
const HUMAN_MODEL_POSE_BONES = [];
const CONTINUATION_HUMAN_MODEL_MIXERS = [];
const CONTINUATION_HUMAN_MODEL_ROOTS = [];
const CONTINUATION_HUMAN_MODEL_POSE_BONES = [];
let humanModelSource = null;
let humanModelSourceMinY = 0;
let humanModelUnitScale = 1;
let humanModelReady = false;

function base64ToArrayBuffer(base64) {
	const binary = atob(base64);
	const bytes = new Uint8Array(binary.length);
	for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
	return bytes.buffer;
}

function prepareHumanFbxModel() {
	try {
		humanModelSource = new FBXLoader().parse(base64ToArrayBuffer(HUMAN_FBX_BASE64), "");
		humanModelSource.traverse((node) => {
			if (node.isMesh) {
				node.frustumCulled = false;
				node.castShadow = true;
				node.receiveShadow = true;
				if (node.material) {
					const materials = Array.isArray(node.material) ? node.material : [node.material];
					for (const material of materials) {
						material.side = THREE.DoubleSide;
						material.needsUpdate = true;
					}
				}
			}
		});
		const box = new THREE.Box3().setFromObject(humanModelSource);
		const size = new THREE.Vector3();
		box.getSize(size);
		humanModelSourceMinY = Number.isFinite(box.min.y) ? box.min.y : 0;
		humanModelUnitScale = size.y > 0.001 ? HUMAN_MODEL_HEIGHT / size.y : 0.01;
		humanModelReady = true;
	} catch (error) {
		console.warn("FBX human model failed; falling back to sprite humans.", error);
		humanModelReady = false;
	}
}

function collectHumanPoseBones(model) {
	const bones = {
		spine: [],
		head: [],
		leftArm: [],
		rightArm: []
	};
	model.traverse((node) => {
		if (!node.isBone) return;
		const name = (node.name || "").toLowerCase();
		if (name.includes("spine") || name.includes("chest") || name.includes("torso")) bones.spine.push(node);
		if (name.includes("head") || name.includes("neck")) bones.head.push(node);
		if ((name.includes("arm") || name.includes("hand") || name.includes("forearm")) && (name.includes("left") || name.includes("_l") || name.endsWith(".l"))) bones.leftArm.push(node);
		if ((name.includes("arm") || name.includes("hand") || name.includes("forearm")) && (name.includes("right") || name.includes("_r") || name.endsWith(".r"))) bones.rightArm.push(node);
	});
	return bones;
}

function armBoneKind(bone) {
	const name = (bone.name || "").toLowerCase();
	if (name.includes("shoulder")) return "shoulder";
	if (name.includes("upper")) return "upper";
	if (name.includes("lower") || name.includes("forearm")) return "lower";
	if (name.includes("hand")) return "hand";
	return "arm";
}

function applyArmStrikePose(bone, side, power, variant, lead) {
	const kind = armBoneKind(bone);
	const swingVariant = variant % 2;
	const low = variant >= 2 ? 1 : 0;
	const reach = lead ? power : power * 0.38;
	const cross = swingVariant ? -1 : 1;

	if (kind === "shoulder") {
		bone.rotation.x += lead ? -reach * (0.42 + low * 0.16) : reach * 0.14;
		bone.rotation.y += side * cross * reach * (0.58 + low * 0.12);
		bone.rotation.z += side * reach * (0.36 + low * 0.10);
		return;
	}

	if (kind === "upper") {
		bone.rotation.x += lead ? -reach * (1.35 + low * 0.28) : reach * 0.24;
		bone.rotation.y += side * cross * reach * (0.82 + low * 0.18);
		bone.rotation.z += side * reach * (0.62 + low * 0.18);
		return;
	}

	if (kind === "lower") {
		bone.rotation.x += lead ? -reach * (0.92 - low * 0.18) : reach * 0.18;
		bone.rotation.y += side * cross * reach * 0.34;
		bone.rotation.z += side * reach * (0.22 + low * 0.12);
		return;
	}

	if (kind === "hand") {
		bone.rotation.x += lead ? -reach * 0.34 : reach * 0.08;
		bone.rotation.y += side * cross * reach * 0.30;
		bone.rotation.z += side * reach * 0.52;
		return;
	}

	bone.rotation.x += lead ? -reach * 0.70 : reach * 0.14;
	bone.rotation.z += side * reach * 0.28;
}

function applyHumanAttackPose(root, bones, attackTimer, variant) {
	if (!root) return;
	const t = attackTimer > 0 ? 1 - THREE.MathUtils.clamp(attackTimer / HUMAN_ATTACK_DURATION, 0, 1) : 1;
	const windup = Math.sin(THREE.MathUtils.clamp(t / 0.28, 0, 1) * Math.PI) * (t < 0.35 ? 1 : 0);
	const strike = Math.sin(THREE.MathUtils.clamp((t - 0.18) / 0.38, 0, 1) * Math.PI);
	const recover = Math.sin(THREE.MathUtils.clamp((t - 0.48) / 0.52, 0, 1) * Math.PI);
	const impact = Math.max(strike, recover * 0.35);
	const side = variant % 2 === 0 ? 1 : -1;
	const low = variant >= 2 ? 1 : 0;

	root.rotation.z += side * (strike * 0.18 - windup * 0.08);
	root.rotation.x += -strike * (0.16 + low * 0.08) + windup * 0.08;
	root.position.y += Math.sin(t * Math.PI) * 0.035 * low;

	if (!bones) return;

	for (const b of bones.spine) {
		b.rotation.x += -impact * (0.30 + low * 0.08) + windup * 0.12;
		b.rotation.y += side * impact * 0.18;
		b.rotation.z += side * impact * 0.16;
	}
	for (const b of bones.head) {
		b.rotation.x += -impact * 0.12;
		b.rotation.y += side * impact * 0.10;
		b.rotation.z += side * impact * 0.08;
	}

	const leadArm = side > 0 ? bones.rightArm : bones.leftArm;
	const backArm = side > 0 ? bones.leftArm : bones.rightArm;
	const strikePower = Math.max(0, strike * 1.55 + recover * 0.25);
	const windupPower = windup * 0.9;

	for (const b of leadArm) {
		b.rotation.x += windupPower * 0.55;
		b.rotation.y += -side * windupPower * 0.65;
		b.rotation.z += -side * windupPower * 0.55;
		applyArmStrikePose(b, side, strikePower, variant, true);
	}
	for (const b of backArm) {
		b.rotation.x += -windupPower * 0.20;
		b.rotation.y += side * windupPower * 0.25;
		b.rotation.z += side * windupPower * 0.24;
		applyArmStrikePose(b, -side, strikePower, variant, false);
	}
}

function createHumanModelInstance() {
	if (!humanModelReady || !humanModelSource) return null;
	const root = new THREE.Group();
	const model = cloneSkeleton(humanModelSource);
	model.position.y = -humanModelSourceMinY * humanModelUnitScale;
	model.scale.setScalar(humanModelUnitScale);
	root.add(model);
	root.visible = false;
	scene.add(root);
	const mixer = new THREE.AnimationMixer(model);
	if (humanModelSource.animations && humanModelSource.animations.length) {
		const action = mixer.clipAction(humanModelSource.animations[0]);
		action.play();
	}
	return { root, mixer, bones: collectHumanPoseBones(model) };
}

function updateHumanModelFromPools(roots, mixers, poseBones, i, visible, x, z, yaw, scale, dt, attackTimer = 0, attackVariant = 0) {
	const root = roots[i];
	const mixer = mixers[i];
	const bones = poseBones[i];
	if (!root) return false;
	root.visible = visible;
	if (!visible) return true;
	root.position.set(x, HUMAN_VISUAL_FOOT_Y, z);
	root.rotation.set(0, yaw + HUMAN_MODEL_FORWARD_YAW_OFFSET, 0);
	root.scale.setScalar(scale * HUMAN_MODEL_SCALE_MULT);
	if (mixer) mixer.update(dt);
	if (attackTimer > 0) applyHumanAttackPose(root, bones, attackTimer, attackVariant);
	return true;
}

function updateHumanModelInstance(i, visible, x, z, yaw, scale, dt, attackTimer = 0, attackVariant = 0) {
	return updateHumanModelFromPools(HUMAN_MODEL_ROOTS, HUMAN_MODEL_MIXERS, HUMAN_MODEL_POSE_BONES, i, visible, x, z, yaw, scale, dt, attackTimer, attackVariant);
}

function updateContinuationHumanModelInstance(i, visible, x, z, yaw, scale, dt, attackTimer = 0, attackVariant = 0) {
	return updateHumanModelFromPools(CONTINUATION_HUMAN_MODEL_ROOTS, CONTINUATION_HUMAN_MODEL_MIXERS, CONTINUATION_HUMAN_MODEL_POSE_BONES, i, visible, x, z, yaw, scale, dt, attackTimer, attackVariant);
}

/* PMC sprites face screen-right by default; updateTargets flips sprite.scale.x from screen-space walking direction.
Human sprites from OpenGameArt: "PMC Contractor Animation" by Stagnation, CC-BY 3.0.
   Source: https://opengameart.org/content/pmc-contractor-animation */
// PMC_RUN_FRAME_DATA moved to assets/embedded-assets.js


const pmcTextureLoader = new THREE.TextureLoader();
function makePmcTexture(dataUri) {
	const texture = pmcTextureLoader.load(dataUri);
	texture.colorSpace = THREE.SRGBColorSpace;
	texture.magFilter = THREE.NearestFilter;
	texture.minFilter = THREE.NearestFilter;
	texture.generateMipmaps = false;
	return texture;
}

const humanSpriteMaterials = PMC_RUN_FRAME_DATA.map((uri) => new THREE.SpriteMaterial({
	map: makePmcTexture(uri),
	transparent: true,
	alphaTest: 0.12,
	depthWrite: false,
	sizeAttenuation: true
}));
const humanSprites = [];
for (let i = 0; i < TARGET_COUNT; i++) {
	const sprite = new THREE.Sprite(humanSpriteMaterials[0]);
	sprite.visible = false;
	sprite.renderOrder = 2;
	humanSprites.push(sprite);
	scene.add(sprite);
}
const continuationHumanSprites = [];
for (let i = 0; i < CONTINUATION_INSTANCE_COUNT; i++) {
	const sprite = new THREE.Sprite(humanSpriteMaterials[0]);
	sprite.visible = false;
	sprite.renderOrder = 2;
	continuationHumanSprites.push(sprite);
	scene.add(sprite);
}
prepareHumanFbxModel();
for (let i = 0; i < TARGET_COUNT; i++) {
	const instance = createHumanModelInstance();
	HUMAN_MODEL_ROOTS.push(instance ? instance.root : null);
	HUMAN_MODEL_MIXERS.push(instance ? instance.mixer : null);
	HUMAN_MODEL_POSE_BONES.push(instance ? instance.bones : null);
}
for (let i = 0; i < CONTINUATION_INSTANCE_COUNT; i++) {
	const instance = createHumanModelInstance();
	CONTINUATION_HUMAN_MODEL_ROOTS.push(instance ? instance.root : null);
	CONTINUATION_HUMAN_MODEL_MIXERS.push(instance ? instance.mixer : null);
	CONTINUATION_HUMAN_MODEL_POSE_BONES.push(instance ? instance.bones : null);
}

const LATTICE_SIDE = 3;
const LATTICE_NODE_COUNT = LATTICE_SIDE * LATTICE_SIDE * LATTICE_SIDE;
const LATTICE_STRIDE = 3;
const LATTICE_TOTAL_NODES = TARGET_COUNT * LATTICE_NODE_COUNT;
const LATTICE_SPACING = 0.23;
const LATTICE_HALF = ((LATTICE_SIDE - 1) * LATTICE_SPACING) / 2;

const latticeRest = [];
const latticePairs = [];
const latticeSurface = new Float32Array(LATTICE_NODE_COUNT);
const latticeCorner = new Float32Array(LATTICE_NODE_COUNT);
const latticeFaceSeed = new Float32Array(LATTICE_NODE_COUNT);

function latticeIndex(x, y, z) {
	return x + y * LATTICE_SIDE + z * LATTICE_SIDE * LATTICE_SIDE;
}

for (let z = 0; z < LATTICE_SIDE; z++) {
	for (let y = 0; y < LATTICE_SIDE; y++) {
		for (let x = 0; x < LATTICE_SIDE; x++) {
			const px = x * LATTICE_SPACING - LATTICE_HALF;
			const py = y * LATTICE_SPACING - LATTICE_HALF;
			const pz = z * LATTICE_SPACING - LATTICE_HALF;
			const id = latticeIndex(x, y, z);
			latticeRest[id] = new THREE.Vector3(px, py, pz);
			const onSurface = x === 0 || x === LATTICE_SIDE - 1 || y === 0 || y === LATTICE_SIDE - 1 || z === 0 || z === LATTICE_SIDE - 1;
			const cornerScore =
				(x === 0 || x === LATTICE_SIDE - 1 ? 1 : 0) +
				(y === 0 || y === LATTICE_SIDE - 1 ? 1 : 0) +
				(z === 0 || z === LATTICE_SIDE - 1 ? 1 : 0);
			latticeSurface[id] = onSurface ? 1 : 0;
			latticeCorner[id] = cornerScore / 3;
			latticeFaceSeed[id] = Math.sin((id + 1) * 12.9898) * 43758.5453 % 1;
			if (latticeFaceSeed[id] < 0) latticeFaceSeed[id] += 1;
		}
	}
}

for (let z = 0; z < LATTICE_SIDE; z++) {
	for (let y = 0; y < LATTICE_SIDE; y++) {
		for (let x = 0; x < LATTICE_SIDE; x++) {
			const a = latticeIndex(x, y, z);
			if (x < LATTICE_SIDE - 1) latticePairs.push([a, latticeIndex(x + 1, y, z)]);
			if (y < LATTICE_SIDE - 1) latticePairs.push([a, latticeIndex(x, y + 1, z)]);
			if (z < LATTICE_SIDE - 1) latticePairs.push([a, latticeIndex(x, y, z + 1)]);
		}
	}
}

const LATTICE_RODS_PER_TARGET = latticePairs.length;
const LATTICE_TOTAL_RODS = TARGET_COUNT * LATTICE_RODS_PER_TARGET;

const latticePos = new Float32Array(LATTICE_TOTAL_NODES * LATTICE_STRIDE);
const latticeVel = new Float32Array(LATTICE_TOTAL_NODES * LATTICE_STRIDE);
const targetVisualPull = new Float32Array(TARGET_COUNT);
const targetVisualPullVel = new Float32Array(TARGET_COUNT);
const targetIngestProgress = new Float32Array(TARGET_COUNT);
const targetLatchedToScreen = new Float32Array(TARGET_COUNT);

const SOUL = Object.freeze({
	FREE: 0,
	ATTRACTED: 1,
	LATCHED: 2,
	INGESTING: 3,
	RECOILING: 4
});
const SOUL_FREE = SOUL.FREE;
const SOUL_ATTRACTED = SOUL.ATTRACTED;
const SOUL_LATCHED = SOUL.LATCHED;
const SOUL_INGESTING = SOUL.INGESTING;
const SOUL_RECOILING = SOUL.RECOILING;
const soulState = new Uint8Array(TARGET_COUNT);
const soulRecoilTime = new Float32Array(TARGET_COUNT);
const soulDepositShot = new Uint8Array(TARGET_COUNT);
const soulDepositNearMissPlayed = new Uint8Array(TARGET_COUNT);
const soulCaptureCommitted = new Uint8Array(TARGET_COUNT);
const soulCaptureQueued = new Uint8Array(TARGET_COUNT);
const SOUL_MORPH_DURATION = 0.72;
const soulMorphTime = new Float32Array(TARGET_COUNT);
const soulCaptureQueue = [];

function targetIndex(i) {
	return i * TARGET_STRIDE;
}

function setSoulState(i, state) {
	soulState[i] = state;
}

function isSoulCapturing(i) {
	return soulState[i] === SOUL_LATCHED || soulState[i] === SOUL_INGESTING;
}

function isSoulUnavailable(i) {
	return soulCaptureQueued[i] || soulCaptureCommitted[i];
}

function isTargetSlurpable(idx) {
	return targetData[idx + T_SLURPABLE] > 0.5;
}

function clearSoulMotion(idx) {
	targetData[idx + T_VX] = 0;
	targetData[idx + T_VY] = 0;
	targetData[idx + T_VZ] = 0;
}

function clearSoulCaptureFlags(i) {
	soulRecoilTime[i] = 0;
	soulCaptureCommitted[i] = 0;
	soulCaptureQueued[i] = 0;
	targetIngestProgress[i] = 0;
	targetLatchedToScreen[i] = 0;
}

const SOUL_ATTRACTION_RANGE = 15.5;
const SOUL_ATTRACTION_CONE_RADIUS = 2.35;
const SOUL_CAPTURE_CYLINDER_RADIUS = 1.75;
const SOUL_CAPTURE_CYLINDER_HEIGHT = 2.25;
const SOUL_LATCH_DISTANCE = 0.48;
const SOUL_SEAL_DISTANCE = 0.14;
const SOUL_RECOIL_DURATION = 0.55;
const SOUL_CAPTURE_COMMIT_PHASE = 0.92;

const SCREEN_HALF_WIDTH = 0.07 * 0.5;
const SCREEN_HALF_HEIGHT = 0.125 * 0.5;
const SCREEN_FRONT_OFFSET = 0.018;
const SOUL_SEAL_BODY_OFFSET = 0.36;

const PHONE_SOLID_HALF_X = 0.08 * 0.5;
const PHONE_SOLID_HALF_Y = 0.16 * 0.5;
const PHONE_SOLID_HALF_Z = 0.012 * 0.5;
const SOUL_CORE_SOLID_RADIUS = 0.33;
const PHONE_SOLID_VISUAL_PADDING = 0.012;

const soulToScreen = new THREE.Vector3();
const soulPoint = new THREE.Vector3();
const soulRecoilDir = new THREE.Vector3();
const screenLocalPoint = new THREE.Vector3();
const screenPlaneCenter = new THREE.Vector3();
const screenPlaneFront = new THREE.Vector3();
const screenPlaneNormal = new THREE.Vector3();
const shootOrigin = new THREE.Vector3();
const shootDir = new THREE.Vector3();
const phoneSolidLocal = new THREE.Vector3();
const phoneSolidWorld = new THREE.Vector3();
const firstPersonVacuumRight = new THREE.Vector3();
const firstPersonVacuumUp = new THREE.Vector3();
const firstPersonSlurpCameraLocal = new THREE.Vector3();

function updateFirstPersonVacuumPullPoint() {
	// In first person the phone/screen is hidden. Pull latched souls through the
	// camera plane to a point behind the eye instead of down toward the floor.
	if (cameraMode.current !== CAMERA_MODE_FIRST) {
		vacuumPullPoint.copy(screenPlaneCenter);
		return;
	}
	camera.getWorldDirection(shotDir);
	shotDir.normalize();
	vacuumPullPoint.copy(camera.position)
		.addScaledVector(shotDir, -0.85);
}

function updateScreenPlaneBasis() {
	screen.localToWorld(screenPlaneCenter.set(0, 0, 0));
	screen.localToWorld(screenPlaneFront.set(0, 0, 1));
	screenPlaneNormal.copy(screenPlaneFront).sub(screenPlaneCenter);
	if (screenPlaneNormal.lengthSq() < 0.001) {
		screenPlaneNormal.set(0, 0, 1);
	} else {
		screenPlaneNormal.normalize();
	}
}

function getScreenLatchPoint(out, worldPoint) {
	screenLocalPoint.copy(worldPoint);
	screen.worldToLocal(screenLocalPoint);
	screenLocalPoint.x = THREE.MathUtils.clamp(
		screenLocalPoint.x,
		-SCREEN_HALF_WIDTH * 0.92,
		SCREEN_HALF_WIDTH * 0.92
	);
	screenLocalPoint.y = THREE.MathUtils.clamp(
		screenLocalPoint.y,
		-SCREEN_HALF_HEIGHT * 0.92,
		SCREEN_HALF_HEIGHT * 0.92
	);
	screenLocalPoint.z = SCREEN_FRONT_OFFSET + SOUL_SEAL_BODY_OFFSET;
	return screen.localToWorld(out.copy(screenLocalPoint));
}

function keepSoulCoreOutsidePhoneSolid(idx, preferScreenFront = false) {
	const soulWorldZ = getNearestRepeatedWorldZ(targetData[idx + T_Z], player.pos.z);
	phoneSolidWorld.set(
		targetData[idx + T_X],
		targetData[idx + T_Y],
		soulWorldZ
	);
	phone.worldToLocal(phoneSolidLocal.copy(phoneSolidWorld));

	const hx = PHONE_SOLID_HALF_X + SOUL_CORE_SOLID_RADIUS;
	const hy = PHONE_SOLID_HALF_Y + SOUL_CORE_SOLID_RADIUS;
	const hz = PHONE_SOLID_HALF_Z + SOUL_CORE_SOLID_RADIUS;

	if (
		Math.abs(phoneSolidLocal.x) > hx ||
		Math.abs(phoneSolidLocal.y) > hy ||
		Math.abs(phoneSolidLocal.z) > hz
	) {
		return false;
	}

	if (preferScreenFront) {
		phoneSolidLocal.z = hz;
	} else {
		const px = hx - Math.abs(phoneSolidLocal.x);
		const py = hy - Math.abs(phoneSolidLocal.y);
		const pz = hz - Math.abs(phoneSolidLocal.z);
		if (px <= py && px <= pz) {
			phoneSolidLocal.x = Math.sign(phoneSolidLocal.x || 1) * hx;
		} else if (py <= px && py <= pz) {
			phoneSolidLocal.y = Math.sign(phoneSolidLocal.y || 1) * hy;
		} else {
			phoneSolidLocal.z = Math.sign(phoneSolidLocal.z || 1) * hz;
		}
	}

	phone.localToWorld(phoneSolidWorld.copy(phoneSolidLocal));
	targetData[idx + T_X] = phoneSolidWorld.x;
	targetData[idx + T_Y] = phoneSolidWorld.y;
	writeTargetWorldZCanonical(idx, phoneSolidWorld.z);
	targetData[idx + T_VX] = 0;
	targetData[idx + T_VY] = 0;
	targetData[idx + T_VZ] = 0;
	return true;
}

function isWorldPointInsidePhoneSolid(worldPoint, padding = 0) {
	phone.worldToLocal(phoneSolidLocal.copy(worldPoint));
	return (
		Math.abs(phoneSolidLocal.x) <= PHONE_SOLID_HALF_X + padding &&
		Math.abs(phoneSolidLocal.y) <= PHONE_SOLID_HALF_Y + padding &&
		Math.abs(phoneSolidLocal.z) <= PHONE_SOLID_HALF_Z + padding
	);
}

function resetSoulPointState(i) {
	setSoulState(i, SOUL_FREE);
	clearSoulCaptureFlags(i);
}

function markSoulSlurpable(i, idx) {
	if (isTargetSlurpable(idx)) return;
	const wasBrute = (targetData[idx + T_KIND] || 0) === 1;
	targetData[idx + T_SLURPABLE] = 1;
	soulMorphTime[i] = 0;
	targetData[idx + T_HITFLASH] = 1.35;
	const px = targetData[idx + T_X];
	const py = targetData[idx + T_RENDER_Y] || targetData[idx + T_Y];
	const pz = targetData[idx + T_Z];
	spawnRoomParticleBurst(px, py, pz);
	if (wasBrute && Math.random() < FLOWER_DROP_CHANCE) spawnFlowerPowerup(px, HUMAN_VISUAL_FOOT_Y + 0.42, pz);
}

function damageSoulShell(i, amount, hitX, hitY, hitZ) {
	if (isSoulUnavailable(i)) return false;
	const idx = targetIndex(i);
	if (targetData[idx + T_ALIVE] <= 0) return false;
	if (isSoulCapturing(i)) return false;

	if (!isTargetSlurpable(idx)) {
		targetData[idx + T_ARMOR] -= amount;
		targetData[idx + T_HITFLASH] = 1;
		if (targetData[idx + T_ARMOR] <= 0) {
			targetData[idx + T_ARMOR] = 0;
			markSoulSlurpable(i, idx);
		}
	} else {
		targetData[idx + T_HITFLASH] = Math.max(targetData[idx + T_HITFLASH], 0.75);
	}

	const awayX = targetData[idx + T_X] - player.pos.x;
	const awayZ = targetData[idx + T_Z] - getPlayerLocalZ();
	const awayLen = Math.max(Math.hypot(awayX, awayZ), 0.001);
	targetData[idx + T_VX] += (awayX / awayLen) * 2.4;
	targetData[idx + T_VY] = Math.max(targetData[idx + T_VY], 1.2);
	targetData[idx + T_VZ] += (awayZ / awayLen) * 2.4;
	spawnRoomParticleBurst(hitX, hitY, hitZ);
	feedSupplementalBattery(FLOWER_ATTACK_FEED, "ATTACK");
	return true;
}

const proximityAttackForward = new THREE.Vector3();
const proximityAttackToSoul = new THREE.Vector3();

function triggerProximityAttack() {
	if (!game.started || game.dead || proximityAttack.cooldown > 0) return;
	const now = pulseTime();
	if (now - proximityAttack.lastTriggerTime <= PROX_ATTACK_COMBO_WINDOW) {
		proximityAttack.comboIndex = (proximityAttack.comboIndex + 1) % PROX_ATTACK_COMBOS.length;
	} else {
		proximityAttack.comboIndex = 0;
	}
	proximityAttack.lastTriggerTime = now;
	const combo = getProximityAttackCombo();
	if (!spendBattery(combo.cost ?? BATTERY_MELEE_COST, "MELEE")) return;
	playEventSound("phoneAttack", 0.44);
	proximityAttack.cooldown = combo.cooldown ?? PROX_ATTACK_COOLDOWN;
	proximityAttack.pose = 1;
	proximityAttack.visualDuration = combo.visual ?? PROX_ATTACK_VISUAL_DURATION;
	proximityAttack.visualTimer = proximityAttack.visualDuration;
	proximityAttack.dashTimer = combo.dash ?? PROX_ATTACK_DASH_DURATION;
	proximityAttack.travel = 0;
	proximityAttack.visualHit = false;
	proximityAttack.variant = combo.variant ?? ((proximityAttack.variant + 1) % PROX_ATTACK_VARIANTS.length);
	proximityAttackHitTargets.clear();

	proximityAttackForward.set(-Math.sin(cursor.yaw), 0, -Math.cos(cursor.yaw)).normalize();
	proximityAttackDir.copy(proximityAttackForward);
	proximityAttackOrigin.copy(player.pos).addScaledVector(proximityAttackDir, 0.22);
	proximityAttackOrigin.y = player.pos.y + 0.42;
	proximityAttackImpact.copy(proximityAttackOrigin).addScaledVector(proximityAttackDir, combo.range ?? PROX_ATTACK_RANGE);
	applyProximityAttackHits();
}


function showComboPop(hitCount) {
	// Combo numbers are rendered by battery event math now.
}

function spawnFlameBurst(x, y, z, strength = 1) {
	if (!particlesRuntimeEnabled()) return;
	const count = Math.floor(CHAIN_FLAME_COUNT * THREE.MathUtils.clamp(strength, 0.7, 2.2));
	for (let n = 0; n < count; n++) {
		const i = particleState.next;
		particleState.next = (particleState.next + 1) % PARTICLE_COUNT;
		const idx = i * PARTICLE_STRIDE;
		const angle = Math.random() * Math.PI * 2;
		const radial = 1.2 + Math.random() * 5.4 * strength;
		const life = 0.42 + Math.random() * 0.32;
		particleData[idx + P_X] = x;
		particleData[idx + P_Y] = y + 0.06;
		particleData[idx + P_Z] = z;
		particleData[idx + P_VX] = Math.cos(angle) * radial;
		particleData[idx + P_VY] = 2.4 + Math.random() * 5.0 * strength;
		particleData[idx + P_VZ] = Math.sin(angle) * radial;
		particleData[idx + P_LIFE] = life;
		particleData[idx + P_MAXLIFE] = life;
	}
}

function spawnChainLineEffect(hitPoints) {
	if (!hitPoints || hitPoints.length < 2) return;
	for (let i = 0; i < hitPoints.length - 1; i++) {
		const a = hitPoints[i];
		const b = hitPoints[i + 1];
		for (let n = 0; n < 8; n++) {
			const t = n / 8;
			const x = THREE.MathUtils.lerp(a.x, b.x, t);
			const y = THREE.MathUtils.lerp(a.y, b.y, t) + Math.sin(t * Math.PI) * 0.18;
			const z = THREE.MathUtils.lerp(a.z, b.z, t);
			spawnFlameBurst(x, y, z, 0.22);
		}
	}
}

function applyProximityAttackHits() {
	const playerLocalZ = getPlayerLocalZ();
	let didHit = false;
	let hitCount = 0;
	const hitPoints = [];
	const combo = getProximityAttackCombo();
	for (let i = 0; i < TARGET_COUNT; i++) {
		if (proximityAttackHitTargets.has(i)) continue;
		const idx = targetIndex(i);
		if (targetData[idx + T_ALIVE] <= 0) continue;
		if (isSoulUnavailable(i)) continue;

		const dx = targetData[idx + T_X] - player.pos.x;
		const dz = targetData[idx + T_Z] - playerLocalZ;
		const forwardDist = dx * proximityAttackDir.x + dz * proximityAttackDir.z;
		const chainRangeBonus = batteryComboActive() ? Math.min(0.75, battery.comboHits * 0.025) : 0;
		if (forwardDist < -0.35 || forwardDist > (combo.range ?? PROX_ATTACK_RANGE) + chainRangeBonus) continue;

		const sideX = dx - proximityAttackDir.x * forwardDist;
		const sideZ = dz - proximityAttackDir.z * forwardDist;
		const sideDistSq = sideX * sideX + sideZ * sideZ;
		const kind = targetData[idx + T_KIND] || 0;
		const chainWidthBonus = batteryComboActive() ? Math.min(0.55, battery.comboHits * 0.018) : 0;
		const hitRadius = (combo.hitRadius ?? PROX_ATTACK_HIT_RADIUS) + chainWidthBonus + (kind === 1 ? 0.28 : 0);
		if (sideDistSq > hitRadius * hitRadius) continue;

		const hitX = targetData[idx + T_X];
		const hitY = targetData[idx + T_RENDER_Y] || targetData[idx + T_Y];
		const hitLocalZ = targetData[idx + T_Z];
		const hitWorldZ = hitLocalZ + getPlayerRoomTileOriginZ();
		proximityAttackHitTargets.add(i);
		proximityAttack.visualHit = true;
		proximityAttackImpact.set(hitX, hitY, hitWorldZ);
		const hitDamage = (combo.damage ?? PROX_ATTACK_DAMAGE) * (1 + Math.min(0.75, hitCount * 0.12));
		if (damageSoulShell(i, hitDamage, hitX, hitY, hitLocalZ)) {
			hitCount++;
			hitPoints.push({ x: hitX, y: hitY, z: hitWorldZ });
			const flameStrength = hitCount > 1 ? 0.95 + hitCount * 0.18 : 0.55;
			spawnFlameBurst(hitX, hitY, hitWorldZ, flameStrength);
			didHit = true;
		}
	}
	if (didHit) {
		registerMeleeBatteryHit(hitCount);
		if (hitCount > 1) {
			chainFxState.lastMultiHitTime = pulseTime();
			chainFxState.lastMultiHitCount = hitCount;
			spawnChainLineEffect(hitPoints);
		}
		if (proximityAttack.dashTimer > 0) {
			const recoilScale = hitCount > 1 ? 0.35 : 1.0;
			proximityAttack.dashTimer = hitCount > 1 ? proximityAttack.dashTimer * 0.35 : 0;
			proximityAttack.travel = Math.min(proximityAttack.travel, combo.range ?? PROX_ATTACK_RANGE);
			player.pos.addScaledVector(proximityAttackDir, -(combo.recoilDistance ?? PROX_ATTACK_RECOIL_DISTANCE) * recoilScale);
			player.vel.addScaledVector(proximityAttackDir, -(combo.recoilSpeed ?? PROX_ATTACK_RECOIL_SPEED) * recoilScale);
			player.vel.x *= hitCount > 1 ? 0.82 : 0.55;
			player.vel.z *= hitCount > 1 ? 0.82 : 0.55;
		}
		flashScreen();
	}
}
function updateProximityAttackDash(dt) {
	if (proximityAttack.dashTimer <= 0) return;
	const combo = getProximityAttackCombo();
	const range = combo.range ?? PROX_ATTACK_RANGE;
	const step = Math.min((combo.dashSpeed ?? PROX_ATTACK_DASH_SPEED) * dt, range - proximityAttack.travel);
	proximityAttack.dashTimer = Math.max(0, proximityAttack.dashTimer - dt);
	proximityAttack.travel += Math.max(0, step);
	player.pos.addScaledVector(proximityAttackDir, step);
	player.vel.x *= 0.55;
	player.vel.z *= 0.55;
	proximityAttackOrigin.copy(player.pos).addScaledVector(proximityAttackDir, 0.22);
	proximityAttackOrigin.y = player.pos.y + 0.42;
	proximityAttackImpact.copy(proximityAttackOrigin).addScaledVector(proximityAttackDir, range * 0.72);
	applyProximityAttackHits();
}

function smooth01(t) {
	t = THREE.MathUtils.clamp(t, 0, 1);
	return t * t * (3 - 2 * t);
}

function isSoulInsideScreenCaptureCylinder(x, y, z) {
	const dx = x - phone.position.x;
	const dz = z - phone.position.z;
	const horizontalDistSq = dx * dx + dz * dz;
	const dy = Math.abs(y - phone.position.y);
	return (
		horizontalDistSq <= SOUL_CAPTURE_CYLINDER_RADIUS * SOUL_CAPTURE_CYLINDER_RADIUS &&
		dy <= SOUL_CAPTURE_CYLINDER_HEIGHT * 0.5
	);
}

function isSoulInAttractionOffer(x, y, z) {
	soulPoint.set(x, y, z);
	const toSoul = soulToScreen.copy(soulPoint).sub(shotOrigin);
	const forwardDistance = toSoul.dot(shotDir);
	if (forwardDistance <= 0 || forwardDistance > SOUL_ATTRACTION_RANGE) return false;
	const ray = new THREE.Ray(shotOrigin, shotDir);
	const coneRadius = SOUL_ATTRACTION_CONE_RADIUS * (0.24 + forwardDistance / SOUL_ATTRACTION_RANGE);
	return ray.distanceSqToPoint(soulPoint) <= coneRadius * coneRadius;
}

function queueSoulCapture(i, idx, x, y, z) {
	if (isSoulUnavailable(i)) return;
	soulCaptureQueued[i] = 1;
	targetIngestProgress[i] = Math.max(targetIngestProgress[i], SOUL_CAPTURE_COMMIT_PHASE);
	soulCaptureQueue.push({ i, x, y, z });
}

function processQueuedSoulCaptures() {
	if (soulCaptureQueue.length === 0) return;
	for (const capture of soulCaptureQueue) {
		completeTargetCapture(capture.i, capture.x, capture.y, capture.z);
	}
	soulCaptureQueue.length = 0;
}

function releaseSoulFromScreen(i, idx) {
	const phase = THREE.MathUtils.clamp(targetIngestProgress[i], 0, 1);
	if (phase >= SOUL_CAPTURE_COMMIT_PHASE) {
		queueSoulCapture(
			i,
			idx,
			targetData[idx + T_X],
			targetData[idx + T_RENDER_Y] || targetData[idx + T_Y],
			targetData[idx + T_Z]
		);
		return;
	}
	const worldZ = getNearestRepeatedWorldZ(targetData[idx + T_Z], vacuumPullPoint.z);
	soulPoint.set(
		targetData[idx + T_X],
		targetData[idx + T_RENDER_Y] || targetData[idx + T_Y],
		worldZ
	);
	soulRecoilDir.copy(soulPoint).sub(vacuumPullPoint);
	if (soulRecoilDir.lengthSq() < 0.001) {
		soulRecoilDir.set(
			Math.sin(cursor.yaw),
			0.18,
			Math.cos(cursor.yaw)
		);
	}
	soulRecoilDir.normalize();
	const recoil = 4.5 + phase * 10.5;
	targetData[idx + T_VX] = soulRecoilDir.x * recoil;
	targetData[idx + T_VY] = 1.4 + phase * 2.2;
	targetData[idx + T_VZ] = soulRecoilDir.z * recoil;
	if (phase > 0.015) {
		playEventSound("endCallTone", 0.34);
	}
	targetIngestProgress[i] = 0;
	targetLatchedToScreen[i] = 0;
	resetSoulVisualDeformation(i);
	setSoulState(i, SOUL_RECOILING);
	soulRecoilTime[i] = SOUL_RECOIL_DURATION;
}
const targetPrevVisualTarget = new Float32Array(TARGET_COUNT);

const fleshNodeMaterial = new THREE.MeshStandardMaterial({
	color: 0xe0a08f,
	emissive: 0x2f0907,
	emissiveIntensity: 0.08,
	roughness: 0.88,
	metalness: 0.0
});
const latticeNodeGeometry = new THREE.SphereGeometry(0.075, 12, 8);
const latticeNodeMesh = new THREE.InstancedMesh(latticeNodeGeometry, fleshNodeMaterial, LATTICE_TOTAL_NODES);
scene.add(latticeNodeMesh);

const fleshRodMaterial = new THREE.MeshStandardMaterial({
	color: 0xc98275,
	emissive: 0x260705,
	emissiveIntensity: 0.06,
	roughness: 0.92,
	metalness: 0.0
});
const latticeRodGeometry = new THREE.BoxGeometry(0.045, 0.045, 1);
const latticeRodMesh = new THREE.InstancedMesh(latticeRodGeometry, fleshRodMaterial, LATTICE_TOTAL_RODS);
latticeNodeMesh.visible = false;
latticeRodMesh.visible = false;
scene.add(latticeRodMesh);

const surfaceNodeIds = [];
const surfaceNodeToVertex = new Int16Array(LATTICE_NODE_COUNT);
surfaceNodeToVertex.fill(-1);
for (let n = 0; n < LATTICE_NODE_COUNT; n++) {
	if (latticeSurface[n] > 0.5) {
		surfaceNodeToVertex[n] = surfaceNodeIds.length;
		surfaceNodeIds.push(n);
	}
}

const surfaceIndices = [];
function addSurfaceQuad(a, b, c, d) {
	const ai = surfaceNodeToVertex[a];
	const bi = surfaceNodeToVertex[b];
	const ci = surfaceNodeToVertex[c];
	const di = surfaceNodeToVertex[d];
	if (ai < 0 || bi < 0 || ci < 0 || di < 0) return;
	surfaceIndices.push(ai, bi, ci, ai, ci, di);
}

for (let y = 0; y < LATTICE_SIDE - 1; y++) {
	for (let z = 0; z < LATTICE_SIDE - 1; z++) {
		addSurfaceQuad(latticeIndex(0, y, z), latticeIndex(0, y + 1, z), latticeIndex(0, y + 1, z + 1), latticeIndex(0, y, z + 1));
		addSurfaceQuad(latticeIndex(LATTICE_SIDE - 1, y, z), latticeIndex(LATTICE_SIDE - 1, y, z + 1), latticeIndex(LATTICE_SIDE - 1, y + 1, z + 1), latticeIndex(LATTICE_SIDE - 1, y + 1, z));
	}
}
for (let x = 0; x < LATTICE_SIDE - 1; x++) {
	for (let z = 0; z < LATTICE_SIDE - 1; z++) {
		addSurfaceQuad(latticeIndex(x, 0, z), latticeIndex(x, 0, z + 1), latticeIndex(x + 1, 0, z + 1), latticeIndex(x + 1, 0, z));
		addSurfaceQuad(latticeIndex(x, LATTICE_SIDE - 1, z), latticeIndex(x + 1, LATTICE_SIDE - 1, z), latticeIndex(x + 1, LATTICE_SIDE - 1, z + 1), latticeIndex(x, LATTICE_SIDE - 1, z + 1));
	}
}
for (let x = 0; x < LATTICE_SIDE - 1; x++) {
	for (let y = 0; y < LATTICE_SIDE - 1; y++) {
		addSurfaceQuad(latticeIndex(x, y, 0), latticeIndex(x + 1, y, 0), latticeIndex(x + 1, y + 1, 0), latticeIndex(x, y + 1, 0));
		addSurfaceQuad(latticeIndex(x, y, LATTICE_SIDE - 1), latticeIndex(x, y + 1, LATTICE_SIDE - 1), latticeIndex(x + 1, y + 1, LATTICE_SIDE - 1), latticeIndex(x + 1, y, LATTICE_SIDE - 1));
	}
}

const fleshSurfaceMaterial = new THREE.MeshPhysicalMaterial({
	color: 0xe0a08f,
	emissive: 0x2b0806,
	emissiveIntensity: 0.035,
	roughness: 0.72,
	metalness: 0.0,
	clearcoat: 0.35,
	clearcoatRoughness: 0.55,
	side: THREE.DoubleSide,
	flatShading: false
});

const targetSurfaceMeshes = [];
const targetSurfacePositions = [];
for (let i = 0; i < TARGET_COUNT; i++) {
	const positions = new Float32Array(surfaceNodeIds.length * 3);
	for (let v = 0; v < surfaceNodeIds.length; v++) {
		const rest = latticeRest[surfaceNodeIds[v]];
		positions[v * 3] = rest.x;
		positions[v * 3 + 1] = rest.y;
		positions[v * 3 + 2] = rest.z;
	}
	const geometry = new THREE.BufferGeometry();
	geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
	geometry.setIndex(surfaceIndices);
	geometry.computeVertexNormals();
	const mesh = new THREE.Mesh(geometry, fleshSurfaceMaterial);
	mesh.frustumCulled = false;
	scene.add(mesh);
	targetSurfaceMeshes.push(mesh);
	targetSurfacePositions.push(positions);
}

const springNetCenter = new THREE.Vector3();
const springNetPullDir = new THREE.Vector3();
const springNetSide = new THREE.Vector3();
const springNetUp = new THREE.Vector3(0, 1, 0);
const springNetRest = new THREE.Vector3();
const springNetDesired = new THREE.Vector3();
const springNetCurrent = new THREE.Vector3();
const springNetWorld = new THREE.Vector3();
const springNetA = new THREE.Vector3();
const springNetB = new THREE.Vector3();
const springNetAnchor = new THREE.Vector3();
const springNetAnchorSum = new THREE.Vector3();
const springNetTmp = new THREE.Vector3();
const springNetSurfaceProbe = new THREE.Vector3();
const springNetSurfaceDir = new THREE.Vector3();

function springScalarValue(value, velocity, target, frequency, damping, dt) {
	const omega = frequency * Math.PI * 2;
	const f = 1 + 2 * dt * damping * omega;
	const oo = omega * omega;
	const hoo = dt * oo;
	const hhoo = dt * hoo;
	const detInv = 1 / (f + hhoo);
	const newValue = (f * value + dt * velocity + hhoo * target) * detInv;
	const newVelocity = (velocity + hoo * (target - value)) * detInv;
	return [newValue, newVelocity];
}


function resetSoulVisualDeformation(i) {
	targetVisualPull[i] = 0;
	targetVisualPullVel[i] = 0;
	targetPrevVisualTarget[i] = 0;
	soulMorphTime[i] = 0;
	for (let n = 0; n < LATTICE_NODE_COUNT; n++) {
		const nodeBase = (i * LATTICE_NODE_COUNT + n) * LATTICE_STRIDE;
		const r = latticeRest[n];
		latticePos[nodeBase] = r.x;
		latticePos[nodeBase + 1] = r.y;
		latticePos[nodeBase + 2] = r.z;
		latticeVel[nodeBase] = 0;
		latticeVel[nodeBase + 1] = 0;
		latticeVel[nodeBase + 2] = 0;
	}
}

function resetTargetLattice(i) {
	resetSoulPointState(i);
	targetVisualPull[i] = 0;
	targetVisualPullVel[i] = 0;
	targetIngestProgress[i] = 0;
	targetLatchedToScreen[i] = 0;
	targetPrevVisualTarget[i] = 0;
	for (let n = 0; n < LATTICE_NODE_COUNT; n++) {
		const nodeBase = (i * LATTICE_NODE_COUNT + n) * LATTICE_STRIDE;
		const r = latticeRest[n];
		latticePos[nodeBase] = r.x;
		latticePos[nodeBase + 1] = r.y;
		latticePos[nodeBase + 2] = r.z;
		latticeVel[nodeBase] = 0;
		latticeVel[nodeBase + 1] = 0;
		latticeVel[nodeBase + 2] = 0;
	}
}

const SYMBOLS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';

function chooseHumanWalkTarget(i, originX, originZ) {
	const idx = targetIndex(i);
	for (let attempt = 0; attempt < 10; attempt++) {
		const angle = Math.random() * Math.PI * 2;
		const radius = 1.8 + Math.random() * HUMAN_WALK_RANGE;
		const tx = clampHumanWalkTargetX(originX + Math.cos(angle) * radius);
		const tz = clampHumanWalkTargetZ(originZ + Math.sin(angle) * radius);
		if (!isRoomPointBlocked(tx, tz, 0.5)) {
			targetData[idx + T_WALK_TX] = tx;
			targetData[idx + T_WALK_TZ] = tz;
			return;
		}
	}
	targetData[idx + T_WALK_TX] = clampHumanWalkTargetX(originX);
	targetData[idx + T_WALK_TZ] = clampHumanWalkTargetZ(originZ);
}

function canHumanWalk(i) {
	if (soulDepositShot[i]) return false;
	const idx = targetIndex(i);
	return targetData[idx + T_ALIVE] > 0.5 && !isTargetSlurpable(idx) && soulState[i] === SOUL_FREE;
}

function applyHumanAttackHit(i, dx, dz, dist) {
	const idx = targetIndex(i);
	if (targetData[idx + T_ATTACK_HIT] > 0.5) return;
	if (dist > HUMAN_ATTACK_HIT_RANGE) return;
	const inv = dist > 0.001 ? 1 / dist : 1;
	const awayX = -dx * inv;
	const awayZ = -dz * inv;
	player.vel.x += awayX * HUMAN_ATTACK_KNOCKBACK;
	player.vel.z += awayZ * HUMAN_ATTACK_KNOCKBACK;
	spendBattery(BATTERY_ENEMY_HIT_COST, "HIT");
	phoneStorage.lastDischargePulseTime = pulseTime();
	flashScreen();
	targetData[idx + T_ATTACK_HIT] = 1;
}

function updateHumanWalking(dt) {
	const playerLocalZ = getPlayerLocalZ();
	for (let i = 0; i < TARGET_COUNT; i++) {
		if (!canHumanWalk(i)) continue;
		const idx = targetIndex(i);
		targetData[idx + T_Y] = HUMAN_GROUND_Y;
		targetData[idx + T_RENDER_Y] = HUMAN_GROUND_Y;
		targetData[idx + T_ATTACK_COOLDOWN] = Math.max(0, targetData[idx + T_ATTACK_COOLDOWN] - dt);
		let x = targetData[idx + T_X];
		let z = targetData[idx + T_Z];
		let toPlayerX = player.pos.x - x;
		let toPlayerZ = playerLocalZ - z;
		let playerDist = Math.hypot(toPlayerX, toPlayerZ);
		if (playerDist > 0.001 && playerDist < HUMAN_ATTACK_NOTICE_RANGE) {
			targetData[idx + T_WALK_YAW] = Math.atan2(-toPlayerX / playerDist, -toPlayerZ / playerDist);
		}
		if (targetData[idx + T_ATTACK_TIMER] > 0) {
			targetData[idx + T_ATTACK_TIMER] = Math.max(0, targetData[idx + T_ATTACK_TIMER] - dt);
			const attackProgress = 1 - THREE.MathUtils.clamp(targetData[idx + T_ATTACK_TIMER] / HUMAN_ATTACK_DURATION, 0, 1);
			if (attackProgress >= HUMAN_ATTACK_ACTIVE_TIME / HUMAN_ATTACK_DURATION) applyHumanAttackHit(i, toPlayerX, toPlayerZ, playerDist);
			continue;
		}
		if (playerDist < HUMAN_ATTACK_START_RANGE && targetData[idx + T_ATTACK_COOLDOWN] <= 0) {
			targetData[idx + T_ATTACK_TIMER] = HUMAN_ATTACK_DURATION;
			targetData[idx + T_ATTACK_COOLDOWN] = HUMAN_ATTACK_COOLDOWN;
			targetData[idx + T_ATTACK_VARIANT] = (targetData[idx + T_ATTACK_VARIANT] + 1) % 4;
			targetData[idx + T_ATTACK_HIT] = 0;
			continue;
		}
		let dx;
		let dz;
		if (playerDist < HUMAN_ATTACK_NOTICE_RANGE && playerDist > HUMAN_ATTACK_START_RANGE * 0.88) {
			dx = toPlayerX;
			dz = toPlayerZ;
		} else {
			dx = targetData[idx + T_WALK_TX] - x;
			dz = targetData[idx + T_WALK_TZ] - z;
		}
		let dist = Math.hypot(dx, dz);
		if (dist < HUMAN_WALK_TARGET_RADIUS && playerDist >= HUMAN_ATTACK_NOTICE_RANGE) {
			chooseHumanWalkTarget(i, x, z);
			dx = targetData[idx + T_WALK_TX] - x;
			dz = targetData[idx + T_WALK_TZ] - z;
			dist = Math.hypot(dx, dz);
		}
		if (dist > 0.001) {
			dx /= dist;
			dz /= dist;
			const kind = targetData[idx + T_KIND] || 0;
			const aggro = playerDist < HUMAN_ATTACK_NOTICE_RANGE ? 1.28 : 1;
			const speed = HUMAN_WALK_SPEED * aggro * (kind === 1 ? 0.56 : 1) * (0.82 + 0.18 * Math.sin(i * 12.9898));
			const step = Math.min(dist, speed * dt);
			const nx = x + dx * step;
			const nz = z + dz * step;
			if (isRoomPointBlocked(nx, nz, 0.42)) {
				chooseHumanWalkTarget(i, x, z);
			} else {
				x = nx;
				z = nz;
			}
			targetData[idx + T_X] = x;
			targetData[idx + T_Z] = z;
			targetData[idx + T_WALK_YAW] = Math.atan2(-dx, -dz);
			targetData[idx + T_WALK_PHASE] += step * 7.5;
		}
	}
}

function clearTargetSlot(i) {
	const idx = targetIndex(i);
	targetData[idx + T_X] = 0;
	targetData[idx + T_Y] = -999;
	targetData[idx + T_Z] = 0;
	targetData[idx + T_RENDER_Y] = -999;
	targetData[idx + T_FLOAT] = 0;
	targetData[idx + T_SPIN] = 0;
	targetData[idx + T_SCALE] = 0;
	targetData[idx + T_KIND] = 0;
	targetData[idx + T_ALIVE] = 0;
	targetData[idx + T_HEALTH] = 0;
	targetData[idx + T_ARMOR] = 0;
	targetData[idx + T_SLURPABLE] = 0;
	targetData[idx + T_HITFLASH] = 0;
	targetData[idx + T_VX] = 0;
	targetData[idx + T_VY] = 0;
	targetData[idx + T_VZ] = 0;
	targetData[idx + T_ATTACK_TIMER] = 0;
	targetData[idx + T_ATTACK_COOLDOWN] = 0;
	targetData[idx + T_ATTACK_HIT] = 0;
	targetIngestProgress[i] = 0;
	targetLatchedToScreen[i] = 0;
	soulCaptureQueued[i] = 0;
	soulCaptureCommitted[i] = 0;
	soulDepositShot[i] = 0;
	soulDepositNearMissPlayed[i] = 0;
	soulMorphTime[i] = 0;
	setSoulState(i, SOUL_FREE);
	resetSoulPointState(i);
	resetSoulVisualDeformation(i);
	resetTargetLattice(i);
}

function chooseHumanSpawnPoint(i, avoidPoint = null) {
	const playerLocalZ = getPlayerLocalZ();
	let best = null;
	let bestScore = -Infinity;
	for (let tries = 0; tries < 32; tries++) {
		const candidate = randomRoomSpawnPoint(i + tries * 17 + room.index * 31);
		const dxPlayer = candidate.x - player.pos.x;
		const dzPlayer = candidate.z - playerLocalZ;
		const playerDistSq = dxPlayer * dxPlayer + dzPlayer * dzPlayer;
		let oldDistSq = Infinity;
		if (avoidPoint) {
			const dxOld = candidate.x - avoidPoint.x;
			const dzOld = candidate.z - avoidPoint.z;
			oldDistSq = dxOld * dxOld + dzOld * dzOld;
		}
		let overlapPenalty = 0;
		for (let h = 0; h < TARGET_COUNT; h++) {
			if (h === i) continue;
			const hidx = targetIndex(h);
			if (targetData[hidx + T_ALIVE] <= 0 || isTargetSlurpable(hidx) || soulDepositShot[h]) continue;
			const dx = candidate.x - targetData[hidx + T_X];
			const dz = candidate.z - targetData[hidx + T_Z];
			const dSq = dx * dx + dz * dz;
			if (dSq < 6.25) overlapPenalty += 100;
		}
		const score = Math.min(playerDistSq, 144) + Math.min(oldDistSq, 144) - overlapPenalty;
		if (playerDistSq >= HUMAN_RESPAWN_MIN_PLAYER_DIST * HUMAN_RESPAWN_MIN_PLAYER_DIST && oldDistSq >= HUMAN_RESPAWN_MIN_OLD_SOUL_DIST * HUMAN_RESPAWN_MIN_OLD_SOUL_DIST && overlapPenalty <= 0) return candidate;
		if (score > bestScore) {
			bestScore = score;
			best = candidate;
		}
	}
	return best || randomRoomSpawnPoint(i);
}

function resetTarget(i, spawnPoint = null) {
	const idx = targetIndex(i);
	const roomSpawn = spawnPoint || chooseHumanSpawnPoint(i);
	targetData[idx + T_X] = roomSpawn.x;
	targetData[idx + T_Y] = HUMAN_GROUND_Y;
	targetData[idx + T_RENDER_Y] = HUMAN_GROUND_Y;
	targetData[idx + T_Z] = roomSpawn.z;
	targetData[idx + T_FLOAT] = Math.random() * Math.PI * 2;
	targetData[idx + T_SPIN] = 0.4 + Math.random() * 0.8;
	targetData[idx + T_ALIVE] = 1;
	targetData[idx + T_SCALE] = 1;
	targetData[idx + T_HITFLASH] = 0;
	targetData[idx + T_HEALTH] = 1;
	targetData[idx + T_ARMOR] = SOUL_ARMOR_NORMAL;
	targetData[idx + T_SLURPABLE] = 0;
	targetData[idx + T_CHAR] = Math.floor(Math.random() * SYMBOLS.length);
	targetData[idx + T_VX] = 0;
	targetData[idx + T_VY] = 0;
	targetData[idx + T_VZ] = 0;
	targetData[idx + T_WALK_YAW] = Math.random() * Math.PI * 2;
	targetData[idx + T_WALK_PHASE] = Math.random() * Math.PI * 2;
	targetData[idx + T_WALK_FACE] = Math.random() < 0.5 ? -1 : 1;
	targetData[idx + T_ATTACK_TIMER] = 0;
	targetData[idx + T_ATTACK_COOLDOWN] = Math.random() * 0.5;
	targetData[idx + T_ATTACK_VARIANT] = Math.floor(Math.random() * 4);
	targetData[idx + T_ATTACK_HIT] = 0;
	chooseHumanWalkTarget(i, targetData[idx + T_X], targetData[idx + T_Z]);
	targetData[idx + T_KIND] = Math.random() < 0.18 ? 1 : 0;

	if (targetData[idx + T_KIND] === 1) {
		targetData[idx + T_HEALTH] = 2.8;
		targetData[idx + T_ARMOR] = SOUL_ARMOR_BRUTE;
		targetData[idx + T_SCALE] = 1.7;
	}

	soulCaptureQueued[i] = 0;
	soulCaptureCommitted[i] = 0;
	soulDepositShot[i] = 0;
	soulMorphTime[i] = 0;
	setSoulState(i, SOUL_FREE);
	resetSoulPointState(i);
	resetSoulVisualDeformation(i);
	resetTargetLattice(i);
}

function countActiveHumans() {
	let count = 0;
	for (let i = 0; i < TARGET_COUNT; i++) {
		const idx = targetIndex(i);
		if (targetData[idx + T_ALIVE] > 0.5 && !isTargetSlurpable(idx) && !soulDepositShot[i] && soulState[i] === SOUL_FREE) count++;
	}
	return count;
}

function queueHumanRespawnFromSoul(i, x, y, z) {
	if (room.doorOpen) return;
	roomHumanRespawnQueue.push({ delay: THREE.MathUtils.lerp(HUMAN_RESPAWN_DELAY_MIN, HUMAN_RESPAWN_DELAY_MAX, Math.random()), avoid: { x, y, z } });
}

function updateRoomPopulation(dt) {
	if (room.doorOpen) {
		roomHumanRespawnQueue.length = 0;
		return;
	}
	for (let q = roomHumanRespawnQueue.length - 1; q >= 0; q--) {
		roomHumanRespawnQueue[q].delay -= dt;
	}
	let activeHumans = countActiveHumans();
	for (let q = roomHumanRespawnQueue.length - 1; q >= 0 && activeHumans < getActiveHumanTarget(); q--) {
		const request = roomHumanRespawnQueue[q];
		if (request.delay > 0) continue;
		let slot = -1;
		for (let i = 0; i < TARGET_COUNT; i++) {
			const idx = targetIndex(i);
			if (targetData[idx + T_ALIVE] <= 0 && !soulDepositShot[i] && soulState[i] === SOUL_FREE && !soulCaptureQueued[i] && !soulCaptureCommitted[i]) {
				slot = i;
				break;
			}
		}
		if (slot === -1) break;
		resetTarget(slot, chooseHumanSpawnPoint(slot, request.avoid));
		roomHumanRespawnQueue.splice(q, 1);
		activeHumans++;
	}
}

for (let i = 0; i < TARGET_COUNT; i++) {
	if (i < getActiveHumanTarget()) resetTarget(i);
	else clearTargetSlot(i);
}

function updateCamera() {
	// Wrapped-room topology no longer uses portal camera compression or
	// crossing handoff. Keep camera follow constants stable so boot/runtime
	// do not reference removed portal transition state.
	const thirdPersonDistance = 3;
	const thirdPersonHeight = 1.1;
	const lookLift = 0.45;
	const firstPersonEyeHeight = 0.72;
	const firstPersonForwardOffset = 0.18;
	const minCameraY = GROUND_Y + 0.8;
	vAimDir.set(
		-Math.sin(cursor.yaw) * Math.cos(cursor.pitch),
		Math.sin(cursor.pitch),
		-Math.cos(cursor.yaw) * Math.cos(cursor.pitch)
	).normalize();
	if (cameraMode.current === CAMERA_MODE_FIRST) {
		vCamPos.copy(player.pos);
		vCamPos.y += firstPersonEyeHeight;
		vCamPos.addScaledVector(vAimDir, firstPersonForwardOffset);
		camera.position.copy(vCamPos);
		vLookAt.copy(camera.position).addScaledVector(vAimDir, 10);
		camera.lookAt(vLookAt);
		updatePlayerAvatarVisibility();
		return;
	}
	vCameraOffset.copy(vAimDir).multiplyScalar(-thirdPersonDistance);
	vCameraOffset.y += thirdPersonHeight;
	vCamPos.copy(player.pos).add(vCameraOffset);
	if (vCamPos.y < minCameraY) {
		vCamPos.y = minCameraY;
	}
	constrainThirdPersonCamera(vCamPos, player.pos);
	camera.position.copy(vCamPos);
	vLookAt.copy(player.pos).addScaledVector(vAimDir, 10);
	vLookAt.y += lookLift;
	camera.lookAt(vLookAt);
	updatePlayerAvatarVisibility();
}

const particleGeometry = new THREE.BoxGeometry(0.08, 0.08, 0.08);
const particleMaterial = new THREE.MeshBasicMaterial({
	color: 0xff4444,
	transparent: true,
	opacity: 0.9
});
const particleMesh = new THREE.InstancedMesh(particleGeometry, particleMaterial, PARTICLE_COUNT);
scene.add(particleMesh);


function chooseDischargedSoulSlot() {
	// Discharged souls reuse the packed targetData pool, but they must only take
	// fully hidden/free slots. Falling back to slot 0 can overwrite a live human or
	// an in-flight capture and corrupt the shared target/soul pipeline.
	for (let i = 0; i < TARGET_COUNT; i++) {
		const idx = targetIndex(i);
		const hidden = targetData[idx + T_ALIVE] <= 0 && targetData[idx + T_SCALE] <= 0.001;
		const idle = soulState[i] === SOUL_FREE && !soulDepositShot[i] && !soulCaptureQueued[i] && !soulCaptureCommitted[i] && targetLatchedToScreen[i] <= 0;
		if (hidden && idle) return i;
	}
	return -1;
}

function spawnDischargedSoul(cube) {
	if (!cube) return false;
	const slot = chooseDischargedSoulSlot();
	if (slot < 0) return false;
	const idx = targetIndex(slot);

	updateScreenPlaneBasis();
	shootOrigin.copy(screenPlaneCenter);

	camera.getWorldDirection(shootDir);
	shootDir.normalize();
	shootDir.y = THREE.MathUtils.clamp(
		shootDir.y,
		DISCHARGE_MAX_DOWN_AIM,
		DISCHARGE_MAX_UP_AIM
	);
	shootDir.normalize();

	const spawnPos = screenPlaneCenter.clone().addScaledVector(screenPlaneNormal, SCREEN_FRONT_OFFSET + 0.28);
	spawnPos.y = Math.max(spawnPos.y, 0.95);
	const speed = cube.kind === 1 ? DISCHARGE_BRUTE_LAUNCH_SPEED : DISCHARGE_LAUNCH_SPEED;

	resetSoulPointState(slot);

	targetData[idx + T_X] = spawnPos.x;
	targetData[idx + T_Y] = spawnPos.y;
	writeTargetWorldZInPlayerTile(idx, spawnPos.z);
	targetData[idx + T_FLOAT] = Math.random() * Math.PI * 2;
	targetData[idx + T_SPIN] = 0.7 + Math.random() * 1.2;
	targetData[idx + T_ALIVE] = 1;
	targetData[idx + T_SCALE] = cube.kind === 1 ? 1.7 : 1.0;
	targetData[idx + T_HITFLASH] = 1.25;
	targetData[idx + T_CHAR] = Math.floor(Math.random() * SYMBOLS.length);
	targetData[idx + T_RENDER_Y] = spawnPos.y;
	targetData[idx + T_HEALTH] = cube.kind === 1 ? 2.8 : 1.0;
	targetData[idx + T_ARMOR] = 0;
	targetData[idx + T_SLURPABLE] = 1;
	targetData[idx + T_VX] = shootDir.x * speed;
	targetData[idx + T_VY] = shootDir.y * speed + DISCHARGE_VERTICAL_LIFT;
	targetData[idx + T_VZ] = shootDir.z * speed;
	targetData[idx + T_WALK_YAW] = Math.atan2(-shootDir.x, -shootDir.z);
	targetData[idx + T_WALK_PHASE] = 0;
	chooseHumanWalkTarget(slot, spawnPos.x, spawnPos.z);
	targetData[idx + T_KIND] = cube.kind || 0;

	resetTargetLattice(slot);
	resetSoulVisualDeformation(slot);
	soulMorphTime[slot] = SOUL_MORPH_DURATION;
	setSoulState(slot, SOUL_RECOILING);
	soulDepositShot[slot] = 1;
	soulDepositNearMissPlayed[slot] = 0;
	soulRecoilTime[slot] = DISCHARGE_MAX_LIFETIME;

	playEventSound("sentMessage", 0.62);
	spawnParticleBurst(spawnPos.x, spawnPos.y, spawnPos.z);
	return true;
}

function triggerDischargePose() {
	actionPose.dischargeTimer = Math.max(actionPose.dischargeTimer, DISCHARGE_POSE_HOLD);
	actionPose.discharge = 1;
}

function fireStoredCube() {
	if (phoneStorage.storedCubes.length <= 0) return;
	if (!spendBattery(BATTERY_SHOOT_COST, "DISCHARGE")) return;
	triggerDischargePose();

	const cube = phoneStorage.storedCubes.pop();
	phoneStorage.lastDischargePulseTime = pulseTime();
	shootState.impulseTime = phoneStorage.lastDischargePulseTime;
	shootState.cursorJoinTime = frame.now || performance.now();
	syncStoredMirror();
	updateStoredSoulDisplays();

	pendingDischargedSouls.push({ cube, age: 0 });

	phoneStorage.isFull = false;
}


function processPendingDischargedSouls(dt) {
	if (pendingDischargedSouls.length === 0) return;

	const poseReady = actionPose.screenForwardTurn > 0.45;
	for (let i = pendingDischargedSouls.length - 1; i >= 0; i--) {
		const pending = pendingDischargedSouls[i];
		pending.age += dt;

		if (!poseReady && pending.age < DISCHARGE_POSE_RELEASE_DELAY) continue;

		if (spawnDischargedSoul(pending.cube)) {
			pendingDischargedSouls.splice(i, 1);
		} else if (pending.age > 0.75) {
			// If the target pool is temporarily saturated, do not corrupt a live slot.
			// Return the cube to storage and let the player try again later.
			phoneStorage.storedCubes.push(pending.cube);
			syncStoredMirror();
			updateStoredSoulDisplays();
			pendingDischargedSouls.splice(i, 1);
		}
	}
}


function spawnParticleBurst(x, y, z) {
	if (!particlesRuntimeEnabled()) return;
	for (let n = 0; n < 22; n++) {
		const i = particleState.next;
		particleState.next = (particleState.next + 1) % PARTICLE_COUNT;
		const idx = i * PARTICLE_STRIDE;
		const life = 0.55 + Math.random() * 0.35;
		particleData[idx + P_X] = x;
		particleData[idx + P_Y] = y;
		particleData[idx + P_Z] = z;
		particleData[idx + P_VX] = (Math.random() - 0.5) * 5;
		particleData[idx + P_VY] = Math.random() * 4;
		particleData[idx + P_VZ] = (Math.random() - 0.5) * 5;
		particleData[idx + P_LIFE] = life;
		particleData[idx + P_MAXLIFE] = life;
	}
}

function spawnRoomParticleBurst(x, y, localZ) {
	spawnParticleBurst(x, y, localZ + getPlayerRoomTileOriginZ());
}

function updateProximityAttackVisuals(dt) {
	if (proximityAttack.visualTimer <= 0) {
		proximitySlashMesh.visible = false;
		proximityImpactRing.visible = false;
		proximityStreakMesh.visible = false;
		proximitySlashMaterial.opacity = 0;
		proximityImpactMaterial.opacity = 0;
		proximityStreakMaterial.opacity = 0;
		return;
	}

	proximityAttack.visualTimer = Math.max(0, proximityAttack.visualTimer - dt);
	const t = 1 - THREE.MathUtils.clamp(proximityAttack.visualTimer / proximityAttack.visualDuration, 0, 1);
	const strike = Math.sin(t * Math.PI);
	const fade = 1 - t;
	const hitBoost = proximityAttack.visualHit ? 1.25 : 0.72;

	proximityAttackSide.set(proximityAttackDir.z, 0, -proximityAttackDir.x);
	if (proximityAttackSide.lengthSq() < 0.001) proximityAttackSide.set(1, 0, 0);
	proximityAttackSide.normalize();

	const attackVariant = PROX_ATTACK_VARIANTS[proximityAttack.variant] || PROX_ATTACK_VARIANTS[0];
	const combo = getProximityAttackCombo();
	const comboPower = 0.85 + proximityAttack.comboIndex * 0.12;
	const slashPos = dummy.position.copy(proximityAttackOrigin)
		.addScaledVector(proximityAttackDir, 0.66 + (0.22 + 0.05 * proximityAttack.comboIndex) * t)
		.addScaledVector(proximityAttackSide, attackVariant.side * comboPower * (0.10 + Math.sin(t * Math.PI * 1.5) * 0.13));
	proximitySlashMesh.position.copy(slashPos);
	dummy.rotation.set(0, cursor.yaw, 0);
	proximitySlashMesh.quaternion.copy(dummy.quaternion);
	proximitySlashMesh.rotateX(Math.PI * 0.5);
	proximitySlashMesh.rotateZ(-Math.PI * 0.28 + t * Math.PI * 1.15);
	proximitySlashMesh.scale.setScalar((0.75 + t * 0.72) * hitBoost);
	proximitySlashMaterial.opacity = fade * 0.72;
	proximitySlashMesh.visible = true;

	const streakMid = dummy.position.copy(proximityAttackOrigin).lerp(proximityAttackImpact, 0.46);
	const streakLen = Math.max(0.3, proximityAttackOrigin.distanceTo(proximityAttackImpact) * (0.70 + comboPower * 0.10 + strike * 0.18));
	proximityStreakMesh.position.copy(streakMid);
	proximityStreakMesh.quaternion.setFromUnitVectors(yAxis, proximityAttackDir);
	proximityStreakMesh.scale.set(1.0 + strike * 1.2, streakLen, 1.0 + strike * 1.2);
	proximityStreakMaterial.opacity = fade * 0.42;
	proximityStreakMesh.visible = true;

	proximityImpactRing.position.copy(proximityAttackImpact);
	proximityImpactRing.quaternion.copy(camera.quaternion);
	proximityImpactRing.scale.setScalar((0.45 + t * 1.45) * hitBoost);
	proximityImpactMaterial.opacity = proximityAttack.visualHit ? fade * 0.9 : fade * 0.26;
	proximityImpactRing.visible = true;
}

function updateParticles(dt) {
	if (!particlesRuntimeEnabled()) {
		if (particleMesh) particleMesh.visible = false;
		return;
	}
	if (particleMesh) particleMesh.visible = true;
	for (let i = 0; i < PARTICLE_COUNT; i++) {
		const idx = i * PARTICLE_STRIDE;
		let life = particleData[idx + P_LIFE];
		if (life <= 0) {
			dummy.position.set(0, -999, 0);
			dummy.scale.setScalar(0);
			dummy.updateMatrix();
			particleMesh.setMatrixAt(i, dummy.matrix);
			continue;
		}
		particleData[idx + P_VY] -= 8 * dt;
		particleData[idx + P_X] += particleData[idx + P_VX] * dt;
		particleData[idx + P_Y] += particleData[idx + P_VY] * dt;
		particleData[idx + P_Z] += particleData[idx + P_VZ] * dt;
		life -= dt;
		particleData[idx + P_LIFE] = life;
		const t = Math.max(life / particleData[idx + P_MAXLIFE], 0);
		dummy.position.set(
			particleData[idx + P_X],
			particleData[idx + P_Y],
			particleData[idx + P_Z]
		);
		dummy.rotation.set(life * 8, life * 4, life * 6);
		dummy.scale.setScalar(t);
		dummy.updateMatrix();
		particleMesh.setMatrixAt(i, dummy.matrix);
	}
	particleMesh.instanceMatrix.needsUpdate = true;
}

const hitCenter = new THREE.Vector3();
const toTarget = new THREE.Vector3();
const shotOrigin = new THREE.Vector3();
const shotDir = new THREE.Vector3();
const characterForward = new THREE.Vector3();
const flatToTarget = new THREE.Vector3();
const AIM_CONE_DOT = 0.72;

function rayHitsTarget(origin, direction) {
	const tileOriginZ = getPlayerRoomTileOriginZ();
	const playerLocalZ = getPlayerLocalZ();
	let bestIndex = -1;
	let bestDistance = Infinity;
	const ray = new THREE.Ray(origin, direction);
	characterForward.set(
		-Math.sin(cursor.yaw),
		0,
		-Math.cos(cursor.yaw)
	).normalize();
	for (let i = 0; i < TARGET_COUNT; i++) {
		const idx = targetIndex(i);
		hitCenter.set(
			targetData[idx + T_X],
			targetData[idx + T_RENDER_Y],
			targetData[idx + T_Z] + tileOriginZ
		);
		toTarget.copy(hitCenter).sub(origin);
		const cameraForwardDistance = toTarget.dot(direction);
		if (cameraForwardDistance <= 0) continue;
		flatToTarget.set(
			targetData[idx + T_X] - player.pos.x,
			0,
			targetData[idx + T_Z] - playerLocalZ
		).normalize();
		const facingDot = flatToTarget.dot(characterForward);
		if (facingDot < AIM_CONE_DOT) continue;
		const radius = 0.55;
		const distSq = ray.distanceSqToPoint(hitCenter);
		if (distSq < radius * radius && cameraForwardDistance < bestDistance) {
			bestDistance = cameraForwardDistance;
			bestIndex = i;
		}
	}
	return bestIndex;
}

function updateVacuum(dt) {
	const time = frame.time;
	const tileOriginZ = getPlayerRoomTileOriginZ();
	proximityAttack.cooldown = Math.max(0, proximityAttack.cooldown - dt);
	if (frame.now - proximityAttack.lastTriggerTime > PROX_ATTACK_COMBO_WINDOW) {
		proximityAttack.comboIndex = 0;
	}
	proximityAttack.pose = Math.max(0, proximityAttack.pose - dt * 5.5);
	const targetPose = vacuum.active ? 1 : 0;
	vacuum.pose += (targetPose - vacuum.pose) * Math.min(1, dt * 10);

	if (phoneStorage.isFull) {
		vacuum.active = false;
		vacuum.power = 0;
		vacuum.fieldStrength = 0;
		vacuum.coneTightness = 0;
		targetLock.target = -1;
		targetLock.strength = 0;
		setSlurpRingtonePlaying(false);
		return;
	}

	if (vacuum.active) {
		vacuum.power = Math.min(1, vacuum.power + VACUUM_CHARGE_SPEED * dt);
	} else {
		vacuum.power = Math.max(0, vacuum.power - VACUUM_DECAY_SPEED * dt);
	}

	vacuum.fieldStrength += ((vacuum.active ? 1 : 0) - vacuum.fieldStrength) * Math.min(1, dt * 5);

	camera.getWorldPosition(shotOrigin);
	camera.getWorldDirection(shotDir);
	shotDir.normalize();
	updateScreenPlaneBasis();
	humanScreenRight.setFromMatrixColumn(camera.matrixWorld, 0).normalize();
	updateFirstPersonVacuumPullPoint();

	const attractionActive = vacuum.active && vacuum.power > 0.32;
	let bestSoul = -1;
	let bestScore = Infinity;

	let offeredFreeSoul = -1;
	let offeredFreeScore = Infinity;
	if (attractionActive) {
		for (let i = 0; i < TARGET_COUNT; i++) {
			if (soulDepositShot[i]) continue;
			if (soulDepositShot[i]) continue;
			if (soulState[i] !== SOUL_FREE && soulState[i] !== SOUL_ATTRACTED) continue;
			const idx = targetIndex(i);
			if (!isTargetSlurpable(idx)) continue;
			const x = targetData[idx + T_X];
			const y = targetData[idx + T_Y];
			const z = targetData[idx + T_Z];
			const worldZ = getNearestRepeatedWorldZ(z, vacuumPullPoint.z);
			if (!isSoulInAttractionOffer(x, y, worldZ)) continue;
			soulPoint.set(x, y, worldZ);
			soulToScreen.copy(vacuumPullPoint).sub(soulPoint);
			const distanceToScreen = soulToScreen.length();
			const cylinderBonus = isSoulInsideScreenCaptureCylinder(x, y, worldZ) ? -3.5 : 0;
			const score = distanceToScreen + cylinderBonus;
			if (score < offeredFreeScore) {
				offeredFreeScore = score;
				offeredFreeSoul = i;
			}
		}
	}

	for (let i = 0; i < TARGET_COUNT; i++) {
		const idx = targetIndex(i);
		const kind = targetData[idx + T_KIND] || 0;
		const soulMass = kind === 1 ? 1.45 : 1.0;
		const x = targetData[idx + T_X];
		const y = targetData[idx + T_Y];
		const z = targetData[idx + T_Z];
		const worldZ = getNearestRepeatedWorldZ(z, vacuumPullPoint.z);
		soulPoint.set(x, y, worldZ);
		soulToScreen.copy(vacuumPullPoint).sub(soulPoint);
		const distanceToScreen = Math.max(soulToScreen.length(), 0.001);
		const insideCylinder = isSoulInsideScreenCaptureCylinder(x, y, worldZ);
		const slurpable = isTargetSlurpable(idx);
		const offered = !soulDepositShot[i] && attractionActive && (
			soulState[i] === SOUL_LATCHED ||
			soulState[i] === SOUL_INGESTING ||
			(slurpable && (insideCylinder || i === offeredFreeSoul))
		);

		if (!attractionActive && (soulState[i] === SOUL_LATCHED || soulState[i] === SOUL_INGESTING)) {
			releaseSoulFromScreen(i, idx);
		}

		if (soulState[i] === SOUL_RECOILING) {
			soulRecoilTime[i] -= dt;
			const isDepositShot = soulDepositShot[i] > 0;
			if (isDepositShot) {
				targetData[idx + T_VY] -= DISCHARGE_GRAVITY * dt;
				const drag = Math.pow(DISCHARGE_AIR_DRAG_PER_SECOND, dt);
				targetData[idx + T_VX] *= drag;
				targetData[idx + T_VY] *= drag;
				targetData[idx + T_VZ] *= drag;
				targetData[idx + T_SPIN] += dt * 10.0;
			} else {
				targetData[idx + T_VY] -= 5.5 * dt;
			}
			const prevX = targetData[idx + T_X];
			const prevY = targetData[idx + T_Y];
			const prevZ = targetData[idx + T_Z];
			targetData[idx + T_X] += targetData[idx + T_VX] * dt;
			targetData[idx + T_Y] += targetData[idx + T_VY] * dt;
			targetData[idx + T_Z] += targetData[idx + T_VZ] * dt;
			if (shotSoulHitsRoomGrid(i, prevX, prevY, prevZ)) {
				depositShotSoulToRoom(i);
				continue;
			}
			if (isDepositShot) {
				const hitHuman = damageHumansAlongDepositShot(i, prevX, prevY, prevZ);
				if (Math.random() < 0.22) spawnRoomParticleBurst(targetData[idx + T_X], targetData[idx + T_Y], targetData[idx + T_Z]);
				if (hitHuman) {
					soulDepositShot[i] = 0;
					clearTargetSlot(i);
					continue;
				}
			}
			if (!isDepositShot) {
				const damping = Math.max(0, 1 - 3.5 * dt);
				targetData[idx + T_VX] *= damping;
				targetData[idx + T_VZ] *= damping;
				if (targetData[idx + T_Y] < HUMAN_GROUND_Y) {
					targetData[idx + T_Y] = HUMAN_GROUND_Y;
					targetData[idx + T_VY] = 0;
				}
			}
			const outOfBounds = isDepositShot
				? (targetData[idx + T_Y] < DISCHARGE_LOST_Y || Math.abs(targetData[idx + T_X]) > ROOM_WIDTH * 1.25 || Math.abs(targetData[idx + T_Z]) > ROOM_DEPTH * 1.25)
				: (targetData[idx + T_Y] < -10 || Math.abs(targetData[idx + T_X]) > ROOM_WIDTH * 0.9 || Math.abs(targetData[idx + T_Z]) > ROOM_DEPTH * 0.9);
			if (outOfBounds || soulRecoilTime[i] <= 0) {
				if (isDepositShot) {
					spawnRoomParticleBurst(targetData[idx + T_X], targetData[idx + T_Y], targetData[idx + T_Z]);
					soulDepositShot[i] = 0;
					clearTargetSlot(i);
				} else {
					setSoulState(i, SOUL_FREE);
					targetIngestProgress[i] = 0;
					targetLatchedToScreen[i] = 0;
					resetSoulVisualDeformation(i);
				}
			}
			continue;
		}

		if (isSoulUnavailable(i) || soulDepositShot[i]) continue;

		if (!offered) {
			if (soulState[i] === SOUL_ATTRACTED) setSoulState(i, SOUL_FREE);
			if (soulState[i] === SOUL_FREE) {
				targetIngestProgress[i] = Math.max(0, targetIngestProgress[i] - dt * 0.75);
				targetLatchedToScreen[i] = 0;
				clearSoulMotion(idx);
			}
			continue;
		}

		if (soulState[i] === SOUL_FREE) setSoulState(i, SOUL_ATTRACTED);

		const score = distanceToScreen + (insideCylinder ? -2.5 : 0);
		if (score < bestScore) {
			bestScore = score;
			bestSoul = i;
		}

		if (soulState[i] === SOUL_ATTRACTED) {
			const dir = soulToScreen.multiplyScalar(1 / distanceToScreen);
			const proximity = 1 - THREE.MathUtils.clamp(distanceToScreen / SOUL_ATTRACTION_RANGE, 0, 1);
			const closeEase = smooth01(proximity);
			const speed = vacuum.power * (3.2 + closeEase * 5.8 + (insideCylinder ? 8.5 : 0)) / soulMass;
			const move = Math.min(distanceToScreen, speed * dt);

			targetData[idx + T_X] += dir.x * move;
			targetData[idx + T_Y] += dir.y * move;
			writeTargetWorldZCanonical(idx, worldZ + dir.z * move);
			clearSoulMotion(idx);
			if (cameraMode.current !== CAMERA_MODE_FIRST) {
				keepSoulCoreOutsidePhoneSolid(idx, insideCylinder);
			}

			if (insideCylinder || distanceToScreen <= SOUL_LATCH_DISTANCE) {
				setSoulState(i, SOUL_LATCHED);
				targetLatchedToScreen[i] = 1;
			}
			continue;
		}

		if (soulState[i] === SOUL_LATCHED || soulState[i] === SOUL_INGESTING) {
			targetLatchedToScreen[i] = 1;

			const sealPoint = cameraMode.current === CAMERA_MODE_FIRST
				? springNetA.copy(vacuumPullPoint)
				: getScreenLatchPoint(springNetA, soulPoint);
			const phase = THREE.MathUtils.clamp(targetIngestProgress[i], 0, 1);
			const sealEase = THREE.MathUtils.smoothstep(phase, 0.08, 0.32);
			const pressureEase = THREE.MathUtils.smoothstep(phase, 0.32, 0.78);
			const popEase = THREE.MathUtils.smoothstep(phase, 0.78, 1.0);
			const latchSpeed = (7.0 + sealEase * 5.0 + popEase * 14.0) / soulMass;
			const toSeal = soulToScreen.copy(sealPoint).sub(soulPoint);
			const sealDist = toSeal.length();
			let postSealDist = sealDist;
			if (sealDist > 0.001) {
				const sealMove = Math.min(sealDist, latchSpeed * dt);
				toSeal.multiplyScalar(1 / sealDist);
				targetData[idx + T_X] += toSeal.x * sealMove;
				targetData[idx + T_Y] += toSeal.y * sealMove;
				writeTargetWorldZCanonical(idx, soulPoint.z + toSeal.z * sealMove);
				postSealDist = Math.max(0, sealDist - sealMove);
			}
			clearSoulMotion(idx);
			if (cameraMode.current !== CAMERA_MODE_FIRST) {
				keepSoulCoreOutsidePhoneSolid(idx, true);
			}

			const sealedEnough = postSealDist <= SOUL_SEAL_DISTANCE;
			if (sealedEnough) {
				setSoulState(i, SOUL_INGESTING);
			} else {
				setSoulState(i, SOUL_LATCHED);
			}

			if (sealedEnough) {
				const phaseRate = vacuum.power * (0.38 + sealEase * 0.55 + pressureEase * 0.85 + popEase * 2.25) / soulMass;
				targetIngestProgress[i] = THREE.MathUtils.clamp(targetIngestProgress[i] + dt * phaseRate, 0, 1);
				targetData[idx + T_HEALTH] -= VACUUM_DAMAGE * vacuum.power * dt * 0.10;
			}

			if (targetIngestProgress[i] >= SOUL_CAPTURE_COMMIT_PHASE || targetData[idx + T_HEALTH] <= 0) {
				queueSoulCapture(
					i,
					idx,
					targetData[idx + T_X],
					targetData[idx + T_RENDER_Y] || targetData[idx + T_Y],
					targetData[idx + T_Z]
				);
				continue;
			}
		}
	}

	updateSlurpRingtoneLoop();
	targetLock.target = bestSoul;
	targetLock.strength += (((bestSoul !== -1 && attractionActive) ? 1 : 0) - targetLock.strength) * Math.min(1, dt * 8);
	vacuum.coneTightness += (((bestSoul !== -1 && attractionActive) ? 1 : 0) - vacuum.coneTightness) * Math.min(1, dt * 4);
	if (bestSoul !== -1 && attractionActive) flashScreen();
}


function keyDown(code) {
	return !!input.keys[code];
}

function pointerOwnsCamera() {
	return game.started && !game.uiPaused && document.pointerLockElement === renderer.domElement;
}

function isRunningInput() {
	return keyDown("ShiftLeft") || keyDown("ShiftRight");
}

function getMoveAxes() {
	return {
		forward: (keyDown("KeyW") ? 1 : 0) - (keyDown("KeyS") ? 1 : 0),
		strafe: (keyDown("KeyD") ? 1 : 0) - (keyDown("KeyA") ? 1 : 0)
	};
}

function writeCursorBasis(forward, right) {
	forward.set(-Math.sin(cursor.yaw), 0, -Math.cos(cursor.yaw));
	right.set(Math.cos(cursor.yaw), 0, -Math.sin(cursor.yaw));
}

function setMode(nextMode) {
	game.mode = "vacuum";
	crosshair.classList.add("vacuum-mode");
	crosshair.classList.remove("laser-mode");
	if (modeLabel) modeLabel.textContent = "";
}

function toggleCameraMode() {
	cameraMode.current = cameraMode.current === CAMERA_MODE_THIRD ? CAMERA_MODE_FIRST : CAMERA_MODE_THIRD;
	if (document.body) {
		document.body.classList.toggle("first-person-camera", cameraMode.current === CAMERA_MODE_FIRST);
	}
}

function shootStoredSoulInput() {
	vacuum.active = false;
	fireStoredCube();
}

function captureDoubleJumpFlipYaw() {
	const axes = getMoveAxes();
	const camForwardX = -Math.sin(cursor.yaw);
	const camForwardZ = -Math.cos(cursor.yaw);
	const camRightX = Math.cos(cursor.yaw);
	const camRightZ = -Math.sin(cursor.yaw);
	let flipDirX = camForwardX * axes.forward + camRightX * axes.strafe;
	let flipDirZ = camForwardZ * axes.forward + camRightZ * axes.strafe;
	const flipDirLen = Math.hypot(flipDirX, flipDirZ);
	if (flipDirLen > 0.001) {
		flipDirX /= flipDirLen;
		flipDirZ /= flipDirLen;
		actionPose.doubleJumpFlipYaw = Math.atan2(-flipDirX, -flipDirZ);
	} else {
		actionPose.doubleJumpFlipYaw = cursor.yaw;
	}
}

function startGroundJump() {
	spendBattery(BATTERY_JUMP_COST, "JUMP");
	player.jumpVel = phoneCharacter.jump.groundVelocity;
	player.grounded = false;
	player.coyoteTimer = 0;
	player.jumpBufferTimer = 0;
	player.airJumpsRemaining = phoneCharacter.jump.airJumps;
}

function startAirJump() {
	spendBattery(BATTERY_DOUBLE_JUMP_COST, "DOUBLE JUMP");
	player.jumpVel = phoneCharacter.jump.airVelocity;
	player.jumpBufferTimer = 0;
	player.airJumpsRemaining--;
	actionPose.doubleJumpTimer = Math.max(actionPose.doubleJumpTimer, DOUBLE_JUMP_POSE_HOLD);
	actionPose.doubleJump = 1;
	actionPose.doubleJumpVacuumPause = Math.max(actionPose.doubleJumpVacuumPause, 0.16);
	captureDoubleJumpFlipYaw();
	spawnParticleBurst(player.pos.x, player.pos.y + 0.08, player.pos.z);
}

function tryJump() {
	if (player.grounded || player.coyoteTimer > 0) {
		startGroundJump();
	} else if (player.airJumpsRemaining > 0) {
		startAirJump();
	}
}

function handlePrimaryActionPress() {
	vacuum.active = true;
}


function startGameMusic() {
	if (!gameMusic) return;
	gameMusic.volume = 0.52 * getMusicLevel() * getMusicDuckGain();
	gameMusic.loop = true;
	const playAttempt = gameMusic.play();
	if (playAttempt && typeof playAttempt.catch === "function") {
		playAttempt.catch(() => {});
	}
}

function stopGameMusic(reset = false) {
	if (!gameMusic) return;
	gameMusic.pause();
	if (reset) {
		try { gameMusic.currentTime = 0; } catch (err) {}
	}
}

function startGameOverMusic() {
	if (!gameOverMusic) return;
	gameOverMusic.volume = 0.62 * getMusicLevel();
	gameOverMusic.loop = false;
	try { gameOverMusic.currentTime = 0; } catch (err) {}
	const playAttempt = gameOverMusic.play();
	if (playAttempt && typeof playAttempt.catch === "function") {
		playAttempt.catch(() => {});
	}
}

function stopGameOverMusic(reset = false) {
	if (!gameOverMusic) return;
	gameOverMusic.pause();
	if (reset) {
		try { gameOverMusic.currentTime = 0; } catch (err) {}
	}
}

function playEventSound(name, volume = 0.55) {
	const src = eventSoundUrls[name];
	if (!src) return;
	let cueKind = "system";
	if (AUDIO_BUS.combat.has(name)) cueKind = "combat";
	else if (AUDIO_BUS.system.has(name)) cueKind = "system";
	duckAudioForCue(cueKind);
	const maxActive = cueKind === "combat" ? 2 : 1;
	const minInterval = cueKind === "combat" ? 0.035 : 0.065;
	playPooledSfx(src, volume, { playbackRate: 1, minInterval, maxActive });
}

applyAudioLevels();

window.digitalBreakdownAudioMix = function () {
	return {
		music: getMusicLevel(),
		sfx: getSfxLevel(),
		musicDuckGain: getMusicDuckGain(),
		slurpDuckGain: getSlurpDuckGain(),
		sfxPools: sfxPools.size,
		activeSfx: sfxActiveVoices.size + getGlobalActiveSfxCount()
	};
};

function setSlurpRingtonePlaying(playing) {
	const key = "slurpRingtone";
	if (!SFX_AUDIO_BASE64[key] || getSfxLevel() <= 0) playing = false;
	if (playing) {
		if (slurpRingtonePlaying || slurpRingtoneStarting) return;
		slurpRingtoneStarting = true;
		decodeSfxBufferForKey(key).then((buffer) => {
			slurpRingtoneStarting = false;
			if (!buffer || slurpRingtonePlaying || !game.started || game.dead) return;
			const ctx = getSfxAudioContext();
			if (!ctx) return;
			slurpRingtoneSource = ctx.createBufferSource();
			slurpRingtoneGain = ctx.createGain();
			slurpRingtoneSource.buffer = buffer;
			slurpRingtoneSource.loop = true;
			slurpRingtoneSource.playbackRate.value = 1;
			slurpRingtoneGain.gain.value = 0.13 * getSfxLevel() * getSlurpDuckGain();
			slurpRingtoneSource.connect(slurpRingtoneGain);
			slurpRingtoneGain.connect(ctx.destination);
			slurpRingtoneSource.onended = () => {
				try { slurpRingtoneSource && slurpRingtoneSource.disconnect(); } catch (err) {}
				try { slurpRingtoneGain && slurpRingtoneGain.disconnect(); } catch (err) {}
				slurpRingtoneSource = null;
				slurpRingtoneGain = null;
				slurpRingtonePlaying = false;
			};
			slurpRingtonePlaying = true;
			try { slurpRingtoneSource.start(0); } catch (err) { slurpRingtonePlaying = false; }
		});
		return;
	}
	if (slurpRingtoneSource) {
		try { slurpRingtoneSource.stop(0); } catch (err) {}
	}
	slurpRingtoneStarting = false;
	slurpRingtonePlaying = false;
	slurpRingtoneSource = null;
	slurpRingtoneGain = null;
}
function updateSlurpRingtoneLoop() {
	if (!game.started || game.dead || phoneStorage.isFull) {
		setSlurpRingtonePlaying(false);
		return;
	}
	let activeSlurp = false;
	for (let i = 0; i < TARGET_COUNT; i++) {
		if (soulDepositShot[i]) continue;
		if (soulState[i] === SOUL_LATCHED || soulState[i] === SOUL_INGESTING) {
			activeSlurp = true;
			break;
		}
	}
	setSlurpRingtonePlaying(activeSlurp);
}

function resetEventSoundState() {
	eventSoundState.lowPowerArmed = true;
	eventSoundState.connectPowerArmed = false;
	eventSoundState.lastDamageAckTime = -9999;
}

function updateBatteryEventSounds(beforeValue = battery.value) {
	if (!battery || game.dead) return;
	const beforeRatio = THREE.MathUtils.clamp(beforeValue / battery.max, 0, 1);
	const nowRatio = THREE.MathUtils.clamp(battery.value / battery.max, 0, 1);
	if (beforeRatio > EVENT_SFX_LOW_THRESHOLD && nowRatio <= EVENT_SFX_LOW_THRESHOLD && eventSoundState.lowPowerArmed) {
		playEventSound("lowPower", 0.48);
		eventSoundState.lowPowerArmed = false;
	}
	if (nowRatio <= EVENT_SFX_VERY_LOW_THRESHOLD) {
		eventSoundState.connectPowerArmed = true;
	}
	if (eventSoundState.connectPowerArmed && beforeRatio < 1 && nowRatio >= 0.995) {
		playEventSound("connectPower", 0.52);
		eventSoundState.connectPowerArmed = false;
		eventSoundState.lowPowerArmed = true;
	}
	if (nowRatio > EVENT_SFX_LOW_THRESHOLD + 0.08) {
		eventSoundState.lowPowerArmed = true;
	}
}

function playDamageAckSound() {
	const now = pulseTime() / 1000;
	if (now - eventSoundState.lastDamageAckTime < EVENT_SFX_DAMAGE_COOLDOWN) return;
	eventSoundState.lastDamageAckTime = now;
	playEventSound("negativeAck", 0.26);
}

function setBootReady() {
	if (loadingStatus) loadingStatus.textContent = "READY";
	if (startButton) {
		startButton.disabled = false;
		startButton.textContent = "START";
	}
}

function placePlayerAtRoomStart() {
	player.pos.set(0, GROUND_Y, ROOM_START_Z);
	player.vel.set(0, 0, 0);
	player.jumpVel = 0;
	player.grounded = true;
	player.coyoteTimer = PARKOUR.coyoteTime;
	player.jumpBufferTimer = 0;
	player.airJumpsRemaining = phoneCharacter.jump.airJumps;
	roomTopology.currentTileIndex = getRoomTileIndex(player.pos.z);
	roomTopology.previousTileIndex = roomTopology.currentTileIndex;
	roomTopology.mode = room.doorOpen ? "advance" : "loop";
}

function resetRunState() {
	resetRunRules();
	setSlurpRingtonePlaying(false);
	stopGameOverMusic(true);
	resetEventSoundState();
	game.started = false;
	game.dead = false;
	game.uiPaused = false;
	closeHudPanels();
	input.keys = {};
	placePlayerAtRoomStart();
	targetLock.target = -1;
	targetLock.strength = 0;
	battery.value = battery.max;
	battery.activity = "Idle";
	clearActivePowerups();
	clearFlowerPowerups();
	updateSupplementalBatteryHud();
	resetBatteryCombo();
	room.index = 1;
	room.seed = 12345;
	room.identity = chooseRoomIdentity(room.seed, room.index);
	vacuum.active = false;
	vacuum.power = 0;
	setMode("vacuum");
	resetRoomInventory();
	syncRoomIdentityPresentation();
	updateRoomVisuals();
	updateBatteryHud();
}

function triggerRunDeath(reason = "EMPTY") {
	setSlurpRingtonePlaying(false);
	if (game.dead) return;
	game.dead = true;
	game.started = false;
	game.uiPaused = false;
	closeHudPanels();
	stopGameMusic(true);
	startGameOverMusic();
	playEventSound("vcEnded", 0.64);
	battery.value = 0;
	battery.activity = "Dead";
	clearActivePowerups();
	clearFlowerPowerups();
	updateSupplementalBatteryHud();
	resetBatteryCombo();
	vacuum.active = false;
	input.keys = {};
	player.vel.set(0, 0, 0);
	player.jumpVel = 0;
	document.body.classList.remove("game-started");
	if (batteryEvents) { batteryEvents.innerHTML = "";  }
	if (document.pointerLockElement === renderer.domElement) document.exitPointerLock();
	if (crosshair) crosshair.style.display = "none";
	if (loadingStatus) loadingStatus.textContent = "BATTERY EMPTY";
	if (startButton) {
		startButton.disabled = false;
		startButton.textContent = "START";
	}
	if (startOverlay) startOverlay.style.display = "flex";
	updateBatteryHud();
}

function startGame() {
	if (batteryEvents) { batteryEvents.innerHTML = "";  }
	if (startButton && startButton.disabled) return;
	resetRunState();
	placePlayerAtRoomStart();
	// Build the first visible gameplay frame before the overlay is hidden.
	// This prevents stale/default instanced matrices from flashing or filling the camera.
	updateCamera();
	updateTargets();
	updateParticles(0);
	game.started = true;
	game.uiPaused = false;
	closeHudPanels();
	warmSfxPools();
	document.body.classList.add("game-started");
	if (batteryEvents) { batteryEvents.innerHTML = "";  }
	startGameMusic();
	playEventSound("vcInvitation", 0.58);
	ensureEnergySymbolStrip();
	updateEnergySymbolStrip();
	startOverlay.style.display = "none";
	crosshair.style.display = "block";
	updateBatteryHud();
	renderer.domElement.requestPointerLock();
}

function setKey(code, isDown) {
	input.keys[code] = isDown;
}

function handleKeyDown(e) {
	setKey(e.code, true);
	if (e.code === "KeyF") triggerProximityAttack();
	if (e.code === "KeyC" && !e.repeat) toggleCameraMode();
	if (e.code === "KeyQ" && !e.repeat) shootStoredSoulInput();
	if (!e.repeat && e.code === "KeyM") triggerDebugDataMosh();
	if (game.started && e.code === "Space" && !e.repeat) {
		player.jumpBufferTimer = PARKOUR.jumpBuffer;
		tryJump();
	}
}

function handleKeyUp(e) {
	setKey(e.code, false);
}

function handlePointerLook(e) {
	if (!pointerOwnsCamera()) return;
	cursor.yaw -= e.movementX * cursor.sensitivity;
	cursor.pitch = Math.max(-cursor.maxPitch, Math.min(cursor.maxPitch, cursor.pitch - e.movementY * cursor.sensitivity));
}

function handlePointerPress(e) {
	if (!pointerOwnsCamera()) return;
	if (e.button === 2) {
		triggerProximityAttack();
		return;
	}
	handlePrimaryActionPress();
}

function stopHeldActions() {
	vacuum.active = false;
}

function blockPointerMenu(e) {
	if (document.pointerLockElement === renderer.domElement) e.preventDefault();
}

function fitViewport() {
	camera.aspect = innerWidth / innerHeight;
	camera.updateProjectionMatrix();
	renderer.setSize(innerWidth, innerHeight);
	applyGraphicsSettings();
}

startButton.onclick = startGame;
renderer.domElement.addEventListener("click", function () {
	if (game.started && !game.dead) leaveHudPauseAndRelock();
});
document.addEventListener("pointerlockchange", function () {
	if (!game.started || game.dead) return;
	if (document.pointerLockElement === renderer.domElement && game.uiPaused && !hasOpenHudPanel()) {
		game.uiPaused = false;
	}
});
window.addEventListener("keydown", handleKeyDown);
window.addEventListener("keyup", handleKeyUp);
window.addEventListener("mousemove", handlePointerLook);
window.addEventListener("mousedown", handlePointerPress);
window.addEventListener("mouseup", stopHeldActions);
window.addEventListener("contextmenu", blockPointerMenu);
window.addEventListener("resize", fitViewport);

const vAimDir = new THREE.Vector3();
const vCameraOffset = new THREE.Vector3();

const cameraCollisionStart = new THREE.Vector3();
const cameraCollisionEnd = new THREE.Vector3();
const cameraCollisionTmp = new THREE.Vector3();
const CAMERA_COLLISION_RADIUS = 0.42;
const CAMERA_COLLISION_BACKOFF = 0.16;

function getSegmentAabbHitT(from, to, box, pad = CAMERA_COLLISION_RADIUS) {
	let tMin = 0;
	let tMax = 1;
	const dx = to.x - from.x;
	const dy = to.y - from.y;
	const dz = to.z - from.z;
	const bounds = [
		[box.minX - pad, box.maxX + pad, from.x, dx],
		[box.bottomY - pad, box.topY + pad, from.y, dy],
		[box.minZ - pad, box.maxZ + pad, from.z, dz]
	];
	for (const [min, max, origin, delta] of bounds) {
		if (Math.abs(delta) < 0.00001) {
			if (origin < min || origin > max) return null;
			continue;
		}
		const inv = 1 / delta;
		let a = (min - origin) * inv;
		let b = (max - origin) * inv;
		if (a > b) { const tmp = a; a = b; b = tmp; }
		tMin = Math.max(tMin, a);
		tMax = Math.min(tMax, b);
		if (tMin > tMax) return null;
	}
	return THREE.MathUtils.clamp(tMin, 0, 1);
}

function constrainThirdPersonCamera(desired, lookBase) {
	cameraCollisionStart.copy(lookBase);
	cameraCollisionStart.y += 0.58;
	cameraCollisionEnd.copy(desired);
	// Use wrapped local coordinates for collision tests, but do not force the
	// chase camera into the player's tile. At a room seam the correct camera
	// position can naturally remain one tile behind the player; clamping it into
	// the player's tile flips it to the opposite side of the wrapped room.
	const localStartZ = wrapZ(cameraCollisionStart.z);
	// Keep the camera ray continuous across the room seam. Wrapping both endpoints
	// independently can turn a short cross-seam chase-camera segment into a long
	// segment across the whole room, which makes forward doorway traversal feel
	// like the camera/control basis briefly flips.
	const localEndZ = localStartZ + (cameraCollisionEnd.z - cameraCollisionStart.z);
	let nearestT = 1;
	for (const c of roomColliders) {
		const t = getSegmentAabbHitT(
			{ x: cameraCollisionStart.x, y: cameraCollisionStart.y, z: localStartZ },
			{ x: cameraCollisionEnd.x, y: cameraCollisionEnd.y, z: localEndZ },
			c
		);
		if (t !== null && t < nearestT) nearestT = t;
	}
	const doorX0 = -fixedPortal.halfWidth;
	const doorX1 = fixedPortal.halfWidth;
	const wallPad = 0.3;
	const roomBounds = [
		{ minX: -ROOM_WIDTH / 2 - 0.3, maxX: -ROOM_WIDTH / 2 + 0.5, bottomY: GROUND_Y, topY: ROOM_WALL_HEIGHT, minZ: -ROOM_DEPTH / 2, maxZ: ROOM_DEPTH / 2 },
		{ minX: ROOM_WIDTH / 2 - 0.5, maxX: ROOM_WIDTH / 2 + 0.3, bottomY: GROUND_Y, topY: ROOM_WALL_HEIGHT, minZ: -ROOM_DEPTH / 2, maxZ: ROOM_DEPTH / 2 },
		// Front/back bounds are segmented around the doorway opening so the chase
		// camera can pass through the same physical door instead of being pinned
		// behind an invisible full-wall collider.
		{ minX: -ROOM_WIDTH / 2, maxX: doorX0, bottomY: GROUND_Y, topY: ROOM_WALL_HEIGHT, minZ: -ROOM_DEPTH / 2 - wallPad, maxZ: -ROOM_DEPTH / 2 + 0.5 },
		{ minX: doorX1, maxX: ROOM_WIDTH / 2, bottomY: GROUND_Y, topY: ROOM_WALL_HEIGHT, minZ: -ROOM_DEPTH / 2 - wallPad, maxZ: -ROOM_DEPTH / 2 + 0.5 },
		{ minX: doorX0, maxX: doorX1, bottomY: fixedPortal.topY, topY: ROOM_WALL_HEIGHT, minZ: -ROOM_DEPTH / 2 - wallPad, maxZ: -ROOM_DEPTH / 2 + 0.5 },
		{ minX: -ROOM_WIDTH / 2, maxX: doorX0, bottomY: GROUND_Y, topY: ROOM_WALL_HEIGHT, minZ: ROOM_DEPTH / 2 - 0.5, maxZ: ROOM_DEPTH / 2 + wallPad },
		{ minX: doorX1, maxX: ROOM_WIDTH / 2, bottomY: GROUND_Y, topY: ROOM_WALL_HEIGHT, minZ: ROOM_DEPTH / 2 - 0.5, maxZ: ROOM_DEPTH / 2 + wallPad },
		{ minX: doorX0, maxX: doorX1, bottomY: fixedPortal.topY, topY: ROOM_WALL_HEIGHT, minZ: ROOM_DEPTH / 2 - 0.5, maxZ: ROOM_DEPTH / 2 + wallPad }
	];
	for (const c of roomBounds) {
		const t = getSegmentAabbHitT(
			{ x: cameraCollisionStart.x, y: cameraCollisionStart.y, z: localStartZ },
			{ x: cameraCollisionEnd.x, y: cameraCollisionEnd.y, z: localEndZ },
			c,
			CAMERA_COLLISION_RADIUS * 0.8
		);
		if (t !== null && t < nearestT) nearestT = t;
	}
	if (nearestT < 1) {
		cameraCollisionTmp.copy(cameraCollisionEnd).sub(cameraCollisionStart);
		const dist = cameraCollisionTmp.length();
		const safeT = dist > 0.001 ? Math.max(0, nearestT - CAMERA_COLLISION_BACKOFF / dist) : 0;
		desired.copy(cameraCollisionStart).addScaledVector(cameraCollisionTmp, safeT);
	}
	desired.x = THREE.MathUtils.clamp(desired.x, -ROOM_WIDTH / 2 + CAMERA_COLLISION_RADIUS, ROOM_WIDTH / 2 - CAMERA_COLLISION_RADIUS);
	const desiredTileOriginZ = getRoomTileOriginZ(getRoomTileIndex(desired.z));
	const desiredLocalZ = wrapZ(desired.z);
	const nearDoorSeam = Math.abs(Math.abs(desiredLocalZ) - ROOM_DEPTH / 2) < CAMERA_COLLISION_RADIUS * 2.8;
	const desiredInDoorAperture = isInsideDoorAperture({ x: desired.x, y: desired.y, z: desiredLocalZ }, CAMERA_COLLISION_RADIUS * 0.8);
	if (!nearDoorSeam || !desiredInDoorAperture) {
		desired.z = desiredTileOriginZ + THREE.MathUtils.clamp(desiredLocalZ, -ROOM_DEPTH / 2 + CAMERA_COLLISION_RADIUS, ROOM_DEPTH / 2 - CAMERA_COLLISION_RADIUS);
	}
	desired.y = THREE.MathUtils.clamp(desired.y, GROUND_Y + 0.65, ROOM_WALL_HEIGHT - 0.45);
}


function updateMovement(dt) {
	updatePlayerPhysics(dt);
	updatePlayerActionPoseTimers(dt);
	updatePhoneVisualPose(dt);
	updatePlayerAvatar(dt);
}

function updatePlayerPhysics(dt) {
	const previousPlayerZ = player.pos.z;
	if (player.jumpBufferTimer > 0) player.jumpBufferTimer = Math.max(0, player.jumpBufferTimer - dt);
	if (player.grounded) player.coyoteTimer = PARKOUR.coyoteTime;
	else player.coyoteTimer = Math.max(0, player.coyoteTimer - dt);

	const running = isRunningInput();
	const power = batteryPower();
	const airControl = player.grounded ? 1 : PARKOUR.airAccelMultiplier;
	const airSpeed = player.grounded ? 1 : PARKOUR.airMaxSpeedMultiplier;
	const accel = (running ? phoneCharacter.movement.runAccel : phoneCharacter.movement.walkAccel) * power * airControl;
	const maxSpeed = (running ? phoneCharacter.movement.runMaxSpeed : phoneCharacter.movement.walkMaxSpeed) * power * airSpeed;
	const vacuumSlow = 1.0 - vacuum.pose * (1.0 - VACUUM_MOVE_MULT);
	writeCursorBasis(vForward, vRight);
	vMove.set(0, 0, 0);
	const axes = getMoveAxes();
	if (axes.forward > 0) vMove.add(vForward);
	if (axes.forward < 0) vMove.sub(vForward);
	if (axes.strafe < 0) vMove.sub(vRight);
	if (axes.strafe > 0) vMove.add(vRight);
	if (vMove.lengthSq() > 0) {
		vMove.normalize();
		player.vel.addScaledVector(vMove, accel * vacuumSlow * dt);
	}
	if (player.vel.length() > maxSpeed * vacuumSlow) {
		player.vel.setLength(maxSpeed * vacuumSlow);
	}
	updateProximityAttackDash(dt);
	if (!player.grounded) {
		player.jumpVel -= phoneCharacter.movement.gravity * dt;
		applyWallClimb(dt);
		player.pos.y += player.jumpVel * dt;
		clampPlayerToRoomHeight();
		const supportY = getPlayerSupportY(player.pos.x, player.pos.z);
		if (player.pos.y <= supportY) {
			player.pos.y = supportY;
			player.jumpVel = 0;
			player.grounded = true;
			player.coyoteTimer = PARKOUR.coyoteTime;
			player.airJumpsRemaining = phoneCharacter.jump.airJumps;
			actionPose.doubleJumpTimer = 0;
			actionPose.doubleJump = 0;
			if (Math.hypot(player.vel.x, player.vel.z) > 1.2) player.vel.multiplyScalar(PARKOUR.landingMomentumBoost);
		}
	}
	if (player.grounded && player.jumpBufferTimer > 0) startGroundJump();
	player.pos.addScaledVector(player.vel, dt);
	resolvePlayerObstacleCollisions();
	updateRoomTopology(previousPlayerZ, player.pos.z);
	clampPlayerToRoomHeight();
	const supportYAfterMove = getPlayerSupportY(player.pos.x, player.pos.z);
	if (player.grounded) {
		if (player.pos.y > supportYAfterMove + 0.12) {
			player.grounded = false;
		} else {
			player.pos.y = supportYAfterMove;
		}
	} else if (player.jumpVel <= 0 && player.pos.y <= supportYAfterMove) {
		player.pos.y = supportYAfterMove;
		player.jumpVel = 0;
		player.grounded = true;
		player.coyoteTimer = PARKOUR.coyoteTime;
		player.airJumpsRemaining = phoneCharacter.jump.airJumps;
	}
	const minX = -ROOM_WIDTH / 2 + 1.1;
	const maxX = ROOM_WIDTH / 2 - 1.1;
	if (player.pos.x < minX) {
		player.pos.x = minX;
		if (player.vel.x < 0) player.vel.x = 0;
		player.vel.z *= PARKOUR.wallSlideRetention;
	} else if (player.pos.x > maxX) {
		player.pos.x = maxX;
		if (player.vel.x > 0) player.vel.x = 0;
		player.vel.z *= PARKOUR.wallSlideRetention;
	}
	const friction = player.grounded ? phoneCharacter.movement.friction : PARKOUR.airFriction;
	player.vel.multiplyScalar(Math.pow(friction, dt * 60));
	phoneTargetPos.copy(player.pos);
}

function updatePlayerActionPoseTimers(dt) {
	actionPose.doubleJumpVacuumPause = Math.max(0, actionPose.doubleJumpVacuumPause - dt);
	actionPose.doubleJumpTimer = Math.max(0, actionPose.doubleJumpTimer - dt);
	actionPose.doubleJump = actionPose.doubleJumpTimer > 0 ? 1 : 0;
	actionPose.dischargeTimer = Math.max(0, actionPose.dischargeTimer - dt);
	actionPose.discharge = actionPose.dischargeTimer > 0 ? 1 : 0;
}


function updatePhoneGait(dt, running) {
	resetPhoneGait();
	const horizontalSpeed = Math.hypot(player.vel.x, player.vel.z);
	const speedMix = THREE.MathUtils.clamp(horizontalSpeed / Math.max(0.001, PHONE_GAIT_RUN_SPEED), 0, 1);
	const sprintMix = running
		? THREE.MathUtils.clamp(horizontalSpeed / Math.max(0.001, PHONE_GAIT_RUN_SPEED * 0.72), 0, 1)
		: 0;
	const runMix = THREE.MathUtils.clamp(
		(horizontalSpeed - PHONE_GAIT_WALK_SPEED * 0.72) /
		Math.max(0.001, PHONE_GAIT_RUN_SPEED - PHONE_GAIT_WALK_SPEED * 0.72),
		0,
		1
	);
	const targetRollEnergy = player.grounded && horizontalSpeed > 0.08
		? THREE.MathUtils.clamp(speedMix * 0.72 + sprintMix * 0.28, 0, 1)
		: 0;
	const gaitResponse = targetRollEnergy > phoneGait.rollEnergy ? 10 : 5.5;
	phoneGait.rollEnergy += (targetRollEnergy - phoneGait.rollEnergy) * Math.min(1, dt * gaitResponse);
	if (player.grounded && horizontalSpeed > 0.025) {
		const distanceThisFrame = horizontalSpeed * dt;
		const sprintRadiusTighten = THREE.MathUtils.lerp(1.0, 0.78, sprintMix);
		phoneGait.phase += distanceThisFrame / (PHONE_GAIT_CYLINDER_RADIUS * sprintRadiusTighten);
	}

	const step = Math.sin(phoneGait.phase);
	const stepAbs = Math.abs(step);
	const plantedSide = step >= 0 ? 1 : -1;
	const conePhase = phoneGait.phase;
	const coneQuadrature = Math.cos(conePhase);
	const contactEase = Math.pow(stepAbs, 0.20);
	const transferEase = Math.pow(1 - Math.abs(coneQuadrature), 0.34);
	const coneSweep = Math.sin(conePhase * 2);
	const coneCatch = Math.pow(Math.max(0, coneSweep), 0.42);
	const mirroredCone = plantedSide * (contactEase * 0.72 + transferEase * 0.28);
	const oloidLoop = Math.sin(conePhase + Math.PI * 0.5);
	const rotationPower = THREE.MathUtils.clamp(runMix * 0.72 + sprintMix * 0.28, 0, 1);
	const exaggeration = THREE.MathUtils.lerp(1.15, 1.55, rotationPower);
	const cylinderPitchAmp = THREE.MathUtils.lerp(PHONE_GAIT_WALK_PITCH, PHONE_GAIT_RUN_PITCH, rotationPower) * exaggeration;
	const cornerRollAmp = THREE.MathUtils.lerp(PHONE_GAIT_WALK_ROLL, PHONE_GAIT_RUN_ROLL, rotationPower) * exaggeration;
	const cornerYawAmp = THREE.MathUtils.lerp(PHONE_GAIT_WALK_YAW, PHONE_GAIT_RUN_YAW, rotationPower) * exaggeration;
	const gaitSuppression = 1.0 - Math.max(vacuum.pose, actionPose.discharge, actionPose.doubleJump) * 0.65;
	const gaitEnergy = phoneGait.rollEnergy * gaitSuppression;

	const forwardRoll = coneCatch * 0.72 + contactEase * 0.28;
	const obliqueSweep = plantedSide * (transferEase * 0.62 + stepAbs * 0.38) * THREE.MathUtils.lerp(0.7, 1.18, rotationPower);
	phoneGait.energy = gaitEnergy;
	phoneGait.pitch = -forwardRoll * cylinderPitchAmp * gaitEnergy;
	phoneGait.roll = (-mirroredCone * cornerRollAmp + coneQuadrature * PHONE_GAIT_CONE_TWIST * transferEase) * gaitEnergy;
	phoneGait.yaw = (mirroredCone * cornerYawAmp + obliqueSweep * PHONE_GAIT_OBLIQUE_SWEEP) * gaitEnergy;
	phoneGait.lift = (contactEase * 0.58 + transferEase * 0.42) * PHONE_GAIT_LIFT * gaitEnergy;
	phoneGait.forward = forwardRoll * PHONE_GAIT_FORWARD_OFFSET * gaitEnergy;
	phoneGait.side = (mirroredCone * PHONE_GAIT_SIDE_OFFSET + oloidLoop * PHONE_GAIT_OLOID_MEANDER * transferEase) * gaitEnergy;
}

function selectPhonePoseState(jumpVacuumBlocked) {
	if (proximityAttack.visualTimer > 0) return PHONE_POSE_STATE.PROXIMITY_ATTACK;
	if (actionPose.discharge > 0) return PHONE_POSE_STATE.DISCHARGING;
	if (vacuum.active && !jumpVacuumBlocked) return PHONE_POSE_STATE.VACUUMING;
	if (phoneGait.energy > 0.01) return PHONE_POSE_STATE.LOCOMOTION;
	return PHONE_POSE_STATE.IDLE;
}

function applyDoubleJumpPhonePose(doubleJumpFlip) {
	phonePose.state = PHONE_POSE_STATE.DOUBLE_JUMP_FLIP;
	dummy.rotation.set(0, actionPose.doubleJumpFlipYaw, 0);
	phonePose.quaternion.copy(dummy.quaternion);
	phonePoseRotateX(-doubleJumpFlip);
}

function applyBasePhoneLocomotionPose(ritualFlip, vacuumWobble) {
	phonePoseRotateX(-ritualFlip + phoneGait.pitch);
	phonePoseRotateY(phoneGait.yaw);
	phonePoseRotateZ(vacuumWobble + phoneGait.roll);
	phonePose.position.y += phoneGait.lift;
	phonePose.position.addScaledVector(vForward, phoneGait.forward);
	phonePose.position.addScaledVector(vRight, phoneGait.side);
}

function applyProximityAttackPhoneOverlay() {
	if (proximityAttack.visualTimer <= 0) return;
	const attackT = 1 - THREE.MathUtils.clamp(proximityAttack.visualTimer / proximityAttack.visualDuration, 0, 1);
	const attackSnap = Math.sin(attackT * Math.PI);
	const attackRecover = Math.sin(Math.min(1, attackT * 1.45) * Math.PI);
	const attackVariant = PROX_ATTACK_VARIANTS[proximityAttack.variant] || PROX_ATTACK_VARIANTS[0];
	const hitWeight = proximityAttack.visualHit ? 1.18 : 0.82;
	const combo = getProximityAttackCombo();
	phonePose.position.addScaledVector(vForward, (combo.lunge ?? PROX_ATTACK_LUNGE_DISTANCE) * attackSnap);
	phonePose.position.addScaledVector(vRight, attackVariant.side * 0.035 * attackSnap);
	phonePose.position.y += attackVariant.lift * attackSnap;
	phonePoseRotateX(attackVariant.pitch * attackSnap * hitWeight);
	phonePoseRotateZ(attackVariant.roll * attackRecover * hitWeight);
	phonePoseRotateY(attackVariant.yaw * attackSnap * hitWeight);
}

function applyPhoneFullWarningOverlay() {
	if (!phoneStorage.isFull) return;
	const now = frame.now;
	const cycle = (now % 1100) / 1100;
	if (cycle < 0.32) {
		phonePose.state = PHONE_POSE_STATE.FULL_WARNING;
		const burst = Math.sin((cycle / 0.32) * Math.PI);
		const ring = Math.sin(now * 0.055);
		phonePoseRotateZ(ring * 0.16 * burst);
		phonePoseRotateX(Math.abs(ring) * 0.045 * burst);
		phonePose.position.x += ring * 0.01 * burst;
	}
	setPhoneScreenEmission(0xff3333, 1.25);
	screenLight.intensity = 4.5;
}

function updatePhoneVisualPose(dt) {
	const running = isRunningInput();
	const jumpVacuumBlocked = actionPose.doubleJumpVacuumPause > 0;
	const phoneActionPose = Math.max(vacuum.pose, actionPose.dischargeTimer > 0 ? 1 : 0);
	phoneTargetPos.y += phoneActionPose * 0.65;
	phoneTargetPos.x += Math.sin(cursor.yaw) * phoneActionPose * 0.25;
	phoneTargetPos.z += Math.cos(cursor.yaw) * phoneActionPose * 0.25;
	resetPhonePoseFromCurrent();
	phonePose.position.lerp(phoneTargetPos, Math.min(1, dt * 12));
	phonePoseBasePos.copy(phonePose.position);
	const leanAmount = running ? 0.5 : 0.35;
	const axes = getMoveAxes();
	const inputMag = Math.hypot(axes.forward, axes.strafe) || 1;
	const localForwardLean = -(axes.forward / inputMag) * leanAmount;
	const localSideLean = -(axes.strafe / inputMag) * leanAmount * 0.82;
	const screenForwardPose = Math.max((vacuum.active && !jumpVacuumBlocked) ? 1 : 0, actionPose.discharge);
	actionPose.screenForwardTurn += (screenForwardPose - actionPose.screenForwardTurn) * Math.min(1, dt * 4.5);
	const easedTurn = actionPose.screenForwardTurn * actionPose.screenForwardTurn * (3 - 2 * actionPose.screenForwardTurn);
	dummy.rotation.set(0, cursor.yaw, 0);
	phoneBaseQuat.copy(dummy.quaternion);
	phoneLeanEuler.set(localForwardLean, 0, localSideLean);
	phoneLeanQuat.setFromEuler(phoneLeanEuler);
	phoneBaseQuat.multiply(phoneLeanQuat);
	camera.getWorldDirection(screenForward).normalize();
	phoneAimTarget.copy(phonePoseBasePos).addScaledVector(screenForward, 10);
	dummy.position.copy(phonePoseBasePos);
	dummy.lookAt(phoneAimTarget);
	phoneAimQuat.copy(dummy.quaternion);
	phonePose.quaternion.copy(phoneBaseQuat).slerp(phoneAimQuat, easedTurn);
	const ritualFlip = Math.sin(easedTurn * Math.PI) * 0.75;
	const doubleJumpPhase = actionPose.doubleJumpTimer > 0
		? 1 - THREE.MathUtils.clamp(actionPose.doubleJumpTimer / DOUBLE_JUMP_POSE_HOLD, 0, 1)
		: 0;
	const doubleJumpEase = doubleJumpPhase * doubleJumpPhase * (3 - 2 * doubleJumpPhase);
	const doubleJumpFlip = doubleJumpEase * Math.PI * 2;
	const vacuumWobble = Math.sin(frame.now * 0.018) * 0.035 * vacuum.pose;
	updatePhoneGait(dt, running);

	// ===== FINAL PHONE POSE ARBITRATION =====
	if (actionPose.doubleJump > 0) {
		applyDoubleJumpPhonePose(doubleJumpFlip);
	} else {
		phonePose.state = selectPhonePoseState(jumpVacuumBlocked);
		applyBasePhoneLocomotionPose(ritualFlip, vacuumWobble);
		applyProximityAttackPhoneOverlay();
		applyPhoneFullWarningOverlay();
	}

	applyPhonePose();
}

function getPlayerHorizontalSpeed() {
	return Math.hypot(player.vel.x, player.vel.z);
}

function updatePlayerAvatar() {
	updatePlayerAvatarVisibility();
}

function updateTargets() {
	const time = frame.time;
	const tileOriginZ = getPlayerRoomTileOriginZ();
	let nodeInstance = 0;
	let rodInstance = 0;
	updateScreenPlaneBasis();
	humanScreenRight.setFromMatrixColumn(camera.matrixWorld, 0).normalize();
	updateFirstPersonVacuumPullPoint();

	for (let i = 0; i < TARGET_COUNT; i++) {
		const idx = targetIndex(i);
		const liveGroundHuman = targetData[idx + T_ALIVE] > 0.5 && !soulDepositShot[i] && !isTargetSlurpable(idx) && targetIngestProgress[i] < 0.01;
		if (liveGroundHuman) {
			targetData[idx + T_Y] = HUMAN_GROUND_Y;
			targetData[idx + T_RENDER_Y] = HUMAN_GROUND_Y;
		}
		const x = targetData[idx + T_X];
		const baseY = targetData[idx + T_Y];
		const z = targetData[idx + T_Z];
		// Render the canonical enemy/soul in the player's current visible tile.
		// Vacuum physics may choose the nearest repeated copy internally, but the
		// visible infinite-room contract is: current tile + previous/next mirrors all
		// show the same full enemy set in the same state.
		const worldZ = z + getPlayerRoomTileOriginZ();
		const floatOffset = targetData[idx + T_FLOAT];
		const spinSpeed = targetData[idx + T_SPIN];
		const scale = targetData[idx + T_SCALE];

		const livingHuman = canHumanWalk(i);
		if (livingHuman) targetData[idx + T_Y] = HUMAN_GROUND_Y;
		const y = soulDepositShot[i] ? baseY : (livingHuman ? HUMAN_GROUND_Y : baseY + Math.sin(time * 0.002 + floatOffset) * 0.18);
		targetData[idx + T_RENDER_Y] = y;

		targetData[idx + T_HITFLASH] = Math.max(0, targetData[idx + T_HITFLASH] - 0.045);
		const health = targetData[idx + T_HEALTH];
		const hitFlash = THREE.MathUtils.clamp(targetData[idx + T_HITFLASH], 0, 1);
		const slurpable = isTargetSlurpable(idx);
		if (slurpable) {
			soulMorphTime[i] = Math.min(SOUL_MORPH_DURATION, soulMorphTime[i] + frame.dt);
		}
		const soulMorphPhase = slurpable
			? THREE.MathUtils.smoothstep(soulMorphTime[i] / SOUL_MORPH_DURATION, 0, 1)
			: 0;
		const armor = Math.max(0, targetData[idx + T_ARMOR]);
		const armorMax = (targetData[idx + T_KIND] || 0) === 1 ? SOUL_ARMOR_BRUTE : SOUL_ARMOR_NORMAL;
		const breakStress = slurpable ? 1 : THREE.MathUtils.clamp(1 - armor / Math.max(armorMax, 0.001), 0, 1);
		const suctionAmount = THREE.MathUtils.clamp(1 - health, 0, 1);
		const ingest = THREE.MathUtils.clamp(targetIngestProgress[i], 0, 1);
		const latched = targetLatchedToScreen[i] > 0.5;
		const livePinForTarget = vacuum.active && vacuum.power > 0.01 && (targetLock.target === i || soulState[i] === SOUL_ATTRACTED || soulState[i] === SOUL_LATCHED || soulState[i] === SOUL_INGESTING);
		const activelyPulled = livePinForTarget || ingest > 0.01;

		const sealPhase = THREE.MathUtils.smoothstep(ingest, 0.32, 0.56);
		const pressurePhase = THREE.MathUtils.smoothstep(ingest, 0.56, 0.86);
		const popPhase = THREE.MathUtils.smoothstep(ingest, 0.86, 1.0);
		const visualTarget = Math.max(
			livePinForTarget ? vacuum.power * targetLock.strength : 0,
			ingest,
			suctionAmount * 0.25,
			breakStress * 0.16,
			hitFlash * 0.18
		);

		targetPrevVisualTarget[i] = visualTarget;

		const springResult = springScalarValue(
			targetVisualPull[i],
			targetVisualPullVel[i],
			activelyPulled ? visualTarget : 0,
			activelyPulled ? 4.8 : 3.2,
			activelyPulled ? 0.78 : 0.58,
			0.016
		);
		targetVisualPull[i] = springResult[0];
		targetVisualPullVel[i] = springResult[1];

		const visualPull = THREE.MathUtils.clamp(targetVisualPull[i], 0, 1);
		const slurpStrength = THREE.MathUtils.clamp(
			Math.max(visualPull, livePinForTarget ? vacuum.power : 0),
			0,
			1
		);
		const latchLimpness = latched
			? THREE.MathUtils.clamp(0.18 + slurpStrength * 0.62 + ingest * 0.34, 0, 1)
			: 0;
		const releaseElastic = 0;
		const ingestEaseRaw = ingest * ingest * (3 - 2 * ingest);
		const ingestEase = ingestEaseRaw;
		const tetherAllowed =
			livePinForTarget &&
			vacuum.active &&
			vacuum.power > 0.01 &&
			(soulState[i] === SOUL_ATTRACTED || soulState[i] === SOUL_LATCHED || soulState[i] === SOUL_INGESTING);
		const tetherIn = tetherAllowed
			? Math.max(
				THREE.MathUtils.smoothstep(visualPull, 0.08, 0.38),
				THREE.MathUtils.smoothstep(ingest, 0.02, 0.22)
			)
			: 0;
		const collapse = Math.max(
			THREE.MathUtils.smoothstep(suctionAmount, 0.76, 1.0),
			pressurePhase * 0.55,
			popPhase
		);
		const rot = time * 0.001 * spinSpeed;
		const readyPulse = slurpable ? 1 + Math.sin(time * 0.018 + i) * 0.055 : 1;
		const hitPulse = 1 + hitFlash * 0.16;
		const idleBreath = (1 + Math.sin(time * 0.003 + i * 1.7) * 0.035) * readyPulse * hitPulse;
		const humanMorphVisible = targetData[idx + T_ALIVE] > 0.5 && !soulDepositShot[i] && slurpable && soulMorphPhase < 0.995 && ingest < 0.01;
		const humanAlive = targetData[idx + T_ALIVE] > 0.5 && !soulDepositShot[i] && !slurpable && ingest < 0.01;
		const humanVisible = humanAlive || humanMorphVisible;
		const humanStress = THREE.MathUtils.clamp(breakStress + hitFlash * 0.18, 0, 1);
		const walkPhase = targetData[idx + T_WALK_PHASE];
		const morphShrink = humanMorphVisible ? (1 - soulMorphPhase) : 1;
		const livingScale = humanVisible ? scale * (1 - humanStress * 0.25) * Math.max(0, morphShrink) : 0;
		const sprite = humanSprites[i];
		if (humanVisible) {
			const modelVisible = updateHumanModelInstance(i, true, x, worldZ, targetData[idx + T_WALK_YAW], livingScale, frame.dt, humanAlive ? targetData[idx + T_ATTACK_TIMER] : 0, targetData[idx + T_ATTACK_VARIANT]);
			if (modelVisible) {
				sprite.visible = false;
			} else {
				const walkFrame = Math.floor(walkPhase * 1.15) % humanSpriteMaterials.length;
				sprite.material = humanSpriteMaterials[walkFrame];
				const faceDx = targetData[idx + T_WALK_TX] - x;
				const faceDz = targetData[idx + T_WALK_TZ] - z;
				if (Math.hypot(faceDx, faceDz) > 0.02) {
					const screenMotion = faceDx * humanScreenRight.x + faceDz * humanScreenRight.z;
					if (Math.abs(screenMotion) > 0.01) targetData[idx + T_WALK_FACE] = screenMotion >= 0 ? 1 : -1;
				}
				const faceSign = targetData[idx + T_WALK_FACE] || 1;
				sprite.position.set(x, HUMAN_GROUND_Y + 0.55 * scale, worldZ);
				sprite.scale.set(0.82 * livingScale * faceSign, 1.18 * livingScale, 1);
				sprite.visible = true;
			}
		} else {
			updateHumanModelInstance(i, false, x, worldZ, targetData[idx + T_WALK_YAW], 0, frame.dt);
			sprite.visible = false;
		}
		const humanScale = 0;

		springNetCenter.set(x, y, worldZ);
		springNetPullDir.set(
			vacuumPullPoint.x - x,
			vacuumPullPoint.y - y,
			vacuumPullPoint.z - worldZ
		);

		if (springNetPullDir.lengthSq() > 0.001) {
			springNetPullDir.normalize();
		} else {
			springNetPullDir.set(Math.sin(rot), 0, Math.cos(rot)).normalize();
		}

		springNetSide.crossVectors(springNetPullDir, springNetUp);
		if (springNetSide.lengthSq() < 0.001) {
			springNetSide.set(1, 0, 0);
		} else {
			springNetSide.normalize();
		}

		dummy.position.copy(springNetCenter);
		const cubeMorph = slurpable ? soulMorphPhase : 0;
		if (cubeMorph > 0.001) {
			if (!soulDepositShot[i] && (activelyPulled || visualPull > 0.01)) {
				dummy.lookAt(vacuumPullPoint);
			} else if (soulDepositShot[i]) {
				dummy.rotation.set(rot * 1.2, rot * 1.7, rot * 0.9);
			} else {
				dummy.rotation.set(0, rot, 0);
			}
			let firstPersonSlurpVisibility = 1;
			if (cameraMode.current === CAMERA_MODE_FIRST && !soulDepositShot[i] &&
				(soulState[i] === SOUL_ATTRACTED || soulState[i] === SOUL_LATCHED || soulState[i] === SOUL_INGESTING)) {
				camera.worldToLocal(firstPersonSlurpCameraLocal.copy(springNetCenter));
				firstPersonSlurpVisibility = 1 - THREE.MathUtils.smoothstep(firstPersonSlurpCameraLocal.z, -0.62, -0.18);
			}
			const cubeScale = scale * cubeMorph * (soulDepositShot[i] ? 1.12 : (slurpable ? 0.78 : 0.52)) * idleBreath * firstPersonSlurpVisibility;
			dummy.scale.setScalar(cubeScale);
		} else {
			dummy.scale.setScalar(0);
		}
		dummy.updateMatrix();
		targetMesh.setMatrixAt(i, dummy.matrix);

		// Wrapped-space continuity: mirror the active gameplay-visible entities into
		// the repeated room shells. These copies are visual only. targetData, AI,
		// collision, capture, and damage remain owned by the canonical active room.
		for (let c = 0; c < ROOM_CONTINUATION_TILE_OFFSETS.length; c++) {
			const continuationInstance = i * ROOM_CONTINUATION_TILE_OFFSETS.length + c;
			const continuationOriginZ = getContinuationTileOriginZ(ROOM_CONTINUATION_TILE_OFFSETS[c]);
			if (cubeMorph > 0.001) {
				continuationMatrix.copy(dummy.matrix);
				continuationMatrix.setPosition(x, y, z + continuationOriginZ);
				continuationTargetMesh.setMatrixAt(continuationInstance, continuationMatrix);
			} else {
				setHiddenContinuationInstance(continuationTargetMesh, continuationInstance);
			}

			const continuationSprite = continuationHumanSprites[continuationInstance];
			if (humanVisible && livingScale > 0.001) {
				const continuationWorldZ = z + continuationOriginZ;
				const continuationModelVisible = updateContinuationHumanModelInstance(
					continuationInstance,
					true,
					x,
					continuationWorldZ,
					targetData[idx + T_WALK_YAW],
					livingScale,
					frame.dt,
					humanAlive ? targetData[idx + T_ATTACK_TIMER] : 0,
					targetData[idx + T_ATTACK_VARIANT]
				);
				hideContinuationHumanMirror(continuationInstance);
				if (continuationSprite) {
					if (continuationModelVisible) {
						continuationSprite.visible = false;
					} else {
						const walkFrame = Math.floor(walkPhase * 1.15) % humanSpriteMaterials.length;
						continuationSprite.material = humanSpriteMaterials[walkFrame];
						const faceSign = targetData[idx + T_WALK_FACE] || 1;
						continuationSprite.position.set(x, HUMAN_GROUND_Y + 0.55 * scale, continuationWorldZ);
						continuationSprite.scale.set(0.82 * livingScale * faceSign, 1.18 * livingScale, 1);
						continuationSprite.visible = true;
					}
				}
			} else {
				updateContinuationHumanModelInstance(continuationInstance, false, x, z + continuationOriginZ, targetData[idx + T_WALK_YAW], 0, frame.dt);
				if (continuationSprite) continuationSprite.visible = false;
				hideContinuationHumanMirror(continuationInstance);
			}
		}

		springNetAnchorSum.set(0, 0, 0);
		let anchorWeight = 0;
		let bestTipScore = -Infinity;
		let bestTipX = springNetCenter.x;
		let bestTipY = springNetCenter.y;
		let bestTipZ = springNetCenter.z;

		for (let n = 0; n < LATTICE_NODE_COUNT; n++) {
			const nodeBase = (i * LATTICE_NODE_COUNT + n) * LATTICE_STRIDE;
			const rest = latticeRest[n];

			springNetRest.copy(rest);
			const restLen = Math.max(springNetRest.length(), 0.001);
			const outward = springNetTmp.copy(springNetRest).multiplyScalar(1 / restLen);
			const facingPull = THREE.MathUtils.clamp(outward.dot(springNetPullDir) * 0.5 + 0.5, 0, 1);
			const surface = latticeSurface[n];
			const corner = latticeCorner[n];

			const breathe = Math.sin(time * 0.004 + i * 1.31 + n * 0.77) * 0.018;
			const stressWave = Math.sin(time * 0.012 + i * 2.17 + n * 1.9) * visualPull;
			const directSurfacePull = surface * visualPull * Math.pow(facingPull, 1.35);
			const faceCluster = Math.max(surface * Math.pow(facingPull, 2.4), directSurfacePull);

			const armPattern =
				surface *
				Math.pow(facingPull, 3.0) *
				(0.5 + 0.5 * Math.sin(time * 0.007 + i * 4.9 + n * 2.2));
			const cheekPattern =
				surface *
				Math.pow(facingPull, 1.7) *
				(1 - corner * 0.55) *
				(0.65 + 0.35 * Math.sin(time * 0.011 + i + n));

			springNetDesired.copy(springNetRest);
			springNetDesired.multiplyScalar(idleBreath + breathe);

			const neck = surface * Math.pow(facingPull, 4.2);
			const shoulder = surface * Math.pow(facingPull, 1.8) * (1 - neck);
			const rearLag = surface * Math.pow(1 - facingPull, 1.4);
			const axisOffset = springNetRest.dot(springNetPullDir);
			const axisPoint = springNetTmp.copy(springNetPullDir).multiplyScalar(axisOffset);
			const radial = springNetDesired.clone().sub(axisPoint);
			const taperStrength = visualPull * (neck * 0.72 + shoulder * 0.28 + collapse * facingPull * 0.55);

			springNetDesired.addScaledVector(springNetPullDir, visualPull * neck * (0.18 + armPattern * 0.08));
			springNetDesired.addScaledVector(springNetPullDir, directSurfacePull * 0.12);
			springNetDesired.addScaledVector(springNetPullDir, visualPull * shoulder * 0.10);
			springNetDesired.addScaledVector(springNetPullDir, collapse * facingPull * 0.12);
			springNetDesired.addScaledVector(radial, -taperStrength);

			springNetDesired.addScaledVector(springNetSide, stressWave * surface * (0.04 + cheekPattern * 0.04));
			springNetDesired.addScaledVector(springNetUp, Math.sin(time * 0.010 + i * 3.1 + n) * visualPull * surface * 0.045);

			springNetDesired.addScaledVector(springNetPullDir, -visualPull * rearLag * 0.13);

			if (!activelyPulled && releaseElastic > 0.001 && surface > 0.5) {
				const reboundWave = Math.sin(time * 0.026 + i * 2.7 + n * 1.41);
				const reboundAmount = releaseElastic * (0.10 + corner * 0.035) * reboundWave;
				springNetDesired.addScaledVector(outward, reboundAmount);
				springNetDesired.addScaledVector(springNetSide, releaseElastic * Math.sin(time * 0.020 + n * 0.73) * 0.035);
			}

			if (ingest > 0.001 && livePinForTarget) {
				const localDepth = THREE.MathUtils.clamp(facingPull, 0, 1);
				const nodeDelay = (1 - localDepth) * 0.36 + corner * 0.05;
				const nodeIngest = THREE.MathUtils.clamp(
					(ingestEase - nodeDelay) / Math.max(1 - nodeDelay, 0.001),
					0,
					1
				);
				const nodeEase = nodeIngest * nodeIngest * (3 - 2 * nodeIngest);

				const apertureX = THREE.MathUtils.clamp(
					(rest.x / Math.max(LATTICE_HALF, 0.001)) * SCREEN_HALF_WIDTH,
					-SCREEN_HALF_WIDTH,
					SCREEN_HALF_WIDTH
				);
				const apertureY = THREE.MathUtils.clamp(
					(rest.y / Math.max(LATTICE_HALF, 0.001)) * SCREEN_HALF_HEIGHT,
					-SCREEN_HALF_HEIGHT,
					SCREEN_HALF_HEIGHT
				);
				screen.localToWorld(springNetA.set(apertureX, apertureY, SCREEN_FRONT_OFFSET));

				const worldDesiredX = x + springNetDesired.x;
				const worldDesiredY = y + springNetDesired.y;
				const worldDesiredZ = worldZ + springNetDesired.z;

				const toScreenX = springNetA.x - worldDesiredX;
				const toScreenY = springNetA.y - worldDesiredY;
				const toScreenZ = springNetA.z - worldDesiredZ;

				const sealWeight = sealPhase * Math.pow(localDepth, 2.6);
				const pressureWeight = pressurePhase * (0.25 + localDepth * 0.55);
				const rearBulge = pressurePhase * (1 - popPhase) * Math.pow(1 - localDepth, 1.7);
				const popWeight = popPhase * (0.45 + nodeEase * 0.85);
				const swallowWeight = THREE.MathUtils.clamp(sealWeight + pressureWeight + popWeight, 0, 1);

				springNetDesired.x += toScreenX * swallowWeight;
				springNetDesired.y += toScreenY * swallowWeight;
				springNetDesired.z += toScreenZ * swallowWeight;

				const membraneSqueeze = (sealWeight * 0.35 + pressureWeight * 0.68 + popWeight) * surface;
				springNetDesired.addScaledVector(radial, -membraneSqueeze);

				springNetDesired.addScaledVector(springNetPullDir, -rearBulge * 0.18);
				springNetDesired.addScaledVector(springNetPullDir, popWeight * 0.16);

				const pressureJitter = Math.sin(time * 0.035 + i * 3.7 + n * 1.9) * pressurePhase * (1 - popPhase) * 0.018;
				springNetDesired.addScaledVector(springNetSide, pressureJitter * surface);
				springNetDesired.addScaledVector(springNetUp, pressureJitter * 0.7 * surface);
			}

			springNetCurrent.set(
				latticePos[nodeBase],
				latticePos[nodeBase + 1],
				latticePos[nodeBase + 2]
			);

			let vx = latticeVel[nodeBase];
			let vy = latticeVel[nodeBase + 1];
			let vz = latticeVel[nodeBase + 2];

			const stiffness = latched
				? 7.0 + (1 - latchLimpness) * 6.5
				: (activelyPulled ? 14.5 : (releaseElastic > 0.01 ? 22.0 : 20.0));
			const damping = latched
				? 0.84 + latchLimpness * 0.11
				: (activelyPulled ? 0.76 : (releaseElastic > 0.01 ? 0.91 : 0.66));
			const step = 0.016;

			vx += (springNetDesired.x - springNetCurrent.x) * stiffness * step;
			vy += (springNetDesired.y - springNetCurrent.y) * stiffness * step;
			vz += (springNetDesired.z - springNetCurrent.z) * stiffness * step;

			if (!activelyPulled) {
				const homeBoost = releaseElastic > 0.01 ? 0.55 : 1.15;
				vx += (springNetRest.x - springNetCurrent.x) * homeBoost * step * 60;
				vy += (springNetRest.y - springNetCurrent.y) * homeBoost * step * 60;
				vz += (springNetRest.z - springNetCurrent.z) * homeBoost * step * 60;
			}

			if (!activelyPulled && releaseElastic > 0.01 && surface > 0.5) {
				const recoilWave = Math.sin(time * 0.030 + i * 1.9 + n * 1.37);
				vx += outward.x * releaseElastic * recoilWave * 0.032;
				vy += outward.y * releaseElastic * recoilWave * 0.032;
				vz += outward.z * releaseElastic * recoilWave * 0.032;
				vx += springNetSide.x * releaseElastic * Math.sin(time * 0.025 + n) * 0.018;
				vz += springNetSide.z * releaseElastic * Math.sin(time * 0.025 + n) * 0.018;
			}

			vx *= Math.pow(damping, step * 60);
			vy *= Math.pow(damping, step * 60);
			vz *= Math.pow(damping, step * 60);

			springNetCurrent.x += vx * step;
			springNetCurrent.y += vy * step;
			springNetCurrent.z += vz * step;

			if (!activelyPulled && releaseElastic < 0.01) {
				const dx = springNetCurrent.x - springNetRest.x;
				const dy = springNetCurrent.y - springNetRest.y;
				const dz = springNetCurrent.z - springNetRest.z;
				const distSq = dx * dx + dy * dy + dz * dz;
				const velSq = vx * vx + vy * vy + vz * vz;
				if (distSq < 0.00008 && velSq < 0.00008) {
					springNetCurrent.copy(springNetRest);
					vx = 0;
					vy = 0;
					vz = 0;
				}
			}

			latticePos[nodeBase] = springNetCurrent.x;
			latticePos[nodeBase + 1] = springNetCurrent.y;
			latticePos[nodeBase + 2] = springNetCurrent.z;
			latticeVel[nodeBase] = vx;
			latticeVel[nodeBase + 1] = vy;
			latticeVel[nodeBase + 2] = vz;

			springNetWorld.copy(springNetCenter).add(springNetCurrent);

			if (faceCluster > 0.35) {
				springNetAnchorSum.addScaledVector(springNetWorld, faceCluster);
				anchorWeight += faceCluster;
			}

			const tipDist = springNetWorld.distanceTo(vacuumPullPoint);
			const tipScore = faceCluster * 4.0 + visualPull * facingPull - tipDist * 0.18;
			if (surface > 0.5 && tipScore > bestTipScore) {
				bestTipScore = tipScore;
				bestTipX = springNetWorld.x;
				bestTipY = springNetWorld.y;
				bestTipZ = springNetWorld.z;
			}

			dummy.position.copy(springNetWorld);
			const nodeSwallowShrink = 1 - ingestEase * Math.pow(facingPull, 2.2) * 0.42 - popPhase * 0.48;
			const nodeScale = Math.max(0, (0.72 + surface * 0.35 + visualPull * faceCluster * 0.62 + Math.abs(stressWave) * 0.12) * nodeSwallowShrink);
			const signedScreenDistance = springNetTmp.copy(springNetWorld).sub(screenPlaneCenter).dot(screenPlaneNormal);
			const screenCutoff = latched && popPhase > 0.25 && signedScreenDistance < -0.002;
			const solidPhoneMask = isWorldPointInsidePhoneSolid(springNetWorld, PHONE_SOLID_VISUAL_PADDING);
			dummy.scale.setScalar((screenCutoff || solidPhoneMask) ? 0 : nodeScale);
			dummy.updateMatrix();
			latticeNodeMesh.setMatrixAt(nodeInstance++, dummy.matrix);
		}

		if (anchorWeight > 0.001) {
			springNetAnchor.copy(springNetAnchorSum).multiplyScalar(1 / anchorWeight);
			springNetAnchor.lerp(springNetTmp.set(bestTipX, bestTipY, bestTipZ), 0.62);
		} else {
			springNetAnchor.copy(springNetCenter).addScaledVector(springNetPullDir, 0.38);
		}


		for (let r = 0; r < LATTICE_RODS_PER_TARGET; r++) {
			const pair = latticePairs[r];
			const aBase = (i * LATTICE_NODE_COUNT + pair[0]) * LATTICE_STRIDE;
			const bBase = (i * LATTICE_NODE_COUNT + pair[1]) * LATTICE_STRIDE;

			springNetA.set(
				x + latticePos[aBase],
				y + latticePos[aBase + 1],
				worldZ + latticePos[aBase + 2]
			);
			springNetB.set(
				x + latticePos[bBase],
				y + latticePos[bBase + 1],
				worldZ + latticePos[bBase + 2]
			);

			const dist = springNetA.distanceTo(springNetB);
			dummy.position.copy(springNetA).lerp(springNetB, 0.5);
			dummy.lookAt(springNetB);
			const rodStress = THREE.MathUtils.clamp((dist - LATTICE_SPACING) / 0.35, 0, 1);
			const aBehind = springNetTmp.copy(springNetA).sub(screenPlaneCenter).dot(screenPlaneNormal) < -0.002;
			const bBehind = springNetTmp.copy(springNetB).sub(screenPlaneCenter).dot(screenPlaneNormal) < -0.002;
			const rodInsidePhone =
				isWorldPointInsidePhoneSolid(springNetA, PHONE_SOLID_VISUAL_PADDING) ||
				isWorldPointInsidePhoneSolid(springNetB, PHONE_SOLID_VISUAL_PADDING);
			if (rodInsidePhone || (latched && ingest > 0.56 && (aBehind || bBehind))) {
				dummy.scale.setScalar(0);
			} else {
				dummy.scale.set(
					0.68 + rodStress * 0.22,
					0.68 + rodStress * 0.22,
					dist
				);
			}
			dummy.updateMatrix();
			latticeRodMesh.setMatrixAt(rodInstance++, dummy.matrix);
		}

		const surfaceMesh = targetSurfaceMeshes[i];
		surfaceMesh.visible = slurpable || ingest > 0.01 || visualPull > 0.25;
		const surfacePositions = targetSurfacePositions[i];
		for (let v = 0; v < surfaceNodeIds.length; v++) {
			const n = surfaceNodeIds[v];
			const nodeBase = (i * LATTICE_NODE_COUNT + n) * LATTICE_STRIDE;
			springNetTmp.set(
				latticePos[nodeBase],
				latticePos[nodeBase + 1],
				latticePos[nodeBase + 2]
			);

			if (latched) {
				const rest = latticeRest[n];
				const restLen = Math.max(rest.length(), 0.001);
				const localDepth = THREE.MathUtils.clamp(
					(rest.x * springNetPullDir.x + rest.y * springNetPullDir.y + rest.z * springNetPullDir.z) / restLen * 0.5 + 0.5,
					0,
					1
				);
				const rear = 1 - localDepth;
				const limp = THREE.MathUtils.clamp(
					latchLimpness + sealPhase * 0.16 + pressurePhase * 0.18,
					0,
					1
				);
				const sag = limp * rear * (1 - popPhase) * (0.20 + slurpStrength * 0.24);
				const smear = rear * limp * (1 - popPhase) * (0.12 + slurpStrength * 0.20);
				const flatten = limp * surfaceNodeIds.length > 0 ? limp : 0;

				springNetTmp.y -= sag;
				springNetTmp.addScaledVector(springNetPullDir, -smear);
				springNetTmp.multiplyScalar(1 - flatten * rear * 0.10);

				springNetWorld.copy(springNetCenter).add(springNetTmp);
				const minFrontDistance = 0.006 + rear * limp * (0.018 + slurpStrength * 0.022);
				const signedScreenDistance = springNetWorld.sub(screenPlaneCenter).dot(screenPlaneNormal);
				if (popPhase < 0.72 && signedScreenDistance < minFrontDistance) {
					springNetTmp.addScaledVector(
						screenPlaneNormal,
						minFrontDistance - signedScreenDistance
					);
				}
			}

			surfacePositions[v * 3] = springNetTmp.x;
			surfacePositions[v * 3 + 1] = springNetTmp.y;
			surfacePositions[v * 3 + 2] = springNetTmp.z;
		}
		surfaceMesh.position.copy(springNetCenter);
		surfaceMesh.geometry.attributes.position.needsUpdate = true;
		surfaceMesh.geometry.computeVertexNormals();

		if (tetherAllowed && tetherIn > 0.01) {
			const midX = (springNetAnchor.x + vacuumPullPoint.x) * 0.5;
			const midY = (springNetAnchor.y + vacuumPullPoint.y) * 0.5;
			const midZ = (springNetAnchor.z + vacuumPullPoint.z) * 0.5;
			const rawDist = springNetAnchor.distanceTo(vacuumPullPoint);
			const dist = Math.min(rawDist, 4.5);

			dummy.position.set(midX, midY, midZ);
			dummy.lookAt(vacuumPullPoint);

			const tetherWobble = Math.sin(time * 0.018 + i * 4.1) * 0.10 * visualPull;
			dummy.rotateX(tetherWobble);
			dummy.rotateZ(tetherWobble * 0.7);

			const neckWidth = (0.92 - visualPull * 0.62 - collapse * 0.18 - ingestEase * 0.26) * tetherIn;
			dummy.scale.set(
				Math.max(0.12, neckWidth),
				Math.max(0.12, neckWidth),
				dist * tetherIn
			);

			dummy.updateMatrix();
			strandMesh.setMatrixAt(i, dummy.matrix);
		} else {
			dummy.position.set(0, -999, 0);
			dummy.scale.setScalar(0);
			dummy.updateMatrix();
			strandMesh.setMatrixAt(i, dummy.matrix);
		}
	}



	targetMesh.instanceMatrix.needsUpdate = true;
	humanHeadMesh.instanceMatrix.needsUpdate = true;
	humanTorsoMesh.instanceMatrix.needsUpdate = true;
	humanArmMesh.instanceMatrix.needsUpdate = true;
	humanLegMesh.instanceMatrix.needsUpdate = true;
	continuationTargetMesh.instanceMatrix.needsUpdate = true;
	continuationHumanHeadMesh.instanceMatrix.needsUpdate = true;
	continuationHumanTorsoMesh.instanceMatrix.needsUpdate = true;
	continuationHumanArmMesh.instanceMatrix.needsUpdate = true;
	continuationHumanLegMesh.instanceMatrix.needsUpdate = true;
	strandMesh.instanceMatrix.needsUpdate = true;
	latticeNodeMesh.instanceMatrix.needsUpdate = true;
	latticeRodMesh.instanceMatrix.needsUpdate = true;
}

function updateScreenFlash() {
	const elapsed = frame.now - game.screenFlashTime;
	if (elapsed < 120) {
		const t = elapsed / 120;
		setPhoneScreenEmission(0xffffff, 2.8 - 2.2 * t);
		screenLight.intensity = 24 - 22 * t;
	} else {
		setPhoneScreenEmission(0x12304a, 0.75);
		screenLight.intensity = 0;
	}
}

const crosshairState = {
	hasAimTarget: false,
	vacuumSpin: 0
};

function updateCrosshair() {
	const meleeActive = proximityAttack.visualTimer > 0 || proximityAttack.dashTimer > 0 || proximityAttack.pose > 0.02;
	if (crosshair) {
		crosshair.style.display = game.started && !game.dead && !meleeActive ? "block" : "none";
	}
	if (meleeActive) {
		crosshairState.hasAimTarget = false;
		return;
	}

	camera.getWorldPosition(shotOrigin);
	camera.getWorldDirection(shotDir);
	shotDir.normalize();

	const candidate = rayHitsTarget(shotOrigin, shotDir);
	crosshairState.hasAimTarget = targetLock.target !== -1 || candidate !== -1;
}


function updateRuntimeDiagnostics(dt) {
	graphicsFpsState.frames += 1;
	graphicsFpsState.accum += dt;
	if (graphicsFpsState.accum >= 0.25) {
		graphicsFpsState.value = Math.round(graphicsFpsState.frames / Math.max(0.001, graphicsFpsState.accum));
		graphicsFpsState.frames = 0;
		graphicsFpsState.accum = 0;
		if (graphicsFpsCounter) graphicsFpsCounter.textContent = `FPS: ${graphicsFpsState.value}`;
	}
	if (!graphicsSettings.runtimeDebug || !runtimeDebugOverlay) return;
	runtimeDebugState.accum += dt;
	if (runtimeDebugState.accum < 0.12) return;
	runtimeDebugState.accum = 0;
	const stored = typeof getStoredCount === "function" ? getStoredCount() : (phoneStorage ? phoneStorage.storedCubes.length : 0);
	const lines = [
		`STATE    started=${game.started} dead=${game.dead} paused=${game.uiPaused} mode=${game.mode}`,
		`ROOM     index=${room.index} identity=lab enemies=humans souls=${room.depositedSouls}/${room.requiredSouls} door=${room.doorOpen ? "open" : "locked"}`,
		`RULES    count=${runRules.active.length} active=${getRunRuleSummary()} req=${getRunScalar("requiredSouls", ROOM_REQUIRED_SOULS)} enemies+${getRunScalar("extraEnemies", 0)} slurp×${getRunScalar("slurpRate", 1).toFixed(2)}`,
		`AVATAR   active=phone phoneVisible=${phone.visible ? "yes" : "no"}`,
		`PLAYER   x=${player.pos.x.toFixed(2)} y=${player.pos.y.toFixed(2)} z=${player.pos.z.toFixed(2)} v=${player.vel.length().toFixed(2)}`,
		`BATTERY  main=${battery.value.toFixed(1)} supplemental=${supplementalBattery.value.toFixed(1)} stacks=${activePowerupStacks.flower || 0}`,
		`SOULS    stored=${stored} pendingShots=${pendingDischargedSouls.length} flowers=${flowerPowerups.length} flowerPool=${flowerPowerupPool.length}`,
		`DOOR     mode=${roomTopology.mode} prevTile=${roomTopology.previousTileIndex} tile=${roomTopology.currentTileIndex}`,
		`GRAPHICS preset=${graphicsSettings.preset} legacy=${isLegacyGraphicsMode() ? "yes" : "no"} scale=${Math.round(graphicsSettings.renderScale * 100)}% portalWindow=off particles=${particlesRuntimeEnabled() ? "on" : "off"}`,
		`MOSH     active=${doorDataMoshState.active ? "yes" : "no"} strength=${doorDataMoshState.strength.toFixed(2)} hotkey=M`,
		`FPS      ${graphicsFpsState.value}`
	];
	runtimeDebugState.text = lines.join("\n");
	runtimeDebugOverlay.textContent = runtimeDebugState.text;
}

function updateFrame(time = 0) {
	frame.time = time;
	frame.now = performance.now();
	frame.dt = Math.min(clock.getDelta(), 0.1);
	const dt = frame.dt;
	updateRuntimeDiagnostics(dt);
	if (!game.started || game.dead) {
		updateBatteryHud();
		return;
	}
	if (game.uiPaused) {
		clock.getDelta();
		updateCamera();
		updateScreenFlash();
		updateTargets();
		updateCrosshair();
		applyCrosshairArmTransforms();
		updateSoulWindowMotion();
		return;
	}
	updateMovement(dt);
	processPendingDischargedSouls(dt);
	updateHumanWalking(dt);
	updateVacuum(dt);
	updateBattery(dt);
	if (!game.started || game.dead) return;
	processQueuedSoulCaptures();
	updateRoomPopulation(dt);
	updateFlowerPowerups(dt);
	updateRoomLoop(dt);
	updateCamera();
	updateScreenFlash();
	updateTargets();
	updateParticles(dt);
	updateProximityAttackVisuals(dt);
	updateCrosshair();
	applyCrosshairArmTransforms();
	updateSoulWindowMotion();
}

function animate(time = 0) {
	requestAnimationFrame(animate);
	updateFrame(time);
	renderer.render(scene, camera);
	captureLastWorldDatamoshFrame();
	renderDoorDataMoshFeedback();
}


function hideInstancedGameplayVisuals() {
	// Defensive startup/reset guard: InstancedMesh instances default to identity matrices.
	// If the first playable frame renders before updateTargets() writes every instance,
	// a default soul-cube instance can sit at the world origin/camera and fill the screen.
	const hidden = new THREE.Matrix4().makeScale(0, 0, 0);
	for (let i = 0; i < TARGET_COUNT; i++) {
		targetMesh.setMatrixAt(i, hidden);
		strandMesh.setMatrixAt(i, hidden);
		humanHeadMesh.setMatrixAt(i, hidden);
		humanTorsoMesh.setMatrixAt(i, hidden);
		humanArmMesh.setMatrixAt(i * 2, hidden);
		humanArmMesh.setMatrixAt(i * 2 + 1, hidden);
		humanLegMesh.setMatrixAt(i * 2, hidden);
		humanLegMesh.setMatrixAt(i * 2 + 1, hidden);
	}
	for (let i = 0; i < CONTINUATION_INSTANCE_COUNT; i++) {
		setHiddenContinuationInstance(continuationTargetMesh, i);
		hideContinuationHumanMirror(i);
	}
	for (let i = 0; i < LATTICE_TOTAL_NODES; i++) latticeNodeMesh.setMatrixAt(i, hidden);
	for (let i = 0; i < LATTICE_TOTAL_RODS; i++) latticeRodMesh.setMatrixAt(i, hidden);
	targetMesh.instanceMatrix.needsUpdate = true;
	strandMesh.instanceMatrix.needsUpdate = true;
	humanHeadMesh.instanceMatrix.needsUpdate = true;
	humanTorsoMesh.instanceMatrix.needsUpdate = true;
	humanArmMesh.instanceMatrix.needsUpdate = true;
	humanLegMesh.instanceMatrix.needsUpdate = true;
	continuationTargetMesh.instanceMatrix.needsUpdate = true;
	continuationHumanHeadMesh.instanceMatrix.needsUpdate = true;
	continuationHumanTorsoMesh.instanceMatrix.needsUpdate = true;
	continuationHumanArmMesh.instanceMatrix.needsUpdate = true;
	continuationHumanLegMesh.instanceMatrix.needsUpdate = true;
	latticeNodeMesh.instanceMatrix.needsUpdate = true;
	latticeRodMesh.instanceMatrix.needsUpdate = true;
}

hideInstancedGameplayVisuals();
setBootReady();
animate();


function applyCrosshairArmTransforms() {
	if (!crosshair || !crosshairRotor || Object.values(crosshairArms).some((arm) => !arm)) return;

	const hasLock =
		typeof targetLock.target !== "undefined" && targetLock.target !== -1;

	const vacuuming =
		typeof vacuum.active !== "undefined" && vacuum.active;

	const now = frame.now;
	const spinDt = Math.min(frame.dt, 0.05);

	let spread = 0;
	let spin = 0;
	const shootJoinAge = now - shootState.cursorJoinTime;
	const shootJoinActive = shootJoinAge >= 0 && shootJoinAge < 180;
	const shootJoinEase = shootJoinActive ? 1 - THREE.MathUtils.clamp(shootJoinAge / 180, 0, 1) : 0;

	if (game.mode === "laser") {
		spread = 0;
		spin = hasLock || crosshairState.hasAimTarget ? 45 : 0;
		crosshairState.vacuumSpin += (0 - crosshairState.vacuumSpin) * Math.min(1, spinDt * 12);
	} else {
		const t = now * 0.003;
		const hover = Math.sin(t) * 2;
		const slurpStrength = vacuuming
			? THREE.MathUtils.clamp(vacuum.power * (0.72 + targetLock.strength * 0.28), 0, 1)
			: 0;

		spread = vacuuming ? 15 + slurpStrength * 8 + hover : 8 + hover;

		if (slurpStrength > 0.01) {
			const spinSpeed = 80 + slurpStrength * 620;
			crosshairState.vacuumSpin = (crosshairState.vacuumSpin + spinSpeed * spinDt) % 360;
		} else {
			crosshairState.vacuumSpin += (0 - crosshairState.vacuumSpin) * Math.min(1, spinDt * 8);
		}
		spin = crosshairState.vacuumSpin;
	}

	if (shootJoinActive) {
		// Fired soul accuracy cue: arms snap into one joined cross, then reopen into the normal active cursor.
		spread = THREE.MathUtils.lerp(spread, 0, shootJoinEase);
		spin = THREE.MathUtils.lerp(spin, 45, shootJoinEase);
	}

	crosshairRotor.style.transform = `rotate(${spin}deg)`;
	crosshairArms.top.style.transform = `translate(0px, ${-spread}px)`;
	crosshairArms.bottom.style.transform = `translate(0px, ${spread}px)`;
	crosshairArms.left.style.transform = `translate(${-spread}px, 0px)`;
	crosshairArms.right.style.transform = `translate(${spread}px, 0px)`;

	crosshair.classList.toggle("vacuum-mode", game.mode === "vacuum");
	crosshair.classList.toggle("laser-mode", game.mode === "laser");
	crosshair.classList.toggle("shoot-join", shootJoinActive);
	crosshair.classList.toggle("locked", game.mode === "laser" && (hasLock || crosshairState.hasAimTarget));
}
