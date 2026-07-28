#include "MultiplayerProtocol.hpp"
#include <cmath>
#include <cstdio>
#include <memory>

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
  const NetworkWorldContext inputWorld{17,2,4,3};
  auto bytes = encodeInput(2, inputWorld, input);
  PacketHeader h;
  NetworkWorldContext decodedWorld;
  InputCommand decoded;
  ok &= decodeInput(bytes.data(), bytes.size(), h, decodedWorld, decoded) &&
         h.playerId == 2 && decoded.sequence == 7 &&
         decoded.localTick == 99 &&
         decodedWorld == inputWorld &&
         decoded.buttons == (Vacuum | Sprint) &&
         std::abs(decoded.moveZ - 0.75f) < 0.0001f;
  ok &= compareWorldContext({17,2,3,2},inputWorld)==WorldContextCompatibility::Older &&
        compareWorldContext({17,2,5,4},inputWorld)==WorldContextCompatibility::NewerRoom &&
        compareWorldContext({18,2,4,3},inputWorld)==WorldContextCompatibility::Incompatible &&
        compareWorldContext(inputWorld,inputWorld)==WorldContextCompatibility::Compatible;
  GameplayEvent event;
  event.world=inputWorld;event.authoritativeTick=91;event.eventId=5;
  event.type=GameplayEventType::EnemyShellBroken;
  event.sourceEntityId=1;event.targetEntityId=3;
  event.position={1,2,3};event.direction={0,0,-1};event.flags=7;
  const auto eventBytes=encodeEvent(0,event);
  GameplayEvent decodedEvent;
  ok &= decodeEvent(eventBytes.data(),eventBytes.size(),h,decodedEvent) &&
        decodedEvent.world==inputWorld && decodedEvent.eventId==5 &&
        decodedEvent.authoritativeTick==91 &&
        decodedEvent.type==GameplayEventType::EnemyShellBroken &&
        decodedEvent.targetEntityId==3 && decodedEvent.flags==7;
  GameplayEventTracker eventTracker;eventTracker.reset(inputWorld);
  ok &= eventTracker.accept(decodedEvent) && !eventTracker.accept(decodedEvent);
  auto staleEvent=decodedEvent;staleEvent.world.roomGeneration=3;staleEvent.eventId=6;
  ok &= !eventTracker.accept(staleEvent);
  auto wrongSessionEvent=decodedEvent;
  wrongSessionEvent.world.sessionId++;
  wrongSessionEvent.eventId=6;
  auto wrongRunEvent=decodedEvent;
  wrongRunEvent.world.runGeneration++;
  wrongRunEvent.eventId=6;
  ok &= !eventTracker.accept(wrongSessionEvent) &&
        !eventTracker.accept(wrongRunEvent);
  GameplayEvent vacuumEvent=decodedEvent;
  vacuumEvent.eventId=6;
  vacuumEvent.type=GameplayEventType::SoulLatched;
  vacuumEvent.targetEntityId=1;
  const auto vacuumEventBytes=encodeEvent(0,vacuumEvent);
  GameplayEvent decodedVacuumEvent;
  ok &= decodeEvent(vacuumEventBytes.data(),vacuumEventBytes.size(),h,
                    decodedVacuumEvent) &&
        decodedVacuumEvent.type==GameplayEventType::SoulLatched &&
        decodedVacuumEvent.targetEntityId==1 &&
        eventTracker.accept(decodedVacuumEvent) &&
        !eventTracker.accept(decodedVacuumEvent);
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
  players[1].targetYaw = 0.75f;
  players[1].jumpVel = 4.5f;
  players[1].airJumpsRemaining = 0;
  players[1].storedSoulBruteMask = 1;
  players[1].actionFlags = 1 | 2 | 4 | 16 | 32 | 64;
  players[1].locomotion = NetLocomotionState::Airborne;
  players[1].action = NetActionState::AirLunge;
  players[1].actionPhase = NetActionPhase::Contact;
  players[1].actionSequence = 12;
  players[1].actionTargetId = 3;
  players[1].actionProgress = 0.65f;
  players[1].ledgeCollider = 3;
  players[1].ledgeNormal = {1,0,0};
  players[1].ledgeMantleTimer = 0.18f;
  players[1].vacuumFieldStrength = 0.8f;
  players[1].supplementalValue = 32.0f;
  players[1].supplementalMax = 85.0f;
  players[1].flowerStacks = 1;
  players[1].phoneRoll = 0.42f;
  players[1].doubleJumpFlip = 0.65f;
  players[1].phoneOrientation = quatAxisAngle({0,1,0},0.8f);
  players[1].phoneActionState = 6;
  players[1].meleeVariant = 3;
  players[1].airLungeRotation = 1.2f;
  players[1].flags |= 1u << 3;
  players[1].bleedoutTimer = 9.5f;
  players[1].reviveCharge = 4.0f;
  players[1].grabbedByTarget = 2;
  auto world = captureWorld(game.state(), players, 123);
  world.world=inputWorld;
  GameState &worldState = const_cast<GameState &>(game.state());
  worldState.targets[0].alive = true;
  worldState.targets[0].humanAnimationTime = 4.25f;
  worldState.targets[0].locomotionAmount = 1.0f;
  worldState.targets[0].attackTimer = 0.42f;
  worldState.targets[0].attackVariant = 3;
  worldState.targets[0].attackDirection = {0.6f, 0.0f, -0.8f};
  worldState.targets[0].hitFlash = 0.7f;
  worldState.targets[0].phase = 1.7f;
  worldState.targets[0].floatOffset = 2.1f;
  worldState.targets[0].vacuumPullAmount = 0.55f;
  worldState.targets[0].captureCollapseAmount = 0.35f;
  worldState.targets[0].visibility = 0.8f;
  worldState.targets[1].alive=true;
  worldState.targets[1].slurpable=true;
  worldState.targets[1].soulState=SoulState::Ingesting;
  worldState.targets[1].latchedToScreen=true;
  worldState.targets[1].ingestProgress=0.63f;
  worldState.targets[1].networkOwnerPlayerId=1;
  worldState.topology.currentTileIndex = -2;
  worldState.topology.previousTileIndex = -1;
  worldState.topology.advancing = true;
  worldState.doorTransition.active = true;
  worldState.doorTransition.progress = 0.6f;
  worldState.debug.colliderCount = 1;
  worldState.roomColliders[0].center = {2,0.5f,-3};
  worldState.roomColliders[0].width = 1.5f;
  worldState.captures[0].pos = {-1.25f,3.05f,-15};
  worldState.secretTv.available = true;
  worldState.secretTv.entrancePos = {13,0.5f,4};
  worldState.bullets[0].alive = true;
  worldState.bullets[0].brute = true;
  worldState.bullets[0].pos = {2,1,-4};
  worldState.bullets[0].vel = {0,0,-9};
  worldState.bullets[0].life = 1.2f;
  world = captureWorld(game.state(), players, 123);
  world.world=inputWorld;
  auto snapshotBytes = encodeSnapshot(0, world, 8);
  WorldSnapshot roundtrip;
  ok &= decodeSnapshot(snapshotBytes.data(), snapshotBytes.size(), h,
                       roundtrip) &&
        h.type == MessageType::Snapshot && roundtrip.tick == 123 &&
        roundtrip.world == inputWorld &&
        roundtrip.players[1].active &&
        std::abs(roundtrip.players[1].pos.x - 2) < 0.0001f &&
        (roundtrip.players[1].flags & (1u << 3)) != 0 &&
        std::abs(roundtrip.players[1].bleedoutTimer - 9.5f) < 0.0001f &&
        roundtrip.players[1].grabbedByTarget == 2 &&
        roundtrip.roomSeed == 12345 &&
        std::abs(roundtrip.players[1].jumpVel - 4.5f) < 0.0001f &&
        roundtrip.players[1].ledgeCollider == 3 &&
        std::abs(roundtrip.players[1].supplementalValue - 32.0f) < 0.0001f &&
        roundtrip.players[1].meleeVariant == 3 &&
        std::abs(roundtrip.players[1].phoneOrientation.w -
                 world.players[1].phoneOrientation.w) < 0.0001f &&
        roundtrip.players[1].phoneActionState == 6 &&
        roundtrip.players[1].locomotion == NetLocomotionState::Airborne &&
        roundtrip.players[1].action == NetActionState::AirLunge &&
        roundtrip.players[1].actionPhase == NetActionPhase::Contact &&
        roundtrip.players[1].actionSequence == 12 &&
        roundtrip.players[1].actionTargetId == 3 &&
        std::abs(roundtrip.players[1].actionProgress - 0.65f) < 0.0001f &&
        std::abs(roundtrip.targets[0].animationTime - 4.25f) < 0.0001f &&
        roundtrip.targets[0].attackVariant == 3 &&
        std::abs(roundtrip.targets[0].attackDirection.x - 0.6f) < 0.0001f &&
        std::abs(roundtrip.targets[0].vacuumPullAmount - 0.55f) < 0.0001f &&
        roundtrip.targets[1].soulState==SoulState::Ingesting &&
        (roundtrip.targets[1].visualFlags&2u)!=0 &&
        std::abs(roundtrip.targets[1].ingest-0.63f)<0.0001f &&
        roundtrip.targets[1].ownerPlayerId==1 &&
        roundtrip.topology.currentTileIndex == -2 &&
        roundtrip.doorTransition.active &&
        roundtrip.roomColliderCount == 1 &&
        std::abs(roundtrip.roomColliders[0].center.x - 2.0f) < 0.0001f &&
        std::abs(roundtrip.capturePositions[0].x + 1.25f) < 0.0001f &&
        roundtrip.bullets[0].active && roundtrip.bullets[0].brute &&
        std::abs(roundtrip.bullets[0].pos.z + 4.0f) < 0.0001f &&
        roundtrip.tvAvailable &&
        snapshotBytes.size() <= MAX_SNAPSHOT_BYTES;
  Game completeGuest;
  completeGuest.reset();
  completeGuest.configureNetworkGuest(1);
  applyWorld(completeGuest.networkMutableState(), roundtrip, 1);
  ok &= completeGuest.state().player.ledgeHanging &&
        completeGuest.state().player.storedSoulBrute[0] &&
        completeGuest.state().energy.supplementalActive &&
        completeGuest.state().multiplayer.peers[0].active &&
        completeGuest.state().debug.colliderCount == 1 &&
        completeGuest.state().topology.currentTileIndex == -2 &&
        completeGuest.state().doorTransition.active &&
        std::abs(completeGuest.state().captures[0].pos.x + 1.25f) < 0.0001f;
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
  const int pausedHostFrame=game.state().frame;
  game.setUiPaused(true);
  game.update(1.0f/60.0f);
  ok &= game.state().frame>pausedHostFrame;
  game.setUiPaused(false);
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
  Game guestCombat;
  guestCombat.reset();
  guestCombat.configureNetworkGuest(1);
  GameState& guestCombatState=guestCombat.networkMutableState();
  for(auto& target:guestCombatState.targets)target.alive=false;
  guestCombatState.targets[0]=TargetState{};
  guestCombatState.targets[0].alive=true;
  guestCombatState.targets[0].armor=2.0f;
  guestCombatState.targets[0].pos=guestCombatState.player.pos+Vec3{0,0,-0.5f};
  guestCombatState.meleeVisual.airLungeLandingPending=true;
  guestCombatState.meleeVisual.locomotionLunge=true;
  guestCombatState.meleeVisual.hitRadius=2.0f;
  guestCombat.update(1.0f/60.0f);
  ok &= std::abs(guestCombat.state().targets[0].armor-2.0f)<0.0001f;
  Game guestVacuum;
  guestVacuum.reset();
  guestVacuum.configureNetworkGuest(1);
  GameState& guestVacuumState=guestVacuum.networkMutableState();
  guestVacuumState.targets[1]=TargetState{};
  guestVacuumState.targets[1].alive=true;
  guestVacuumState.targets[1].slurpable=true;
  guestVacuumState.targets[1].captureQueued=true;
  guestVacuum.setTouchControls(0,0,0,0,true,false,false,false,false,false);
  guestVacuum.update(1.0f/60.0f);
  ok &= guestVacuum.state().vacuum.active &&
        guestVacuum.state().vacuum.pose>0.0f &&
        guestVacuum.state().vacuum.fieldStrength>0.0f &&
        guestVacuum.state().targets[1].captureQueued &&
        guestVacuum.state().targets[1].alive &&
        guestVacuum.state().player.souls==0;
  Game hostCapture;
  hostCapture.reset();
  hostCapture.configureNetworkHost();
  GameState& hostCaptureState=hostCapture.networkMutableState();
  hostCaptureState.targets[1]=TargetState{};
  hostCaptureState.targets[1].alive=true;
  hostCaptureState.targets[1].slurpable=true;
  hostCaptureState.targets[1].captureQueued=true;
  hostCaptureState.targets[1].ingestProgress=0.95f;
  hostCapture.update(1.0f/60.0f);
  ok &= !hostCapture.state().targets[1].alive &&
        hostCapture.state().player.souls==1;
  auto capturedWorld=captureWorld(hostCapture.state(),
                                  capturePlayers(hostCapture.state()),401);
  capturedWorld.world={17,2,5,4};
  applyWorld(guestVacuum.networkMutableState(),capturedWorld,1);
  ok &= !guestVacuum.state().targets[1].alive &&
        guestVacuum.state().targets[1].soulState==SoulState::Free;
  Game guestDischarge;
  guestDischarge.reset();
  guestDischarge.configureNetworkGuest(1);
  guestDischarge.networkMutableState().player.souls=1;
  guestDischarge.setTouchControls(0,0,0,0,false,false,false,false,true,false);
  guestDischarge.update(1.0f/60.0f);
  ok &= guestDischarge.state().player.souls==1 &&
        guestDischarge.state().energy.dischargeTimer>0.0f &&
        guestDischarge.state().energy.dischargePositionAmount>0.0f;
  Game hostDischarge;
  hostDischarge.reset();
  hostDischarge.configureNetworkHost();
  hostDischarge.networkMutableState().player.souls=1;
  hostDischarge.setTouchControls(0,0,0,0,false,false,false,false,true,false);
  for(int i=0;i<12;++i)hostDischarge.update(1.0f/60.0f);
  bool projectileSpawned=false;
  for(const auto& bullet:hostDischarge.state().bullets)
    projectileSpawned|=bullet.alive;
  ok &= hostDischarge.state().player.souls==0&&projectileSpawned;
  auto projectileWorld=captureWorld(hostDischarge.state(),
      capturePlayers(hostDischarge.state()),450);
  auto projectileBytes=encodeSnapshot(0,projectileWorld,12);
  WorldSnapshot projectileRoundtrip;
  ok &= decodeSnapshot(projectileBytes.data(),projectileBytes.size(),h,
                        projectileRoundtrip) &&
        projectileRoundtrip.bullets[0].active;
  projectileRoundtrip.world={17,2,6,5};
  for(auto& bullet:projectileRoundtrip.bullets)bullet=BulletSnapshot{};
  for(auto& target:projectileRoundtrip.targets){
    target.soulState=SoulState::Free;
    target.flags&=static_cast<std::uint8_t>(~(8u|16u));
  }
  projectileRoundtrip.players[1].actionFlags&=
      static_cast<std::uint8_t>(~2u);
  projectileRoundtrip.players[1].vacuumTarget=-1;
  applyWorld(guestDischarge.networkMutableState(),projectileRoundtrip,1);
  bool projectileRecovered=false;
  for(const auto& bullet:guestDischarge.state().bullets)
    projectileRecovered|=bullet.alive;
  ok &= !projectileRecovered&&!guestDischarge.state().vacuum.active &&
        guestDischarge.state().vacuum.target==-1;
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
  ok &= std::abs(guestProgression.state().targets[0].humanAnimationTime -
                 progressionWorld.targets[0].animationTime) < 0.0001f;
  Game predictionGuest;
  predictionGuest.reset();
  predictionGuest.configureNetworkGuest(1);
  auto predictionWorld=captureWorld(predictionGuest.state(),capturePlayers(predictionGuest.state()),299);
  predictionWorld.players[1].active=true;
  predictionWorld.players[1].id=1;
  predictionWorld.players[1].pos=predictionGuest.state().player.pos;
  applyWorld(predictionGuest.networkMutableState(),predictionWorld,1);
  predictionGuest.setTouchControls(0,0,0,0,false,false,true,false,false,false);
  predictionGuest.update(1.0f/60.0f);
  const float immediateJumpY=predictionGuest.state().player.pos.y;
  predictionWorld.tick=300;
  predictionWorld.players[1].pos=predictionGuest.state().player.pos+Vec3{0.1f,0,0};
  const Vec3 predictedBefore=predictionGuest.state().player.pos;
  applyWorld(predictionGuest.networkMutableState(),predictionWorld,1);
  ok &= immediateJumpY>0.08f &&
        length(predictionGuest.state().player.pos-predictedBefore)<0.001f &&
        length(predictionGuest.state().multiplayer.localPredictionCorrection)>0.09f;
  progressionWorld.captures[0] = false;
  applyWorld(guestProgression.networkMutableState(), progressionWorld, 1);
  progressionWorld.captures[0] = true;
  applyWorld(guestProgression.networkMutableState(), progressionWorld, 1);
  ok &= guestProgression.state().progression.permanent.tokens == 1 &&
        guestProgression.state().progression.permanent.revision > 0;
  if (!snapshotBytes.empty()) {
    const auto validSnapshot = snapshotBytes;
    auto unknownAction=validSnapshot;
    constexpr std::size_t firstPlayerActionOffset=20+14+153+15*48+50;
    unknownAction[firstPlayerActionOffset]=0xff;
    WorldSnapshot unknownActionWorld;
    ok &= !decodeSnapshot(unknownAction.data(),unknownAction.size(),h,unknownActionWorld);
    for(std::size_t cut=0;cut<validSnapshot.size();cut+=std::max<std::size_t>(1,validSnapshot.size()/13)){
      std::vector<std::uint8_t> truncated(validSnapshot.begin(),validSnapshot.begin()+cut);
      WorldSnapshot invalid;
      ok &= !decodeSnapshot(truncated.data(),truncated.size(),h,invalid);
    }
    snapshotBytes[16] ^= 1;
    WorldSnapshot invalid;
    ok &=
        !decodeSnapshot(snapshotBytes.data(), snapshotBytes.size(), h, invalid);
    std::vector<std::uint8_t> oversized(MAX_PACKET_BYTES+1,0);
    ok &= !decodeSnapshot(oversized.data(),oversized.size(),h,invalid);
  }
  if(!vacuumEventBytes.empty()){
    auto unknownEvent=vacuumEventBytes;
    constexpr std::size_t eventTypeOffset=HEADER_BYTES+14;
    unknownEvent[eventTypeOffset]=0xff;
    GameplayEvent invalidEvent;
    ok &= !decodeEvent(unknownEvent.data(),unknownEvent.size(),h,invalidEvent);
    for(std::size_t cut=0;cut<vacuumEventBytes.size();++cut){
      std::vector<std::uint8_t> truncated(vacuumEventBytes.begin(),
                                         vacuumEventBytes.begin()+cut);
      ok &= !decodeEvent(truncated.data(),truncated.size(),h,invalidEvent);
    }
  }
  const auto decodedHash=authoritativeStateHash(roundtrip);
  ok &= decodedHash==authoritativeStateHash(roundtrip) &&
        decodedHash==authoritativeStateHash(world) &&
        visualStateHash(roundtrip)==visualStateHash(world);
  auto changedWorld=std::make_unique<WorldSnapshot>(roundtrip);
  changedWorld->depositedSouls++;
  ok &= authoritativeStateHash(*changedWorld)!=decodedHash;
  auto localOnlyHashState=std::make_unique<GameState>(completeGuest.state());
  const auto localOnlyBase=authoritativeStateHash(
      captureWorld(*localOnlyHashState,roundtrip.players,roundtrip.tick));
  localOnlyHashState->camera.pitch+=0.5f;
  localOnlyHashState->uiPaused=!localOnlyHashState->uiPaused;
  localOnlyHashState->localSettings.musicVolume=0.17f;
  ok &= authoritativeStateHash(captureWorld(*localOnlyHashState,roundtrip.players,
                                             roundtrip.tick))==localOnlyBase;

  auto interpolationA=std::make_unique<WorldSnapshot>(roundtrip);
  auto interpolationB=std::make_unique<WorldSnapshot>(roundtrip);
  interpolationA->tick=300;interpolationB->tick=303;
  interpolationA->players[0].active=interpolationB->players[0].active=true;
  interpolationA->players[0].pos={0,1,0};
  interpolationB->players[0].pos={2,1,0};
  interpolationA->players[0].yaw=3.10f;
  interpolationB->players[0].yaw=-3.10f;
  interpolationA->targets[0].flags=interpolationB->targets[0].flags=1;
  interpolationA->targets[0].pos={0,0,-2};
  interpolationB->targets[0].pos={2,0,-2};
  interpolationA->bullets[0].active=interpolationB->bullets[0].active=true;
  interpolationA->bullets[0].pos={0,1,-1};
  interpolationB->bullets[0].pos={4,1,-1};
  auto interpolator=std::make_unique<SnapshotInterpolator>();
  interpolator->push(*interpolationA,1000);
  interpolator->push(*interpolationB,1100);
  auto interpolationGame=std::make_unique<Game>();
  interpolationGame->reset();
  interpolationGame->configureNetworkGuest(1);
  applyWorld(interpolationGame->networkMutableState(),*interpolationB,1);
  auto visualState=std::make_unique<GameState>(interpolationGame->state());
  interpolator->apply(*visualState,1,1100);
  ok &= std::abs(visualState->multiplayer.peers[0].player.pos.x-1.0f)<0.001f &&
        std::abs(std::abs(visualState->multiplayer.peers[0].player.yaw)-DB_PI)<0.05f &&
        std::abs(visualState->targets[0].pos.x-1.0f)<0.001f &&
        std::abs(visualState->bullets[0].pos.x-2.0f)<0.001f;

  interpolationA->tick=306;interpolationA->players[0].pos={30,1,0};
  interpolator->push(*interpolationA,1200);
  *visualState=interpolationGame->state();
  interpolator->apply(*visualState,1,1200);
  ok &= std::abs(visualState->multiplayer.peers[0].player.pos.x-30.0f)<0.001f;

  interpolationA->tick=309;interpolationA->roomIndex++;
  interpolationA->players[0].pos={3,1,0};
  interpolator->push(*interpolationA,1300);
  *visualState=interpolationGame->state();
  interpolator->apply(*visualState,1,1300);
  ok &= std::abs(visualState->multiplayer.peers[0].player.pos.x-3.0f)<0.001f;

  *interpolationB=*interpolationA;
  interpolationA->tick=312;interpolationB->tick=315;
  interpolationA->players[0].flags&=static_cast<std::uint8_t>(~(1u<<2));
  interpolationB->players[0].flags|=1u<<2;
  interpolationA->players[0].pos={1,1,0};interpolationB->players[0].pos={2,1,0};
  interpolator->push(*interpolationA,1400);interpolator->push(*interpolationB,1500);
  *visualState=interpolationGame->state();
  interpolator->apply(*visualState,1,1500);
  ok &= std::abs(visualState->multiplayer.peers[0].player.pos.x-2.0f)<0.001f;
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
