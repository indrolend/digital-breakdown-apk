#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "HumanVisual.hpp"
#include "VisualIdentity.hpp"
#include "Math.hpp"
#include "PhoneDisplay.hpp"

#define private public
#include "Game.hpp"
#undef private

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr float kEpsilon = 0.0001f;

bool near(float a, float b, float epsilon = kEpsilon) {
  return std::fabs(a - b) <= epsilon;
}

bool sameVec(const Vec3& a, const Vec3& b) {
  return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}

bool sameQuat(const Quat& a, const Quat& b) {
  return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z) && near(a.w, b.w);
}

const char* cueName(AudioCue cue) {
  switch (cue) {
    case AudioCue::LowPower: return "LowPower";
    case AudioCue::VcInvitation: return "VcInvitation";
    default: return "Other";
  }
}

bool hasCueAfter(const AudioState& audio, AudioCue cue, unsigned int serial) {
  for (const auto& event : audio.events) {
    if (event.serial > serial && event.cue == cue) return true;
  }
  return false;
}

int countCueAfter(const AudioState& audio, AudioCue cue, unsigned int serial) {
  int count = 0;
  for (const auto& event : audio.events) {
    if (event.serial > serial && event.cue == cue) ++count;
  }
  return count;
}

void clearWorldActivity(GameState& state) {
  for (auto& target : state.targets) target = TargetState{};
  for (auto& bullet : state.bullets) bullet = BulletState{};
  for (auto& pending : state.pendingShots) pending = PendingShotState{};
  for (auto& flower : state.flowers) flower = FlowerPowerupState{};
  for (auto& particle : state.particles) particle = ParticleState{};
}

void makeIdlePlayer(PlayerState& player) {
  player.alive = true;
  player.downed = false;
  player.battery = 80.0f;
  player.grounded = true;
  player.jumpVel = 0.0f;
  player.vel = {};
  player.grabbedByTarget = -1;
  player.ledgeHanging = false;
  player.ledgeMantleTimer = 0.0f;
}

Game makeHostWithActivePeer(int peerId = 1) {
  Game game;
  game.reset();
  game.configureNetworkHost();
  game.setNetworkPeerActive(peerId, true);
  GameState& state = game.networkMutableState();
  clearWorldActivity(state);
  makeIdlePlayer(state.player);
  for (auto& peer : state.multiplayer.peers) {
    if (!peer.active) continue;
    makeIdlePlayer(peer.player);
    peer.input = InputState{};
    peer.energy = EnergyState{};
    peer.vacuum = VacuumState{};
    peer.camera = state.camera;
    peer.meleeVisual = MeleeVisualState{};
  }
  return game;
}

bool progressionTimersAdvanceOnce() {
  Game game = makeHostWithActivePeer(1);
  GameState& state = game.networkMutableState();
  state.progression.run.batteryRegenLock = 1.0f;
  state.progression.run.headshotRegenTax = 0.5f;
  state.progression.run.relayPrimerTimer = 0.75f;
  state.progression.run.impactGuardTimer = 0.66f;
  state.progression.run.lastStandCooldown = 0.55f;
  state.progression.run.lungeReboundTimer = 0.44f;
  state.progression.run.headshotRechargeBoost = 0.33f;

  const RunProgressionState before = state.progression.run;
  game.update(kDt);
  const RunProgressionState after = game.state().progression.run;

  const float expectedLock = before.batteryRegenLock - kDt;
  const float expectedTax = before.headshotRegenTax - kDt * 0.11f;
  const float expectedRelay = before.relayPrimerTimer - kDt;
  const float expectedImpact = before.impactGuardTimer - kDt;
  const float expectedLastStand = before.lastStandCooldown - kDt;
  const float expectedRebound = before.lungeReboundTimer - kDt;
  const float expectedBoost = before.headshotRechargeBoost - kDt;

  std::printf(
      "PROGRESSION_TIMER_OBSERVED activePeers=1 dt=%.6f "
      "batteryRegenLock %.6f->%.6f expectedOnce=%.6f expectedTwice=%.6f "
      "headshotRegenTax %.6f->%.6f expectedOnce=%.6f expectedTwice=%.6f "
      "relayPrimerTimer %.6f->%.6f expectedOnce=%.6f expectedTwice=%.6f "
      "impactGuardTimer %.6f->%.6f expectedOnce=%.6f expectedTwice=%.6f "
      "lastStandCooldown %.6f->%.6f expectedOnce=%.6f expectedTwice=%.6f "
      "lungeReboundTimer %.6f->%.6f expectedOnce=%.6f expectedTwice=%.6f "
      "headshotRechargeBoost %.6f->%.6f expectedOnce=%.6f expectedTwice=%.6f\n",
      kDt,
      before.batteryRegenLock, after.batteryRegenLock, expectedLock,
      before.batteryRegenLock - 2.0f * kDt,
      before.headshotRegenTax, after.headshotRegenTax, expectedTax,
      before.headshotRegenTax - 2.0f * kDt * 0.11f,
      before.relayPrimerTimer, after.relayPrimerTimer, expectedRelay,
      before.relayPrimerTimer - 2.0f * kDt,
      before.impactGuardTimer, after.impactGuardTimer, expectedImpact,
      before.impactGuardTimer - 2.0f * kDt,
      before.lastStandCooldown, after.lastStandCooldown, expectedLastStand,
      before.lastStandCooldown - 2.0f * kDt,
      before.lungeReboundTimer, after.lungeReboundTimer, expectedRebound,
      before.lungeReboundTimer - 2.0f * kDt,
      before.headshotRechargeBoost, after.headshotRechargeBoost, expectedBoost,
      before.headshotRechargeBoost - 2.0f * kDt);

  return near(after.batteryRegenLock, expectedLock) &&
         near(after.headshotRegenTax, expectedTax) &&
         near(after.relayPrimerTimer, expectedRelay) &&
         near(after.impactGuardTimer, expectedImpact) &&
         near(after.lastStandCooldown, expectedLastStand) &&
         near(after.lungeReboundTimer, expectedRebound) &&
         near(after.headshotRechargeBoost, expectedBoost);
}

bool remoteActionsDoNotEmitHostAudio() {
  Game game = makeHostWithActivePeer(1);
  GameState& state = game.networkMutableState();
  state.audio = AudioState{};
  const unsigned int beforeSerial = state.audio.nextSerial;
  auto& peer = state.multiplayer.peers[1];
  peer.player.battery = 24.03f;
  peer.input.touchMoveZ = 1.0f;
  peer.input.touchSprint = true;
  peer.input.touchPrimaryHeld = true;
  peer.vacuum.power = 1.0f;

  game.updateNetworkPeers(kDt);

  const AudioState& audio = game.state().audio;
  const bool lowPowerEmitted = hasCueAfter(audio, AudioCue::LowPower, beforeSerial - 1u);
  const int lowPowerCount = countCueAfter(audio, AudioCue::LowPower, beforeSerial - 1u);
  std::printf(
      "REMOTE_AUDIO_OBSERVED beforeSerial=%u afterNextSerial=%u lowPowerCount=%d "
      "firstCueAfter=%s peerBattery=%.6f\n",
      beforeSerial, audio.nextSerial, lowPowerCount,
      lowPowerEmitted ? cueName(AudioCue::LowPower) : "None",
      game.state().multiplayer.peers[1].player.battery);

  return audio.nextSerial == beforeSerial && !lowPowerEmitted;
}

void seedLocalContext(GameState& state) {
  state.input.forward = true;
  state.input.right = true;
  state.input.touchMoveX = 0.37f;
  state.input.touchMoveZ = -0.42f;
  state.input.lookDeltaX = 13.0f;
  state.input.lookDeltaY = -7.0f;
  state.input.commSignalPressed = 3;
  state.player.pos = {1.25f, 0.19f, -2.5f};
  state.player.vel = {0.5f, 0.0f, -0.75f};
  state.player.jumpVel = 1.125f;
  state.player.yaw = 0.70f;
  state.player.targetYaw = 0.72f;
  state.player.battery = 63.5f;
  state.player.souls = 2;
  state.player.airJumpsRemaining = 0;
  state.player.coyoteTimer = 0.04f;
  state.player.commSignal = 2;
  state.player.commSignalTimer = 1.6f;
  state.energy.supplementalActive = true;
  state.energy.supplementalValue = 12.5f;
  state.energy.comboHits = 3;
  state.energy.comboMultiplier = 1.44f;
  state.energy.dischargeTimer = 0.22f;
  state.camera.yaw = 0.31f;
  state.camera.pitch = -0.18f;
  state.camera.firstPerson = true;
  state.vacuum.active = true;
  state.vacuum.power = 0.77f;
  state.vacuum.pose = 0.68f;
  state.vacuum.target = 5;
  state.phonePose.roll = 0.22f;
  state.phonePose.pitch = -0.11f;
  state.phonePose.yaw = 0.33f;
  state.phonePose.actionState = 6;
  state.phoneTransform.position = {1.0f, 2.0f, 3.0f};
  state.phoneTransform.orientation = quatAxisAngle({0.0f, 1.0f, 0.0f}, 0.4f);
  state.phoneVisual.actionLift = 0.41f;
  state.phoneVisual.actionForward = 0.17f;
  state.hud.batteryFill = 0.635f;
  state.hud.storedSouls = 2;
  state.hud.crosshairOpacity = 0.52f;
  state.hud.vacuumField = 0.77f;
  state.hud.hasTarget = true;
  state.meleeVisual.actionSequence = 42;
  state.meleeVisual.visualTimer = 0.19f;
  state.meleeVisual.comboIndex = 2;
  state.meleeCooldown = 0.27f;
  state.meleePose = 0.84f;
  state.meleeComboWindow = 0.39f;
}

bool localContextRestoresExactly() {
  Game game = makeHostWithActivePeer(1);
  GameState& state = game.networkMutableState();
  seedLocalContext(state);
  auto& peer = state.multiplayer.peers[1];
  peer.input.touchMoveX = -1.0f;
  peer.input.touchMoveZ = 1.0f;
  peer.input.touchSprint = true;
  peer.input.touchPrimaryHeld = true;
  peer.player.pos = {-3.0f, 0.08f, 4.0f};
  peer.player.battery = 42.0f;
  peer.camera.yaw = -0.9f;
  peer.vacuum.power = 1.0f;

  NetworkPeerState before;
  game.savePlayerContext(before);
  game.updateNetworkPeers(kDt);
  NetworkPeerState after;
  game.savePlayerContext(after);

  const bool restored =
      before.input.forward == after.input.forward &&
      before.input.right == after.input.right &&
      near(before.input.touchMoveX, after.input.touchMoveX) &&
      near(before.input.touchMoveZ, after.input.touchMoveZ) &&
      near(before.input.lookDeltaX, after.input.lookDeltaX) &&
      near(before.input.lookDeltaY, after.input.lookDeltaY) &&
      before.input.commSignalPressed == after.input.commSignalPressed &&
      sameVec(before.player.pos, after.player.pos) &&
      sameVec(before.player.vel, after.player.vel) &&
      near(before.player.jumpVel, after.player.jumpVel) &&
      near(before.player.yaw, after.player.yaw) &&
      near(before.player.targetYaw, after.player.targetYaw) &&
      near(before.player.battery, after.player.battery) &&
      before.player.souls == after.player.souls &&
      before.player.airJumpsRemaining == after.player.airJumpsRemaining &&
      near(before.player.coyoteTimer, after.player.coyoteTimer) &&
      before.player.commSignal == after.player.commSignal &&
      near(before.player.commSignalTimer, after.player.commSignalTimer) &&
      before.energy.supplementalActive == after.energy.supplementalActive &&
      near(before.energy.supplementalValue, after.energy.supplementalValue) &&
      before.energy.comboHits == after.energy.comboHits &&
      near(before.energy.comboMultiplier, after.energy.comboMultiplier) &&
      near(before.energy.dischargeTimer, after.energy.dischargeTimer) &&
      near(before.camera.yaw, after.camera.yaw) &&
      near(before.camera.pitch, after.camera.pitch) &&
      before.camera.firstPerson == after.camera.firstPerson &&
      before.vacuum.active == after.vacuum.active &&
      near(before.vacuum.power, after.vacuum.power) &&
      near(before.vacuum.pose, after.vacuum.pose) &&
      before.vacuum.target == after.vacuum.target &&
      near(before.phonePose.roll, after.phonePose.roll) &&
      near(before.phonePose.pitch, after.phonePose.pitch) &&
      near(before.phonePose.yaw, after.phonePose.yaw) &&
      before.phonePose.actionState == after.phonePose.actionState &&
      sameVec(before.phoneTransform.position, after.phoneTransform.position) &&
      sameQuat(before.phoneTransform.orientation, after.phoneTransform.orientation) &&
      near(before.phoneVisual.actionLift, after.phoneVisual.actionLift) &&
      near(before.phoneVisual.actionForward, after.phoneVisual.actionForward) &&
      near(before.hud.batteryFill, after.hud.batteryFill) &&
      before.hud.storedSouls == after.hud.storedSouls &&
      near(before.hud.crosshairOpacity, after.hud.crosshairOpacity) &&
      near(before.hud.vacuumField, after.hud.vacuumField) &&
      before.hud.hasTarget == after.hud.hasTarget &&
      before.meleeVisual.actionSequence == after.meleeVisual.actionSequence &&
      near(before.meleeVisual.visualTimer, after.meleeVisual.visualTimer) &&
      before.meleeVisual.comboIndex == after.meleeVisual.comboIndex &&
      near(before.meleeCooldown, after.meleeCooldown) &&
      near(before.meleePose, after.meleePose) &&
      near(before.meleeComboWindow, after.meleeComboWindow);

  std::printf(
      "LOCAL_CONTEXT_RESTORE_OBSERVED restored=%d "
      "posBefore=(%.6f,%.6f,%.6f) posAfter=(%.6f,%.6f,%.6f) "
      "lookBefore=(%.6f,%.6f) lookAfter=(%.6f,%.6f) "
      "batteryBefore=%.6f batteryAfter=%.6f "
      "cameraBefore=(%.6f,%.6f,%d) cameraAfter=(%.6f,%.6f,%d) "
      "meleeBefore=(%u,%.6f,%.6f) meleeAfter=(%u,%.6f,%.6f)\n",
      restored ? 1 : 0,
      before.player.pos.x, before.player.pos.y, before.player.pos.z,
      after.player.pos.x, after.player.pos.y, after.player.pos.z,
      before.input.lookDeltaX, before.input.lookDeltaY,
      after.input.lookDeltaX, after.input.lookDeltaY,
      before.player.battery, after.player.battery,
      before.camera.yaw, before.camera.pitch, before.camera.firstPerson ? 1 : 0,
      after.camera.yaw, after.camera.pitch, after.camera.firstPerson ? 1 : 0,
      before.meleeVisual.actionSequence, before.meleeVisual.visualTimer,
      before.meleeCooldown,
      after.meleeVisual.actionSequence, after.meleeVisual.visualTimer,
      after.meleeCooldown);

  return restored;
}

}  // namespace

int main() {
  bool ok = true;
  ok &= progressionTimersAdvanceOnce();
  ok &= remoteActionsDoNotEmitHostAudio();
  ok &= localContextRestoresExactly();
  if (!ok) {
    std::fprintf(stderr, "HOST_REMOTE_PEER_SIMULATION_ISOLATION_FAILED\n");
    return 1;
  }
  std::printf("HOST_REMOTE_PEER_SIMULATION_ISOLATION_OK\n");
  return 0;
}
