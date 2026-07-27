#include "MultiplayerProtocol.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace dbnet {
namespace {

class Writer {
public:
    std::vector<std::uint8_t> data;
    void u8(std::uint8_t v){data.push_back(v);} void i8(std::int8_t v){u8(static_cast<std::uint8_t>(v));}
    void u16(std::uint16_t v){u8(static_cast<std::uint8_t>(v));u8(static_cast<std::uint8_t>(v>>8));}
    void u32(std::uint32_t v){for(int i=0;i<4;++i)u8(static_cast<std::uint8_t>(v>>(i*8)));}
    void i32(std::int32_t v){u32(static_cast<std::uint32_t>(v));}
    void f32(float v){std::uint32_t bits=0;static_assert(sizeof(bits)==sizeof(v));std::memcpy(&bits,&v,sizeof(v));u32(bits);}
    void vec(const Vec3& v){f32(v.x);f32(v.y);f32(v.z);}
};

class Reader {
public:
    Reader(const std::uint8_t* bytes,std::size_t length):data(bytes),size(length){}
    bool u8(std::uint8_t& v){if(at+1>size)return false;v=data[at++];return true;}
    bool i8(std::int8_t& v){std::uint8_t x=0;if(!u8(x))return false;v=static_cast<std::int8_t>(x);return true;}
    bool u16(std::uint16_t& v){std::uint8_t a=0,b=0;if(!u8(a)||!u8(b))return false;v=static_cast<std::uint16_t>(a|(b<<8));return true;}
    bool u32(std::uint32_t& v){v=0;for(int i=0;i<4;++i){std::uint8_t x=0;if(!u8(x))return false;v|=static_cast<std::uint32_t>(x)<<(i*8);}return true;}
    bool i32(std::int32_t& v){std::uint32_t x=0;if(!u32(x))return false;v=static_cast<std::int32_t>(x);return true;}
    bool f32(float& v){std::uint32_t bits=0;if(!u32(bits))return false;std::memcpy(&v,&bits,sizeof(v));return true;}
    bool vec(Vec3& v){return f32(v.x)&&f32(v.y)&&f32(v.z);} bool done()const{return at==size;}
private: const std::uint8_t* data;std::size_t size=0,at=0;
};

void header(Writer& w,MessageType type,std::uint8_t playerId,std::uint32_t seq,std::uint32_t tick,std::uint32_t payload){w.u32(MAGIC);w.u16(PROTOCOL_VERSION);w.u8(static_cast<std::uint8_t>(type));w.u8(playerId);w.u32(seq);w.u32(tick);w.u32(payload);}

void writePlayer(Writer& w,const PlayerSnapshot& p){
  w.u8(p.active?1:0);w.u8(p.id);w.vec(p.pos);w.vec(p.vel);
  w.f32(p.yaw);w.f32(p.targetYaw);w.f32(p.pitch);w.f32(p.jumpVel);
  w.f32(p.battery);w.u8(p.souls);w.u8(p.flags);w.u8(p.actionFlags);
  w.u8(p.storedSoulBruteMask);w.i8(p.airJumpsRemaining);w.i8(p.ledgeCollider);
  w.vec(p.ledgeNormal);w.f32(p.ledgeHangTime);w.f32(p.ledgeMantleTimer);
  w.f32(p.vacuumPower);w.f32(p.vacuumPose);w.f32(p.vacuumFieldStrength);
  w.f32(p.vacuumConeTightness);w.f32(p.vacuumLockStrength);w.i8(p.vacuumTarget);
  w.f32(p.meleeTimer);w.f32(p.dischargeAmount);w.f32(p.dischargeTimer);
  w.f32(p.supplementalValue);w.f32(p.supplementalMax);w.u8(p.flowerStacks);
  w.f32(p.phonePitch);w.f32(p.phoneRoll);w.f32(p.phoneYaw);w.f32(p.phoneLift);
  w.f32(p.phoneForward);w.f32(p.phoneSide);w.f32(p.doubleJumpTimer);
  w.f32(p.doubleJumpFlipYaw);w.f32(p.doubleJumpFlip);w.u8(p.phoneActionState);
  w.u8(p.meleeVariant);w.u8(p.meleeComboIndex);w.vec(p.meleeDirection);
  w.f32(p.airLungeRotation);w.f32(p.landingRecovery);
  w.f32(p.bleedoutTimer);w.f32(p.reviveCharge);w.f32(p.grabEscape);
  w.i8(p.grabbedByTarget);w.i32(p.secretVisitRoom);w.f32(p.secretVisitTimer);
  w.u8(p.commSignal);w.f32(p.commSignalTimer);
}
bool readPlayer(Reader& r,PlayerSnapshot& p){
  std::uint8_t active=0;
  return r.u8(active)&&((p.active=active!=0),true)&&r.u8(p.id)&&r.vec(p.pos)&&r.vec(p.vel)&&
    r.f32(p.yaw)&&r.f32(p.targetYaw)&&r.f32(p.pitch)&&r.f32(p.jumpVel)&&
    r.f32(p.battery)&&r.u8(p.souls)&&r.u8(p.flags)&&r.u8(p.actionFlags)&&
    r.u8(p.storedSoulBruteMask)&&r.i8(p.airJumpsRemaining)&&r.i8(p.ledgeCollider)&&
    r.vec(p.ledgeNormal)&&r.f32(p.ledgeHangTime)&&r.f32(p.ledgeMantleTimer)&&
    r.f32(p.vacuumPower)&&r.f32(p.vacuumPose)&&r.f32(p.vacuumFieldStrength)&&
    r.f32(p.vacuumConeTightness)&&r.f32(p.vacuumLockStrength)&&r.i8(p.vacuumTarget)&&
    r.f32(p.meleeTimer)&&r.f32(p.dischargeAmount)&&r.f32(p.dischargeTimer)&&
    r.f32(p.supplementalValue)&&r.f32(p.supplementalMax)&&r.u8(p.flowerStacks)&&
    r.f32(p.phonePitch)&&r.f32(p.phoneRoll)&&r.f32(p.phoneYaw)&&r.f32(p.phoneLift)&&
    r.f32(p.phoneForward)&&r.f32(p.phoneSide)&&r.f32(p.doubleJumpTimer)&&
    r.f32(p.doubleJumpFlipYaw)&&r.f32(p.doubleJumpFlip)&&r.u8(p.phoneActionState)&&
    r.u8(p.meleeVariant)&&r.u8(p.meleeComboIndex)&&r.vec(p.meleeDirection)&&
    r.f32(p.airLungeRotation)&&r.f32(p.landingRecovery)&&
    r.f32(p.bleedoutTimer)&&r.f32(p.reviveCharge)&&r.f32(p.grabEscape)&&
    r.i8(p.grabbedByTarget)&&r.i32(p.secretVisitRoom)&&r.f32(p.secretVisitTimer)&&
    r.u8(p.commSignal)&&r.f32(p.commSignalTimer);
}
void writeTarget(Writer& w,const TargetSnapshot& t){w.u8(t.flags);w.u8(static_cast<std::uint8_t>(t.soulState));w.vec(t.pos);w.vec(t.vel);w.f32(t.armor);w.f32(t.health);w.f32(t.capture);w.f32(t.ingest);w.f32(t.recoil);w.f32(t.scale);w.f32(t.visualYaw);w.f32(t.soulMorph);w.f32(t.attackTimer);w.f32(t.attackCooldown);w.f32(t.animationTime);w.f32(t.visualWalkPhase);w.f32(t.locomotionAmount);w.vec(t.attackDirection);w.u8(t.attackVariant);w.u8(t.visualFlags);w.f32(t.hitFlash);w.f32(t.phase);w.f32(t.floatOffset);w.f32(t.spinSpeed);w.f32(t.hitDirectionLocal);w.f32(t.vacuumPullAmount);w.f32(t.captureCollapseAmount);w.f32(t.visibility);w.f32(t.armorRegenDelay);w.f32(t.respawnTimer);w.i8(t.ownerPlayerId);w.i8(t.grabbedPlayerId);w.f32(t.grabCooldown);}
bool readTarget(Reader& r,TargetSnapshot& t){std::uint8_t soul=0;if(!r.u8(t.flags)||!r.u8(soul)||soul>static_cast<std::uint8_t>(SoulState::Revolving))return false;t.soulState=static_cast<SoulState>(soul);return r.vec(t.pos)&&r.vec(t.vel)&&r.f32(t.armor)&&r.f32(t.health)&&r.f32(t.capture)&&r.f32(t.ingest)&&r.f32(t.recoil)&&r.f32(t.scale)&&r.f32(t.visualYaw)&&r.f32(t.soulMorph)&&r.f32(t.attackTimer)&&r.f32(t.attackCooldown)&&r.f32(t.animationTime)&&r.f32(t.visualWalkPhase)&&r.f32(t.locomotionAmount)&&r.vec(t.attackDirection)&&r.u8(t.attackVariant)&&r.u8(t.visualFlags)&&r.f32(t.hitFlash)&&r.f32(t.phase)&&r.f32(t.floatOffset)&&r.f32(t.spinSpeed)&&r.f32(t.hitDirectionLocal)&&r.f32(t.vacuumPullAmount)&&r.f32(t.captureCollapseAmount)&&r.f32(t.visibility)&&r.f32(t.armorRegenDelay)&&r.f32(t.respawnTimer)&&r.i8(t.ownerPlayerId)&&r.i8(t.grabbedPlayerId)&&r.f32(t.grabCooldown);}
void writeCollider(Writer& w,const RoomCollider& c){w.f32(c.minX);w.f32(c.maxX);w.f32(c.minZ);w.f32(c.maxZ);w.f32(c.bottomY);w.f32(c.topY);w.f32(c.width);w.f32(c.depth);w.f32(c.height);w.vec(c.center);}
bool readCollider(Reader& r,RoomCollider& c){return r.f32(c.minX)&&r.f32(c.maxX)&&r.f32(c.minZ)&&r.f32(c.maxZ)&&r.f32(c.bottomY)&&r.f32(c.topY)&&r.f32(c.width)&&r.f32(c.depth)&&r.f32(c.height)&&r.vec(c.center);}
void writeBullet(Writer& w,const BulletSnapshot& b){w.u8(b.active?1:0);w.u8(b.brute?1:0);w.vec(b.pos);w.vec(b.vel);w.f32(b.life);w.f32(b.spin);}
bool readBullet(Reader& r,BulletSnapshot& b){std::uint8_t active=0,brute=0;return r.u8(active)&&r.u8(brute)&&((b.active=active!=0),(b.brute=brute!=0),true)&&r.vec(b.pos)&&r.vec(b.vel)&&r.f32(b.life)&&r.f32(b.spin);}
void writeFlower(Writer& w,const FlowerSnapshot& f){w.u8(f.active?1:0);w.vec(f.pos);w.f32(f.age);w.f32(f.rotation);}
bool readFlower(Reader& r,FlowerSnapshot& f){std::uint8_t active=0;return r.u8(active)&&((f.active=active!=0),true)&&r.vec(f.pos)&&r.f32(f.age)&&r.f32(f.rotation);}

}

bool decodeHeader(const std::uint8_t* data,std::size_t size,PacketHeader& out){if(!data||size<HEADER_BYTES||size>MAX_PACKET_BYTES)return false;Reader r(data,size);std::uint32_t magic=0;std::uint16_t version=0;std::uint8_t type=0;if(!r.u32(magic)||!r.u16(version)||!r.u8(type)||!r.u8(out.playerId)||!r.u32(out.sequence)||!r.u32(out.tick)||!r.u32(out.payloadBytes))return false;if(magic!=MAGIC||version!=PROTOCOL_VERSION||type<1||type>5||out.playerId>=MAX_PLAYERS||out.payloadBytes!=size-HEADER_BYTES)return false;out.type=static_cast<MessageType>(type);return true;}

std::vector<std::uint8_t> encodeInput(std::uint8_t playerId,const InputCommand& input){Writer payload;payload.f32(input.moveX);payload.f32(input.moveZ);payload.f32(input.yaw);payload.f32(input.pitch);payload.u16(input.buttons);Writer packet;header(packet,MessageType::Input,playerId,input.sequence,input.localTick,static_cast<std::uint32_t>(payload.data.size()));packet.data.insert(packet.data.end(),payload.data.begin(),payload.data.end());return packet.data;}
bool decodeInput(const std::uint8_t* data,std::size_t size,PacketHeader& h,InputCommand& input){if(!decodeHeader(data,size,h)||h.type!=MessageType::Input)return false;Reader r(data+HEADER_BYTES,h.payloadBytes);input.sequence=h.sequence;input.localTick=h.tick;return r.f32(input.moveX)&&r.f32(input.moveZ)&&r.f32(input.yaw)&&r.f32(input.pitch)&&r.u16(input.buttons)&&r.done();}

std::vector<std::uint8_t> encodeSnapshot(std::uint8_t playerId,
                                         const WorldSnapshot &s,
                                         std::uint32_t sequence) {
  Writer p;
  p.f32(s.time);
  p.i32(s.roomIndex);
  p.i32(s.roomSeed);
  p.i32(s.requiredSouls);
  p.i32(s.depositedSouls);
  p.u8(s.roomClear ? 1 : 0);
  p.u8(s.started ? 1 : 0);
  p.u8(s.dead ? 1 : 0);
  p.i32(s.runRules.requiredSlotStacks);
  p.i32(s.runRules.crowdedRoomStacks);
  p.i32(s.runRules.fasterSlurpStacks);
  p.i32(s.runRules.nextId);
  p.i32(s.runRules.lastAdded);
  p.u8(s.upgradeMenuActive ? 1 : 0);
  for (auto level : s.temporaryUpgradeLevels)
    p.i32(level);
  for (auto level : s.sharedPermanentUpgradeLevels)
    p.i32(level);
  p.f32(s.roomHeat);
  p.i32(s.tvSignal);p.i32(s.tvDamage);p.i32(s.tvTolerance);p.u8(s.tvBroken?1:0);
  p.u8(s.tvAvailable?1:0);p.vec(s.tvEntrancePos);p.vec(s.tvEntranceNormal);
  p.i32(s.topology.currentTileIndex);p.i32(s.topology.previousTileIndex);p.u8(s.topology.advancing?1:0);
  p.u8(s.doorTransition.active?1:0);p.f32(s.doorTransition.progress);
  p.f32(s.doorTransition.distanceTravelled);p.vec(s.doorTransition.lastPlayerPos);
  p.vec(s.doorTransition.frameMotion);p.u8(s.roomColliderCount);
  for(const auto& collider:s.roomColliders)writeCollider(p,collider);
  for (const auto &player : s.players)
    writePlayer(p, player);
  for (const auto &target : s.targets)
    writeTarget(p, target);
  for (std::size_t i=0;i<s.captures.size();++i){p.u8(s.captures[i]?1:0);p.vec(s.capturePositions[i]);}
  for (const auto &bullet : s.bullets)
    writeBullet(p, bullet);
  for (const auto &flower : s.flowers)
    writeFlower(p, flower);
  if(p.data.size()+HEADER_BYTES>MAX_SNAPSHOT_BYTES){
    std::printf("MULTIPLAYER_SNAPSHOT_REJECT reason=size bytes=%zu budget=%zu\n",p.data.size()+HEADER_BYTES,MAX_SNAPSHOT_BYTES);
    std::fflush(stdout);
    return {};
  }
  Writer packet;
  header(packet, MessageType::Snapshot, playerId, sequence, s.tick,
         static_cast<std::uint32_t>(p.data.size()));
  packet.data.insert(packet.data.end(), p.data.begin(), p.data.end());
  return packet.data;
}
bool decodeSnapshot(const std::uint8_t *data, std::size_t size, PacketHeader &h,
                    WorldSnapshot &s) {
  if (!decodeHeader(data, size, h) || h.type != MessageType::Snapshot)
    return false;
  Reader r(data + HEADER_BYTES, h.payloadBytes);
  s.tick = h.tick;
  std::uint8_t clear = 0, started = 0, dead = 0, upgradeMenu = 0;
  if (!r.f32(s.time) || !r.i32(s.roomIndex) || !r.i32(s.roomSeed) ||
      !r.i32(s.requiredSouls) || !r.i32(s.depositedSouls) || !r.u8(clear) ||
      !r.u8(started) || !r.u8(dead))
    return false;
  s.roomClear = clear != 0;
  s.started = started != 0;
  s.dead = dead != 0;
  if (!r.i32(s.runRules.requiredSlotStacks) ||
      !r.i32(s.runRules.crowdedRoomStacks) ||
      !r.i32(s.runRules.fasterSlurpStacks) || !r.i32(s.runRules.nextId) ||
      !r.i32(s.runRules.lastAdded) || !r.u8(upgradeMenu))
    return false;
  s.upgradeMenuActive = upgradeMenu != 0;
  for (auto &level : s.temporaryUpgradeLevels)
    if (!r.i32(level))
      return false;
  for (auto &level : s.sharedPermanentUpgradeLevels)
    if (!r.i32(level))
      return false;
  std::uint8_t tvBroken=0,tvAvailable=0,topologyAdvancing=0,doorActive=0;
  if (!r.f32(s.roomHeat)||!r.i32(s.tvSignal)||!r.i32(s.tvDamage)||!r.i32(s.tvTolerance)||!r.u8(tvBroken))
    return false;
  s.tvBroken=tvBroken!=0;
  if(!r.u8(tvAvailable)||!r.vec(s.tvEntrancePos)||!r.vec(s.tvEntranceNormal)||
     !r.i32(s.topology.currentTileIndex)||!r.i32(s.topology.previousTileIndex)||!r.u8(topologyAdvancing)||
     !r.u8(doorActive)||!r.f32(s.doorTransition.progress)||!r.f32(s.doorTransition.distanceTravelled)||
     !r.vec(s.doorTransition.lastPlayerPos)||!r.vec(s.doorTransition.frameMotion)||!r.u8(s.roomColliderCount)||
     s.roomColliderCount>ROOM_COLLIDER_COUNT)return false;
  s.tvAvailable=tvAvailable!=0;s.topology.advancing=topologyAdvancing!=0;s.doorTransition.active=doorActive!=0;
  for(auto& collider:s.roomColliders)if(!readCollider(r,collider))return false;
  for (auto &player : s.players)
    if (!readPlayer(r, player))
      return false;
  for (auto &target : s.targets)
    if (!readTarget(r, target))
      return false;
  for (std::size_t i = 0; i < s.captures.size(); ++i) {
    std::uint8_t value = 0;
    if (!r.u8(value)||!r.vec(s.capturePositions[i]))
      return false;
    s.captures[i] = value != 0;
  }
  for (auto &bullet : s.bullets)
    if (!readBullet(r, bullet))
      return false;
  for (auto &flower : s.flowers)
    if (!readFlower(r, flower))
      return false;
  return r.done();
}

std::array<PlayerSnapshot, MAX_PLAYERS> capturePlayers(const GameState &state) {
  std::array<PlayerSnapshot, MAX_PLAYERS> players{};
  auto fill = [](PlayerSnapshot &out, int id, const PlayerState &player,
                 const CameraState &camera, const VacuumState &vacuum,
                 const MeleeVisualState &melee, const EnergyState &energy,
                 const PhonePoseState &phonePose) {
    out.active = true;
    out.id = static_cast<std::uint8_t>(id);
    out.pos = player.pos;
    out.vel = player.vel;
    out.yaw = player.yaw;
    out.targetYaw = player.targetYaw;
    out.pitch = camera.pitch;
    out.jumpVel = player.jumpVel;
    out.battery = player.battery;
    out.souls = static_cast<std::uint8_t>(
        std::max(0, std::min(PHONE_CAPACITY, player.souls)));
    out.flags = (player.grounded ? 1 : 0) | (camera.firstPerson ? 2 : 0) |
                (player.alive ? 4 : 0) | (player.downed ? 8 : 0) | (player.inSecretRoom ? 16 : 0);
    out.actionFlags=(player.ledgeHanging?1:0)|(vacuum.active?2:0)|(energy.supplementalActive?4:0)|
      (melee.airLungePending?8:0)|(melee.airLungeLandingPending?16:0)|
      (melee.locomotionLunge?32:0)|(melee.visualHit?64:0);
    for(int i=0;i<PHONE_CAPACITY;++i)if(player.storedSoulBrute[i])out.storedSoulBruteMask|=static_cast<std::uint8_t>(1u<<i);
    out.airJumpsRemaining=static_cast<std::int8_t>(player.airJumpsRemaining);
    out.ledgeCollider=static_cast<std::int8_t>(player.ledgeCollider);
    out.ledgeNormal=player.ledgeNormal;out.ledgeHangTime=player.ledgeHangTime;out.ledgeMantleTimer=player.ledgeMantleTimer;
    out.vacuumPower = vacuum.power;
    out.vacuumPose = vacuum.pose;
    out.vacuumFieldStrength=vacuum.fieldStrength;out.vacuumConeTightness=vacuum.coneTightness;out.vacuumLockStrength=vacuum.lockStrength;
    out.vacuumTarget = static_cast<std::int8_t>(vacuum.target);
    out.meleeTimer = melee.visualTimer;
    out.dischargeAmount = energy.dischargePositionAmount;
    out.dischargeTimer=energy.dischargeTimer;out.supplementalValue=energy.supplementalValue;
    out.supplementalMax=energy.supplementalMax;out.flowerStacks=static_cast<std::uint8_t>(std::max(0,std::min(255,energy.flowerStacks)));
    out.phonePitch=phonePose.pitch;out.phoneRoll=phonePose.roll;out.phoneYaw=phonePose.yaw;
    out.phoneLift=phonePose.lift;out.phoneForward=phonePose.forward;out.phoneSide=phonePose.side;
    out.doubleJumpTimer=phonePose.doubleJumpTimer;out.doubleJumpFlipYaw=phonePose.doubleJumpFlipYaw;
    out.doubleJumpFlip=phonePose.doubleJumpFlip;out.phoneActionState=static_cast<std::uint8_t>(phonePose.actionState);
    out.meleeVariant=static_cast<std::uint8_t>(melee.variant);out.meleeComboIndex=static_cast<std::uint8_t>(melee.comboIndex);
    out.meleeDirection=melee.direction;out.airLungeRotation=melee.airLungeRotation;out.landingRecovery=melee.landingRecovery;
    out.bleedoutTimer=player.bleedoutTimer;out.reviveCharge=player.reviveCharge;out.grabEscape=player.grabEscape;out.grabbedByTarget=static_cast<std::int8_t>(player.grabbedByTarget);
    out.secretVisitRoom=player.secretVisitRoom;out.secretVisitTimer=player.secretVisitTimer;
    out.commSignal=static_cast<std::uint8_t>(std::max(0,std::min(4,player.commSignal)));
    out.commSignalTimer=std::max(0.0f,player.commSignalTimer);
  };
  const int local =
      state.multiplayer.enabled ? state.multiplayer.localPlayerId : 0;
  fill(players[local], local, state.player, state.camera, state.vacuum,
       state.meleeVisual, state.energy, state.phonePose);
  for (const auto &peer : state.multiplayer.peers)
    if (peer.active && peer.playerId >= 0 && peer.playerId < MAX_PLAYERS &&
        peer.playerId != local)
      fill(players[peer.playerId], peer.playerId, peer.player, peer.camera,
           peer.vacuum, peer.meleeVisual, peer.energy, peer.phonePose);
  return players;
}

WorldSnapshot
captureWorld(const GameState &state,
             const std::array<PlayerSnapshot, MAX_PLAYERS> &players,
             std::uint32_t tick) {
  WorldSnapshot s;
  s.tick = tick;
  s.time = state.time;
  s.roomIndex = state.roomIndex;
  s.roomSeed = state.roomSeed;
  s.requiredSouls = state.requiredSouls;
  s.depositedSouls = state.depositedSouls;
  s.roomClear = state.roomClear;
  s.started = state.started;
  s.dead = state.dead;
  s.runRules = state.runRules;
  s.upgradeMenuActive = state.upgradeMenu.active;
  for (int i = 0; i < 3; ++i)
    s.temporaryUpgradeLevels[i] = state.progression.run.temporaryLevels[i];
  for (int i = 0; i < 3; ++i)
    s.sharedPermanentUpgradeLevels[i] = state.progression.permanent.levels[i];
  s.roomHeat = state.progression.run.roomHeat;
  s.tvSignal=state.secretTv.signal;s.tvDamage=state.secretTv.damage;s.tvTolerance=state.secretTv.tolerance;s.tvBroken=state.secretTv.broken;
  s.tvAvailable=state.secretTv.available;s.tvEntrancePos=state.secretTv.entrancePos;s.tvEntranceNormal=state.secretTv.entranceNormal;
  s.topology=state.topology;s.doorTransition=state.doorTransition;
  s.roomColliderCount=static_cast<std::uint8_t>(std::max(0,std::min(ROOM_COLLIDER_COUNT,state.debug.colliderCount)));
  s.roomColliders=state.roomColliders;
  s.players = players;
  for (int i = 0; i < TARGET_COUNT; ++i) {
    const auto &a = state.targets[i];
    auto &b = s.targets[i];
    b.flags = (a.alive ? 1 : 0) | (a.slurpable ? 2 : 0) | (a.brute ? 4 : 0) |
              (a.captureQueued ? 8 : 0) | (a.captureCommitted ? 16 : 0);
    b.soulState = a.soulState;
    b.pos = a.pos;
    b.vel = a.vel;
    b.armor = a.armor;
    b.health = a.health;
    b.capture = a.capture;
    b.ingest = a.ingestProgress;
    b.recoil = a.recoilTime;
    b.scale = a.scale;
    b.visualYaw = a.visualYaw;
    b.soulMorph = a.soulMorph;
    b.attackTimer = a.attackTimer;
    b.attackCooldown = a.attackCooldown;
    b.animationTime = a.humanAnimationTime;
    b.visualWalkPhase = a.visualWalkPhase;
    b.locomotionAmount = a.locomotionAmount;
    b.attackDirection = a.attackDirection;
    b.attackVariant = static_cast<std::uint8_t>(a.attackVariant);
    b.visualFlags = (a.attackHit ? 1 : 0) | (a.latchedToScreen ? 2 : 0);
    b.hitFlash = a.hitFlash;
    b.phase=a.phase;b.floatOffset=a.floatOffset;b.spinSpeed=a.spinSpeed;
    b.hitDirectionLocal=a.hitDirectionLocal;b.vacuumPullAmount=a.vacuumPullAmount;
    b.captureCollapseAmount=a.captureCollapseAmount;b.visibility=a.visibility;
    b.armorRegenDelay=a.armorRegenDelay;b.respawnTimer=a.respawnTimer;
    b.ownerPlayerId = static_cast<std::int8_t>(a.networkOwnerPlayerId);
    b.grabbedPlayerId=static_cast<std::int8_t>(a.grabbedPlayerId);b.grabCooldown=a.grabCooldown;
  }
  for (int i = 0; i < CAPTURE_COUNT; ++i){
    s.captures[i] = state.captures[i].filled;
    s.capturePositions[i]=state.captures[i].pos;
  }
  for (int i = 0; i < BULLET_COUNT; ++i) {
    const auto &a = state.bullets[i];
    auto &b = s.bullets[i];
    b.active = a.alive;
    b.brute = a.brute;
    b.pos = a.pos;
    b.vel = a.vel;
    b.life = a.life;
    b.spin = a.spin;
  }
  for (int i = 0; i < FLOWER_POWERUP_COUNT; ++i) {
    const auto &a = state.flowers[i];
    auto &b = s.flowers[i];
    b.active = a.active;
    b.pos = a.pos;
    b.age = a.age;
    b.rotation = a.rotationY;
  }
  return s;
}

void applyWorld(GameState &state, const WorldSnapshot &s,
                std::uint8_t localPlayerId) {
  // Authoritative and local clocks are deliberately separate. The snapshot
  // tick is ordered by DesktopMultiplayer and must never replace simulation
  // or presentation time on the receiving client.
  const bool roomChanged=state.multiplayer.hasWorldSnapshot&&state.roomIndex!=s.roomIndex;
  state.roomIndex = s.roomIndex;
  state.roomSeed = s.roomSeed;
  state.requiredSouls = s.requiredSouls;
  state.depositedSouls = s.depositedSouls;
  state.roomClear = s.roomClear;
  state.started = s.started;
  state.dead = s.dead;
  state.runRules = s.runRules;
  state.upgradeMenu.active = s.upgradeMenuActive;
  for (int i = 0; i < 3; ++i)
    state.progression.run.temporaryLevels[i] = s.temporaryUpgradeLevels[i];
  for (int i = 0; i < 3; ++i)
    state.progression.run.networkSharedPermanentLevels[i] = s.sharedPermanentUpgradeLevels[i];
  state.progression.run.roomHeat = s.roomHeat;
  state.secretTv.signal=s.tvSignal;state.secretTv.damage=s.tvDamage;state.secretTv.tolerance=s.tvTolerance;state.secretTv.broken=s.tvBroken;
  state.secretTv.available=s.tvAvailable;state.secretTv.entrancePos=s.tvEntrancePos;state.secretTv.entranceNormal=s.tvEntranceNormal;
  state.topology=s.topology;state.doorTransition=s.doorTransition;
  state.debug.colliderCount=s.roomColliderCount;state.roomColliders=s.roomColliders;
  for (auto &peer : state.multiplayer.peers)
    peer = NetworkPeerState{};
  for (const auto &p : s.players)
    if (p.active) {
      if (p.id == localPlayerId) {
        const Vec3 predictionError=p.pos-state.player.pos;
        const float correctionMagnitude=length(predictionError);
        const bool hardCorrection=!state.multiplayer.hasWorldSnapshot||correctionMagnitude>1.5f;
        if(state.multiplayer.hasWorldSnapshot&&correctionMagnitude>0.05f){
          std::printf("MULTIPLAYER_GUEST_PREDICTION_CORRECTION magnitude=%.3f mode=%s\n",correctionMagnitude,correctionMagnitude>1.5f?"snap":"smooth");
          std::fflush(stdout);
        }
        if(hardCorrection){
          state.player.pos=p.pos;
          state.multiplayer.localPredictionCorrection={};
        }else{
          state.multiplayer.localPredictionCorrection=predictionError;
        }
        if(hardCorrection)state.player.vel = p.vel;
        state.player.yaw = p.yaw;
        state.player.targetYaw=p.targetYaw;
        if(hardCorrection){state.player.jumpVel=p.jumpVel;state.player.airJumpsRemaining=p.airJumpsRemaining;}
        for(int i=0;i<PHONE_CAPACITY;++i)state.player.storedSoulBrute[i]=(p.storedSoulBruteMask&(1u<<i))!=0;
        state.player.ledgeHanging=(p.actionFlags&1)!=0;state.player.ledgeCollider=p.ledgeCollider;
        state.player.ledgeNormal=p.ledgeNormal;state.player.ledgeHangTime=p.ledgeHangTime;state.player.ledgeMantleTimer=p.ledgeMantleTimer;
        state.player.battery = p.battery;
        state.player.souls = p.souls;
        if(hardCorrection)state.player.grounded = (p.flags & 1) != 0;
        state.player.alive = (p.flags & 4) != 0;
        state.player.downed=(p.flags&8)!=0;state.player.bleedoutTimer=p.bleedoutTimer;state.player.reviveCharge=p.reviveCharge;state.player.grabEscape=p.grabEscape;state.player.grabbedByTarget=p.grabbedByTarget;
        state.player.inSecretRoom=(p.flags&16)!=0;state.player.secretVisitRoom=p.secretVisitRoom;state.player.secretVisitTimer=p.secretVisitTimer;
        state.player.commSignal=p.commSignal;state.player.commSignalTimer=p.commSignalTimer;
        // Camera pitch/mode are local presentation state. Body yaw remains
        // authoritative, but routine snapshots must not move the local camera.
        state.vacuum.power = p.vacuumPower;
        state.vacuum.pose = p.vacuumPose;
        state.vacuum.active=(p.actionFlags&2)!=0;state.vacuum.fieldStrength=p.vacuumFieldStrength;
        state.vacuum.coneTightness=p.vacuumConeTightness;state.vacuum.lockStrength=p.vacuumLockStrength;
        state.vacuum.target = p.vacuumTarget;
        state.meleeVisual.visualTimer = p.meleeTimer;
        state.meleeVisual.airLungePending=(p.actionFlags&8)!=0;
        state.meleeVisual.airLungeLandingPending=(p.actionFlags&16)!=0;
        state.meleeVisual.locomotionLunge=(p.actionFlags&32)!=0;state.meleeVisual.visualHit=(p.actionFlags&64)!=0;
        state.meleeVisual.variant=p.meleeVariant;state.meleeVisual.comboIndex=p.meleeComboIndex;
        state.meleeVisual.direction=p.meleeDirection;state.meleeVisual.airLungeRotation=p.airLungeRotation;
        state.meleeVisual.landingRecovery=p.landingRecovery;
        state.energy.dischargePositionAmount = p.dischargeAmount;
        state.energy.dischargeTimer=p.dischargeTimer;state.energy.supplementalActive=(p.actionFlags&4)!=0;
        state.energy.supplementalValue=p.supplementalValue;state.energy.supplementalMax=p.supplementalMax;
        state.energy.flowerStacks=p.flowerStacks;
        state.phonePose.pitch=p.phonePitch;state.phonePose.roll=p.phoneRoll;state.phonePose.yaw=p.phoneYaw;
        state.phonePose.lift=p.phoneLift;state.phonePose.forward=p.phoneForward;state.phonePose.side=p.phoneSide;
        state.phonePose.doubleJumpTimer=p.doubleJumpTimer;state.phonePose.doubleJumpFlipYaw=p.doubleJumpFlipYaw;
        state.phonePose.doubleJumpFlip=p.doubleJumpFlip;state.phonePose.actionState=p.phoneActionState;
      } else if (p.id < MAX_PLAYERS) {
        auto &peer = state.multiplayer.peers[p.id];
        peer.active = true;
        peer.playerId = p.id;
        peer.player.pos = p.pos;
        peer.player.vel = p.vel;
        peer.player.yaw = p.yaw;
        peer.player.targetYaw=p.targetYaw;peer.player.jumpVel=p.jumpVel;peer.player.airJumpsRemaining=p.airJumpsRemaining;
        for(int i=0;i<PHONE_CAPACITY;++i)peer.player.storedSoulBrute[i]=(p.storedSoulBruteMask&(1u<<i))!=0;
        peer.player.ledgeHanging=(p.actionFlags&1)!=0;peer.player.ledgeCollider=p.ledgeCollider;
        peer.player.ledgeNormal=p.ledgeNormal;peer.player.ledgeHangTime=p.ledgeHangTime;peer.player.ledgeMantleTimer=p.ledgeMantleTimer;
        peer.player.battery = p.battery;
        peer.player.souls = p.souls;
        peer.player.grounded = (p.flags & 1) != 0;
        peer.player.alive = (p.flags & 4) != 0;
        peer.player.downed=(p.flags&8)!=0;peer.player.bleedoutTimer=p.bleedoutTimer;peer.player.reviveCharge=p.reviveCharge;peer.player.grabEscape=p.grabEscape;peer.player.grabbedByTarget=p.grabbedByTarget;
        peer.player.inSecretRoom=(p.flags&16)!=0;peer.player.secretVisitRoom=p.secretVisitRoom;peer.player.secretVisitTimer=p.secretVisitTimer;
        peer.player.commSignal=p.commSignal;peer.player.commSignalTimer=p.commSignalTimer;
        peer.camera.pitch = p.pitch;
        peer.camera.firstPerson = (p.flags & 2) != 0;
        peer.vacuum.power = p.vacuumPower;
        peer.vacuum.pose = p.vacuumPose;
        peer.vacuum.active=(p.actionFlags&2)!=0;peer.vacuum.fieldStrength=p.vacuumFieldStrength;
        peer.vacuum.coneTightness=p.vacuumConeTightness;peer.vacuum.lockStrength=p.vacuumLockStrength;
        peer.vacuum.target = p.vacuumTarget;
        peer.meleeVisual.visualTimer = p.meleeTimer;
        peer.meleeVisual.airLungePending=(p.actionFlags&8)!=0;
        peer.meleeVisual.airLungeLandingPending=(p.actionFlags&16)!=0;
        peer.meleeVisual.locomotionLunge=(p.actionFlags&32)!=0;peer.meleeVisual.visualHit=(p.actionFlags&64)!=0;
        peer.meleeVisual.variant=p.meleeVariant;peer.meleeVisual.comboIndex=p.meleeComboIndex;
        peer.meleeVisual.direction=p.meleeDirection;peer.meleeVisual.airLungeRotation=p.airLungeRotation;
        peer.meleeVisual.landingRecovery=p.landingRecovery;
        peer.energy.dischargePositionAmount = p.dischargeAmount;
        peer.energy.dischargeTimer=p.dischargeTimer;peer.energy.supplementalActive=(p.actionFlags&4)!=0;
        peer.energy.supplementalValue=p.supplementalValue;peer.energy.supplementalMax=p.supplementalMax;
        peer.energy.flowerStacks=p.flowerStacks;
        peer.phonePose.pitch=p.phonePitch;peer.phonePose.roll=p.phoneRoll;peer.phonePose.yaw=p.phoneYaw;
        peer.phonePose.lift=p.phoneLift;peer.phonePose.forward=p.phoneForward;peer.phonePose.side=p.phoneSide;
        peer.phonePose.doubleJumpTimer=p.doubleJumpTimer;peer.phonePose.doubleJumpFlipYaw=p.doubleJumpFlipYaw;
        peer.phonePose.doubleJumpFlip=p.doubleJumpFlip;peer.phonePose.actionState=p.phoneActionState;
        peer.phoneVisual =
            makePhoneVisualState(p.vacuumPose, p.vacuumPower, 0, s.time, false);
        peer.phoneTransform.position = p.pos + Vec3{0, 0.54f, 0};
        peer.phoneTransform.orientation = peer.player.downed?quatNormalized(quatAxisAngle({0,1,0},p.yaw)*quatAxisAngle({1,0,0},DB_PI*0.5f)):quatAxisAngle({0, 1, 0}, p.yaw);
        peer.phoneTransform.screenRight =
            rotate(peer.phoneTransform.orientation, {1, 0, 0});
        peer.phoneTransform.screenUp =
            rotate(peer.phoneTransform.orientation, {0, 1, 0});
        peer.phoneTransform.screenNormal =
            rotate(peer.phoneTransform.orientation, {0, 0, 1});
        peer.phoneTransform.screenCenter =
            peer.phoneTransform.position +
            peer.phoneTransform.screenNormal * PHONE_SCREEN_Z_OFFSET;
        peer.phoneTransform.vacuumPullPoint =
            peer.phoneTransform.screenCenter +
            peer.phoneTransform.screenNormal * 0.24f;
      }
    }
  for (int i = 0; i < TARGET_COUNT; ++i) {
    const auto &a = s.targets[i];
    auto &b = state.targets[i];
    const bool visualTransition=state.multiplayer.hasWorldSnapshot&&
      (b.alive!=((a.flags&1)!=0)||b.slurpable!=((a.flags&2)!=0)||
       b.soulState!=a.soulState||(b.attackTimer<=0.0f)!=(a.attackTimer<=0.0f));
    if(visualTransition){
      std::printf("MULTIPLAYER_ENEMY_VISUAL_TRANSITION target=%d alive=%d soul=%d attack=%d\n",i,(a.flags&1)!=0,static_cast<int>(a.soulState),a.attackTimer>0.0f);
      std::fflush(stdout);
    }
    b.alive = (a.flags & 1) != 0;
    b.slurpable = (a.flags & 2) != 0;
    b.brute = (a.flags & 4) != 0;
    b.captureQueued = (a.flags & 8) != 0;
    b.captureCommitted = (a.flags & 16) != 0;
    b.soulState = a.soulState;
    const Vec3 previousVisualPos=b.pos+b.networkVisualOffset;
    const bool hadSnapshot=state.multiplayer.hasWorldSnapshot;
    b.pos = a.pos;
    b.networkVisualOffset=hadSnapshot&&!roomChanged?previousVisualPos-a.pos:Vec3{};
    b.vel = a.vel;
    b.armor = a.armor;
    b.health = a.health;
    b.capture = a.capture;
    b.ingestProgress = a.ingest;
    b.recoilTime = a.recoil;
    b.scale = a.scale;
    b.visualYaw = a.visualYaw;
    b.soulMorph = a.soulMorph;
    b.attackTimer = a.attackTimer;
    b.attackCooldown = a.attackCooldown;
    b.humanAnimationTime = a.animationTime;
    b.visualWalkPhase = a.visualWalkPhase;
    b.locomotionAmount = a.locomotionAmount;
    b.attackDirection = a.attackDirection;
    b.attackVariant = a.attackVariant;
    b.attackHit = (a.visualFlags & 1) != 0;
    b.latchedToScreen = (a.visualFlags & 2) != 0;
    b.hitFlash = a.hitFlash;
    b.phase=a.phase;b.floatOffset=a.floatOffset;b.spinSpeed=a.spinSpeed;
    b.hitDirectionLocal=a.hitDirectionLocal;b.vacuumPullAmount=a.vacuumPullAmount;
    b.captureCollapseAmount=a.captureCollapseAmount;b.visibility=a.visibility;
    b.armorRegenDelay=a.armorRegenDelay;b.respawnTimer=a.respawnTimer;
    b.visualReaction=makeHumanReactionVisual(
      b.visualWalkPhase,b.locomotionAmount,b.hitFlash,b.hitDirectionLocal,
      b.vacuumPullAmount,b.captureCollapseAmount,b.soulMorph,b.visibility>0.5f,
      b.attackTimer,b.attackVariant);
    b.networkOwnerPlayerId = a.ownerPlayerId;
    b.grabbedPlayerId=a.grabbedPlayerId;b.grabCooldown=a.grabCooldown;
  }
  const bool initialSnapshot = !state.multiplayer.hasWorldSnapshot;
  for (int i = 0; i < CAPTURE_COUNT; ++i) {
    auto &capture = state.captures[i];
    capture.pos=s.capturePositions[i];
    capture.filled = s.captures[i];
    if (!capture.filled) {
      capture.tokenAwarded = false;
    } else if (!capture.tokenAwarded) {
      capture.tokenAwarded = true;
      if (!initialSnapshot) {
        ++state.progression.permanent.tokens;
        ++state.progression.permanent.revision;
      }
    }
  }
  state.multiplayer.hasWorldSnapshot = true;
  for (int i = 0; i < BULLET_COUNT; ++i) {
    const auto &a = s.bullets[i];
    auto &b = state.bullets[i];
    b.alive = a.active;
    b.brute = a.brute;
    b.pos = a.pos;
    b.vel = a.vel;
    b.life = a.life;
    b.spin = a.spin;
  }
  for (int i = 0; i < FLOWER_POWERUP_COUNT; ++i) {
    const auto &a = s.flowers[i];
    auto &b = state.flowers[i];
    b.active = a.active;
    b.pos = a.pos;
    b.age = a.age;
    b.rotationY = a.rotation;
  }
}
}
