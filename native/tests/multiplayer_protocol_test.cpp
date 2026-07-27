#include "MultiplayerProtocol.hpp"
#include <cmath>
#include <cstdio>

int main() {
  using namespace dbnet;
  bool ok = true;
  InputCommand input;
  input.sequence = 7;
  input.localTick = 99;
  input.moveX = -0.5f;
  input.moveZ = 0.75f;
  input.yaw = 1.25f;
  input.pitch = -0.4f;
  input.buttons = Vacuum | Sprint;
  auto bytes = encodeInput(2, input);
  PacketHeader h;
  InputCommand decoded;
  ok &= decodeInput(bytes.data(), bytes.size(), h, decoded) &&
        h.playerId == 2 && decoded.sequence == 7 &&
        decoded.localTick == 99 &&
        decoded.buttons == (Vacuum | Sprint) &&
        std::abs(decoded.moveZ - 0.75f) < 0.0001f;
  Game commandGame;
  commandGame.reset();
  commandGame.setTouchControls(0.25f, 1.0f, 0.0f, 0.0f, true, true,
                               true, true, false, false);
  const PlayerCommand canonical = commandGame.capturePlayerCommand(41, 73);
  ok &= canonical.sequence == 41 && canonical.localTick == 73 &&
        canonical.moveX > 0.24f && canonical.moveZ > 0.99f &&
        (canonical.buttons & CommandSprint) != 0 &&
        (canonical.buttons & CommandJump) != 0 &&
        (canonical.buttons & CommandVacuum) != 0 &&
        (canonical.buttons & CommandMelee) != 0;
  Game game;
  game.reset();
  std::array<PlayerSnapshot, MAX_PLAYERS> players{};
  players[0].active = true;
  players[0].id = 0;
  players[0].pos = game.state().player.pos;
  players[0].battery = 100;
  players[1].active = true;
  players[1].id = 1;
  players[1].pos = {2, 0.08f, 3};
  players[1].battery = 73;
  players[1].flags |= 1u << 3;
  players[1].bleedoutTimer = 9.5f;
  players[1].reviveCharge = 4.0f;
  players[1].grabbedByTarget = 2;
  auto world = captureWorld(game.state(), players, 123);
  auto snapshotBytes = encodeSnapshot(0, world, 8);
  WorldSnapshot roundtrip;
  ok &= decodeSnapshot(snapshotBytes.data(), snapshotBytes.size(), h,
                       roundtrip) &&
        h.type == MessageType::Snapshot && roundtrip.tick == 123 &&
        roundtrip.players[1].active &&
        std::abs(roundtrip.players[1].pos.x - 2) < 0.0001f &&
        (roundtrip.players[1].flags & (1u << 3)) != 0 &&
        std::abs(roundtrip.players[1].bleedoutTimer - 9.5f) < 0.0001f &&
        roundtrip.players[1].grabbedByTarget == 2 &&
        roundtrip.roomSeed == 12345;
  game.configureNetworkHost();
  game.setNetworkPeerActive(1, true);
  const Vec3 hostBefore = game.state().player.pos;
  const Vec3 peerBefore = game.state().multiplayer.peers[1].player.pos;
  game.setNetworkPeerInput(1, 1, 0, 1, 0, 0, Forward);
  for (int i = 0; i < 30; ++i)
    game.update(1.0f / 60.0f);
  const auto capturedPlayers = capturePlayers(game.state());
  ok &= capturedPlayers[0].active && capturedPlayers[1].active &&
        length(game.state().player.pos - hostBefore) < 0.001f &&
        game.state().multiplayer.peers[1].player.pos.z < peerBefore.z - 0.05f;
  Game combat;
  combat.reset();
  combat.configureNetworkHost();
  combat.setNetworkPeerActive(1, true);
  GameState &combatState = const_cast<GameState &>(combat.state());
  combatState.player.pos = {8, 0.08f, 8};
  combatState.multiplayer.peers[1].player.pos = {0, 0.08f, 0};
  combatState.multiplayer.peers[1].player.battery = 100;
  for (auto &target : combatState.targets)
    target.alive = false;
  TargetState &enemy = combatState.targets[0];
  enemy = TargetState{};
  enemy.alive = true;
  enemy.pos = {0, 0.08f, -0.7f};
  enemy.walkTarget = enemy.pos;
  const float hostBattery = combatState.player.battery;
  for (int i = 0; i < 55; ++i)
    combat.update(1.0f / 60.0f);
  ok &= combat.state().multiplayer.peers[1].player.battery < 95 &&
        std::abs(combat.state().player.battery - hostBattery) < 0.01f;
  Game hostProgression;
  hostProgression.setPersistentProgression(0, 4, 3, 2);
  hostProgression.reset();
  hostProgression.configureNetworkHost();
  GameState &hostProgressionState =
      const_cast<GameState &>(hostProgression.state());
  hostProgressionState.upgradeMenu.active = true;
  hostProgressionState.progression.run.temporaryLevels = {2, 3, 4};
  hostProgressionState.progression.run.roomHeat = 0.75f;
  auto progressionWorld = captureWorld(
      hostProgression.state(), capturePlayers(hostProgression.state()), 200);
  progressionWorld.tvSignal = 11;
  progressionWorld.tvDamage = 2;
  progressionWorld.tvTolerance = 4;
  progressionWorld.tvBroken = true;
  auto progressionBytes = encodeSnapshot(0, progressionWorld, 9);
  WorldSnapshot progressionRoundtrip;
  ok &= decodeSnapshot(progressionBytes.data(), progressionBytes.size(), h,
                       progressionRoundtrip) &&
        progressionRoundtrip.upgradeMenuActive &&
        progressionRoundtrip.temporaryUpgradeLevels[1] == 3 &&
        progressionRoundtrip.sharedPermanentUpgradeLevels[0] == 4 &&
        progressionRoundtrip.tvSignal == 11 &&
        progressionRoundtrip.tvDamage == 2 && progressionRoundtrip.tvBroken &&
        std::abs(progressionRoundtrip.roomHeat - 0.75f) < 0.0001f;
  Game guestProgression;
  guestProgression.reset();
  guestProgression.configureNetworkGuest(1);
  progressionWorld.captures[0] = true;
  guestProgression.networkMutableState().frame = 777;
  guestProgression.networkMutableState().time = 12.5f;
  guestProgression.networkMutableState().camera.pitch = 0.37f;
  guestProgression.networkMutableState().camera.firstPerson = true;
  applyWorld(guestProgression.networkMutableState(), progressionWorld, 1);
  ok &= guestProgression.state().progression.permanent.tokens == 0 &&
        guestProgression.state().upgradeMenu.active &&
        guestProgression.state().frame == 777 &&
        std::abs(guestProgression.state().time - 12.5f) < 0.0001f &&
        std::abs(guestProgression.state().camera.pitch - 0.37f) < 0.0001f &&
        guestProgression.state().camera.firstPerson &&
        guestProgression.state()
                .progression.run.networkSharedPermanentLevels[0] == 4;
  progressionWorld.captures[0] = false;
  applyWorld(guestProgression.networkMutableState(), progressionWorld, 1);
  progressionWorld.captures[0] = true;
  applyWorld(guestProgression.networkMutableState(), progressionWorld, 1);
  ok &= guestProgression.state().progression.permanent.tokens == 1 &&
        guestProgression.state().progression.permanent.revision > 0;
  if (!snapshotBytes.empty()) {
    snapshotBytes[16] ^= 1;
    WorldSnapshot invalid;
    ok &=
        !decodeSnapshot(snapshotBytes.data(), snapshotBytes.size(), h, invalid);
  }
  std::printf("MULTIPLAYER_PROTOCOL_%s input=%zu snapshot=%zu peerBattery=%.2f "
              "hostBattery=%.2f attackHit=%d owner=%d\n",
              ok ? "OK" : "FAILED", bytes.size(),
              encodeSnapshot(0, world, 8).size(),
              combat.state().multiplayer.peers[1].player.battery,
              combat.state().player.battery,
              combat.state().targets[0].attackHit ? 1 : 0,
              combat.state().enemyAttackOwner);
  return ok ? 0 : 1;
}
