#include "MultiplayerProtocol.hpp"

#include <algorithm>
#include <cmath>
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
    void quat(const Quat& q){f32(q.w);f32(q.x);f32(q.y);f32(q.z);}
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
    bool vec(Vec3& v){return f32(v.x)&&f32(v.y)&&f32(v.z);}
    bool quat(Quat& q){return f32(q.w)&&f32(q.x)&&f32(q.y)&&f32(q.z);}
    bool done()const{return at==size;}
private: const std::uint8_t* data;std::size_t size=0,at=0;
};

void header(Writer& w,MessageType type,std::uint8_t playerId,std::uint32_t seq,std::uint32_t tick,std::uint32_t payload){w.u32(MAGIC);w.u16(PROTOCOL_VERSION);w.u8(static_cast<std::uint8_t>(type));w.u8(playerId);w.u32(seq);w.u32(tick);w.u32(payload);}
void writeWorldContext(Writer& w,const NetworkWorldContext& c){w.u32(c.sessionId);w.u32(c.runGeneration);w.u32(c.roomGeneration);w.u16(c.roomIndex);}
bool readWorldContext(Reader& r,NetworkWorldContext& c){return r.u32(c.sessionId)&&r.u32(c.runGeneration)&&r.u32(c.roomGeneration)&&r.u16(c.roomIndex);}

void writePlayer(Writer& w,const PlayerSnapshot& p){
  w.u8(p.active?1:0);w.u8(p.id);w.vec(p.pos);w.vec(p.vel);
  w.f32(p.yaw);w.f32(p.targetYaw);w.f32(p.pitch);w.f32(p.jumpVel);
  w.f32(p.battery);w.u8(p.souls);w.u8(p.flags);w.u8(p.actionFlags);
  w.u8(static_cast<std::uint8_t>(p.locomotion));w.u8(static_cast<std::uint8_t>(p.action));
  w.u8(static_cast<std::uint8_t>(p.actionPhase));w.u16(p.actionSequence);
  w.u16(p.actionTargetId);w.f32(p.actionProgress);
  w.u8(p.storedSoulBruteMask);w.i8(p.airJumpsRemaining);w.i8(p.ledgeCollider);
  w.vec(p.ledgeNormal);w.f32(p.ledgeHangTime);w.f32(p.ledgeMantleTimer);
  w.f32(p.vacuumPower);w.f32(p.vacuumPose);w.f32(p.vacuumFieldStrength);
  w.f32(p.vacuumConeTightness);w.f32(p.vacuumLockStrength);w.i8(p.vacuumTarget);
  w.f32(p.meleeTimer);w.f32(p.dischargeAmount);w.f32(p.dischargeTimer);
  w.f32(p.supplementalValue);w.f32(p.supplementalMax);w.u8(p.flowerStacks);
  w.f32(p.phonePitch);w.f32(p.phoneRoll);w.f32(p.phoneYaw);w.f32(p.phoneLift);
  w.f32(p.phoneForward);w.f32(p.phoneSide);w.f32(p.doubleJumpTimer);
  w.f32(p.doubleJumpFlipYaw);w.f32(p.doubleJumpFlip);w.quat(p.phoneOrientation);w.u8(p.phoneActionState);
  w.u8(p.meleeVariant);w.u8(p.meleeComboIndex);w.vec(p.meleeDirection);
  w.f32(p.airLungeRotation);w.f32(p.landingRecovery);
  w.f32(p.bleedoutTimer);w.f32(p.reviveCharge);w.f32(p.grabEscape);
  w.i8(p.grabbedByTarget);w.i32(p.secretVisitRoom);w.f32(p.secretVisitTimer);
  w.u8(p.commSignal);w.f32(p.commSignalTimer);
}
bool readPlayer(Reader& r,PlayerSnapshot& p){
  std::uint8_t active=0,locomotion=0,action=0,phase=0;
  return r.u8(active)&&((p.active=active!=0),true)&&r.u8(p.id)&&r.vec(p.pos)&&r.vec(p.vel)&&
    r.f32(p.yaw)&&r.f32(p.targetYaw)&&r.f32(p.pitch)&&r.f32(p.jumpVel)&&
    r.f32(p.battery)&&r.u8(p.souls)&&r.u8(p.flags)&&r.u8(p.actionFlags)&&
    r.u8(locomotion)&&locomotion<=static_cast<std::uint8_t>(NetLocomotionState::Dead)&&((p.locomotion=static_cast<NetLocomotionState>(locomotion)),true)&&
    r.u8(action)&&action<=static_cast<std::uint8_t>(NetActionState::Revive)&&((p.action=static_cast<NetActionState>(action)),true)&&
    r.u8(phase)&&phase<=static_cast<std::uint8_t>(NetActionPhase::Recovery)&&((p.actionPhase=static_cast<NetActionPhase>(phase)),true)&&
    r.u16(p.actionSequence)&&r.u16(p.actionTargetId)&&r.f32(p.actionProgress)&&
    r.u8(p.storedSoulBruteMask)&&r.i8(p.airJumpsRemaining)&&r.i8(p.ledgeCollider)&&
    r.vec(p.ledgeNormal)&&r.f32(p.ledgeHangTime)&&r.f32(p.ledgeMantleTimer)&&
    r.f32(p.vacuumPower)&&r.f32(p.vacuumPose)&&r.f32(p.vacuumFieldStrength)&&
    r.f32(p.vacuumConeTightness)&&r.f32(p.vacuumLockStrength)&&r.i8(p.vacuumTarget)&&
    r.f32(p.meleeTimer)&&r.f32(p.dischargeAmount)&&r.f32(p.dischargeTimer)&&
    r.f32(p.supplementalValue)&&r.f32(p.supplementalMax)&&r.u8(p.flowerStacks)&&
    r.f32(p.phonePitch)&&r.f32(p.phoneRoll)&&r.f32(p.phoneYaw)&&r.f32(p.phoneLift)&&
    r.f32(p.phoneForward)&&r.f32(p.phoneSide)&&r.f32(p.doubleJumpTimer)&&
    r.f32(p.doubleJumpFlipYaw)&&r.f32(p.doubleJumpFlip)&&r.quat(p.phoneOrientation)&&r.u8(p.phoneActionState)&&
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

WorldContextCompatibility compareWorldContext(const NetworkWorldContext& packet,const NetworkWorldContext& current){
  if(packet.sessionId!=current.sessionId||packet.runGeneration!=current.runGeneration)return WorldContextCompatibility::Incompatible;
  if(packet.roomGeneration<current.roomGeneration)return WorldContextCompatibility::Older;
  if(packet.roomGeneration>current.roomGeneration)return WorldContextCompatibility::NewerRoom;
  return packet.roomIndex==current.roomIndex?WorldContextCompatibility::Compatible:WorldContextCompatibility::Incompatible;
}

std::vector<std::uint8_t> encodeInput(std::uint8_t playerId,const NetworkWorldContext& world,const InputCommand& input){Writer payload;writeWorldContext(payload,world);payload.f32(input.moveX);payload.f32(input.moveZ);payload.f32(input.yaw);payload.f32(input.pitch);payload.u16(input.buttons);Writer packet;header(packet,MessageType::Input,playerId,input.sequence,input.localTick,static_cast<std::uint32_t>(payload.data.size()));packet.data.insert(packet.data.end(),payload.data.begin(),payload.data.end());return packet.data;}
bool decodeInput(const std::uint8_t* data,std::size_t size,PacketHeader& h,NetworkWorldContext& world,InputCommand& input){if(!decodeHeader(data,size,h)||h.type!=MessageType::Input)return false;Reader r(data+HEADER_BYTES,h.payloadBytes);input.sequence=h.sequence;input.localTick=h.tick;return readWorldContext(r,world)&&r.f32(input.moveX)&&r.f32(input.moveZ)&&r.f32(input.yaw)&&r.f32(input.pitch)&&r.u16(input.buttons)&&r.done();}
std::vector<std::uint8_t> encodeInput(std::uint8_t playerId,const InputCommand& input){return encodeInput(playerId,{},input);}
bool decodeInput(const std::uint8_t* data,std::size_t size,PacketHeader& h,InputCommand& input){NetworkWorldContext ignored;return decodeInput(data,size,h,ignored,input);}

std::vector<std::uint8_t> encodeEvent(std::uint8_t playerId,const GameplayEvent& event){
  Writer payload;writeWorldContext(payload,event.world);
  payload.u8(static_cast<std::uint8_t>(event.type));
  payload.u16(event.sourceEntityId);payload.u16(event.targetEntityId);
  payload.vec(event.position);payload.vec(event.direction);payload.u16(event.flags);
  Writer packet;header(packet,MessageType::Event,playerId,event.eventId,event.authoritativeTick,static_cast<std::uint32_t>(payload.data.size()));
  packet.data.insert(packet.data.end(),payload.data.begin(),payload.data.end());return packet.data;
}
bool decodeEvent(const std::uint8_t* data,std::size_t size,PacketHeader& h,GameplayEvent& event){
  if(!decodeHeader(data,size,h)||h.type!=MessageType::Event)return false;
  Reader r(data+HEADER_BYTES,h.payloadBytes);std::uint8_t type=0;
  event.eventId=h.sequence;event.authoritativeTick=h.tick;
  return readWorldContext(r,event.world)&&r.u8(type)&&
    type<=static_cast<std::uint8_t>(GameplayEventType::ProjectileDespawned)&&
    ((event.type=static_cast<GameplayEventType>(type)),true)&&
    r.u16(event.sourceEntityId)&&r.u16(event.targetEntityId)&&
    r.vec(event.position)&&r.vec(event.direction)&&r.u16(event.flags)&&r.done();
}

std::vector<GameplayEvent> deriveMeleeEvents(
    const WorldSnapshot& previous,const WorldSnapshot& current,
    std::uint32_t& nextEventId){
  std::vector<GameplayEvent> events;
  auto append=[&](GameplayEventType type,std::uint16_t source,
                  std::uint16_t target,const Vec3& position,
                  const Vec3& direction){
    GameplayEvent event;
    event.world=current.world;event.authoritativeTick=current.tick;
    event.eventId=++nextEventId;event.type=type;
    event.sourceEntityId=source;event.targetEntityId=target;
    event.position=position;event.direction=direction;
    events.push_back(event);
  };
  for(std::size_t i=0;i<current.players.size();++i){
    const auto& before=previous.players[i];
    const auto& now=current.players[i];
    if(now.active&&now.actionSequence!=0&&
       now.actionSequence!=before.actionSequence)
      append(GameplayEventType::PlayerActionStarted,
             static_cast<std::uint16_t>(i),now.actionTargetId,
             now.pos,now.meleeDirection);
    if(now.active&&now.actionPhase==NetActionPhase::Contact&&
       before.actionPhase!=NetActionPhase::Contact)
      append(GameplayEventType::PlayerActionContact,
             static_cast<std::uint16_t>(i),now.actionTargetId,
             now.pos,now.meleeDirection);
  }
  for(std::size_t i=0;i<current.targets.size();++i){
    const auto& before=previous.targets[i];
    const auto& now=current.targets[i];
    if(now.armor<before.armor)
      append(GameplayEventType::EnemyHitConfirmed,0,
             static_cast<std::uint16_t>(i),now.pos,now.vel);
    if((before.flags&2u)==0&&(now.flags&2u)!=0)
      append(GameplayEventType::EnemyShellBroken,0,
             static_cast<std::uint16_t>(i),now.pos,now.vel);
    if(before.soulMorph<=0.0f&&now.soulMorph>0.0f)
      append(GameplayEventType::SoulEmergenceStarted,0,
             static_cast<std::uint16_t>(i),now.pos,now.vel);
  }
  return events;
}

void GameplayEventTracker::reset(const NetworkWorldContext& world){world_=world;lastEventId_=0;}
bool GameplayEventTracker::accept(const GameplayEvent& event){
  if(event.world!=world_||event.eventId==0||event.eventId<=lastEventId_)return false;
  lastEventId_=event.eventId;return true;
}

std::vector<std::uint8_t> encodeSnapshot(std::uint8_t playerId,
                                         const WorldSnapshot &s,
                                         std::uint32_t sequence) {
  Writer p;
  writeWorldContext(p,s.world);
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
  if (!readWorldContext(r,s.world) || !r.f32(s.time) || !r.i32(s.roomIndex) || !r.i32(s.roomSeed) ||
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
    const float speed=horizontalLength(player.vel);
    out.locomotion=!player.alive?NetLocomotionState::Dead:
      player.ledgeMantleTimer>0?NetLocomotionState::LedgeMantle:
      player.ledgeHanging?NetLocomotionState::LedgeHang:
      !player.grounded?NetLocomotionState::Airborne:
      speed>4.0f?NetLocomotionState::Sprinting:
      speed>0.05f?NetLocomotionState::Walking:NetLocomotionState::Idle;
    out.action=player.grabbedByTarget>=0?NetActionState::Grabbed:
      player.downed?NetActionState::DamageReaction:
      melee.locomotionLunge?NetActionState::AirLunge:
      melee.visualTimer>0?NetActionState::Melee:
      energy.dischargeTimer>0?NetActionState::Discharge:
      vacuum.active?NetActionState::Vacuum:NetActionState::None;
    out.actionPhase=out.action==NetActionState::None?NetActionPhase::None:
      melee.visualHit?NetActionPhase::Contact:
      melee.visualTimer>melee.visualDuration*0.7f?NetActionPhase::Startup:
      melee.visualTimer>0?NetActionPhase::Active:
      player.downed?NetActionPhase::Recovery:NetActionPhase::Active;
    out.actionSequence=melee.actionSequence;
    out.actionTargetId=vacuum.target>=0?static_cast<std::uint16_t>(vacuum.target):0xffffu;
    out.actionProgress=melee.visualDuration>0?clampf(1.0f-melee.visualTimer/melee.visualDuration,0.0f,1.0f):
      std::max(vacuum.pose,energy.dischargePositionAmount);
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
    out.doubleJumpFlip=phonePose.doubleJumpFlip;out.phoneOrientation=phonePose.orientation;
    out.phoneActionState=static_cast<std::uint8_t>(phonePose.actionState);
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
        const bool hardCorrection=!state.multiplayer.hasWorldSnapshot||correctionMagnitude>LOCAL_PREDICTION_SNAP_DISTANCE;
        if(state.multiplayer.hasWorldSnapshot&&correctionMagnitude>LOCAL_PREDICTION_LOG_DISTANCE){
          std::printf("MULTIPLAYER_GUEST_PREDICTION_CORRECTION magnitude=%.3f mode=%s\n",correctionMagnitude,correctionMagnitude>LOCAL_PREDICTION_SNAP_DISTANCE?"snap":"smooth");
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
        state.meleeVisual.actionSequence=p.actionSequence;
        state.meleeVisual.direction=p.meleeDirection;state.meleeVisual.airLungeRotation=p.airLungeRotation;
        state.meleeVisual.landingRecovery=p.landingRecovery;
        state.energy.dischargePositionAmount = p.dischargeAmount;
        state.energy.dischargeTimer=p.dischargeTimer;state.energy.supplementalActive=(p.actionFlags&4)!=0;
        state.energy.supplementalValue=p.supplementalValue;state.energy.supplementalMax=p.supplementalMax;
        state.energy.flowerStacks=p.flowerStacks;
        state.phonePose.pitch=p.phonePitch;state.phonePose.roll=p.phoneRoll;state.phonePose.yaw=p.phoneYaw;
        state.phonePose.lift=p.phoneLift;state.phonePose.forward=p.phoneForward;state.phonePose.side=p.phoneSide;
        state.phonePose.doubleJumpTimer=p.doubleJumpTimer;state.phonePose.doubleJumpFlipYaw=p.doubleJumpFlipYaw;
        state.phonePose.doubleJumpFlip=p.doubleJumpFlip;state.phonePose.orientation=p.phoneOrientation;
        state.phonePose.actionState=p.phoneActionState;
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
        peer.meleeVisual.actionSequence=p.actionSequence;
        peer.meleeVisual.direction=p.meleeDirection;peer.meleeVisual.airLungeRotation=p.airLungeRotation;
        peer.meleeVisual.landingRecovery=p.landingRecovery;
        peer.energy.dischargePositionAmount = p.dischargeAmount;
        peer.energy.dischargeTimer=p.dischargeTimer;peer.energy.supplementalActive=(p.actionFlags&4)!=0;
        peer.energy.supplementalValue=p.supplementalValue;peer.energy.supplementalMax=p.supplementalMax;
        peer.energy.flowerStacks=p.flowerStacks;
        peer.phonePose.pitch=p.phonePitch;peer.phonePose.roll=p.phoneRoll;peer.phonePose.yaw=p.phoneYaw;
        peer.phonePose.lift=p.phoneLift;peer.phonePose.forward=p.phoneForward;peer.phonePose.side=p.phoneSide;
        peer.phonePose.doubleJumpTimer=p.doubleJumpTimer;peer.phonePose.doubleJumpFlipYaw=p.doubleJumpFlipYaw;
        peer.phonePose.doubleJumpFlip=p.doubleJumpFlip;peer.phonePose.orientation=p.phoneOrientation;
        peer.phonePose.actionState=p.phoneActionState;
        peer.phoneVisual =
            makePhoneVisualState(p.vacuumPose, p.vacuumPower, 0, s.time, false);
        const float phoneActionAmount=std::max(p.vacuumPose,p.dischargeAmount);
        peer.phoneVisual.actionLift=phoneActionAmount*0.65f;
        peer.phoneVisual.actionForward=phoneActionAmount*0.25f;
        const Vec3 forward{-std::sin(p.yaw),0.0f,-std::cos(p.yaw)};
        const Vec3 right{std::cos(p.yaw),0.0f,-std::sin(p.yaw)};
        peer.phoneTransform.position=p.pos+Vec3{0,peer.phonePose.lift+peer.phoneVisual.actionLift,0}
          +forward*(peer.phonePose.forward-peer.phoneVisual.actionForward)+right*peer.phonePose.side;
        peer.phoneTransform.orientation=peer.player.downed
          ?quatNormalized(quatAxisAngle({0,1,0},p.yaw)*quatAxisAngle({1,0,0},DB_PI*0.5f))
          :quatNormalized(peer.phonePose.orientation*
             quatAxisAngle({1,0,0},peer.phoneVisual.pitch)*
             quatAxisAngle({0,0,1},peer.phoneVisual.roll));
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
    b.pos = a.pos;
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

namespace {
struct StableHash {
  std::uint64_t value=1469598103934665603ull;
  void byte(std::uint8_t v){value^=v;value*=1099511628211ull;}
  void u32(std::uint32_t v){for(int i=0;i<4;++i)byte(static_cast<std::uint8_t>(v>>(i*8)));}
  void i32(std::int32_t v){u32(static_cast<std::uint32_t>(v));}
  void boolean(bool v){byte(v?1:0);}
  void scalar(float v){i32(static_cast<std::int32_t>(std::lround(clampf(v,-1000000.0f,1000000.0f)*1000.0f)));}
  void vec(const Vec3& v){scalar(v.x);scalar(v.y);scalar(v.z);}
};
void hashPlayerGameplay(StableHash& h,const PlayerSnapshot& p){
  h.boolean(p.active);h.byte(p.id);h.vec(p.pos);h.vec(p.vel);h.scalar(p.yaw);
  h.scalar(p.jumpVel);h.scalar(p.battery);h.byte(p.souls);h.byte(p.flags);
  h.byte(p.actionFlags);h.byte(p.storedSoulBruteMask);h.byte(static_cast<std::uint8_t>(p.airJumpsRemaining));
  h.byte(static_cast<std::uint8_t>(p.ledgeCollider));h.vec(p.ledgeNormal);
  h.scalar(p.ledgeHangTime);h.scalar(p.ledgeMantleTimer);h.byte(static_cast<std::uint8_t>(p.vacuumTarget));
  h.scalar(p.meleeTimer);h.scalar(p.dischargeTimer);h.scalar(p.bleedoutTimer);
  h.scalar(p.reviveCharge);h.scalar(p.grabEscape);h.byte(static_cast<std::uint8_t>(p.grabbedByTarget));
  h.i32(p.secretVisitRoom);h.scalar(p.secretVisitTimer);
}
void hashTargetGameplay(StableHash& h,const TargetSnapshot& t){
  h.byte(t.flags);h.byte(static_cast<std::uint8_t>(t.soulState));h.vec(t.pos);h.vec(t.vel);
  h.scalar(t.armor);h.scalar(t.health);h.scalar(t.capture);h.scalar(t.ingest);
  h.scalar(t.recoil);h.scalar(t.attackTimer);h.scalar(t.attackCooldown);
  h.vec(t.attackDirection);h.byte(t.attackVariant);h.byte(t.visualFlags);
  h.scalar(t.armorRegenDelay);h.scalar(t.respawnTimer);
  h.byte(static_cast<std::uint8_t>(t.ownerPlayerId));h.byte(static_cast<std::uint8_t>(t.grabbedPlayerId));
}
float angleDelta(float from,float to){
  float d=std::fmod(to-from+DB_PI,DB_PI*2.0f);
  if(d<0.0f)d+=DB_PI*2.0f;
  return d-DB_PI;
}
float lerpf(float a,float b,float t){return a+(b-a)*t;}
float angleLerp(float a,float b,float t){return a+angleDelta(a,b)*t;}
Vec3 vectorLerp(const Vec3& a,const Vec3& b,float t){return a+(b-a)*t;}
bool farApart(const Vec3& a,const Vec3& b){
  return lengthSq(a-b)>REMOTE_TELEPORT_RESET_DISTANCE*REMOTE_TELEPORT_RESET_DISTANCE;
}
void rebuildPeerTransform(NetworkPeerState& peer){
  const float yaw=peer.player.yaw;
  const Vec3 forward{-std::sin(yaw),0.0f,-std::cos(yaw)};
  const Vec3 right{std::cos(yaw),0.0f,-std::sin(yaw)};
  peer.phoneTransform.position=peer.player.pos+Vec3{0,peer.phonePose.lift+peer.phoneVisual.actionLift,0}
    +forward*(peer.phonePose.forward-peer.phoneVisual.actionForward)+right*peer.phonePose.side;
  peer.phoneTransform.orientation=peer.player.downed
    ?quatNormalized(quatAxisAngle({0,1,0},yaw)*quatAxisAngle({1,0,0},DB_PI*0.5f))
    :quatNormalized(peer.phonePose.orientation*
       quatAxisAngle({1,0,0},peer.phoneVisual.pitch)*
       quatAxisAngle({0,0,1},peer.phoneVisual.roll));
  peer.phoneTransform.screenRight=normalized(rotate(peer.phoneTransform.orientation,{1,0,0}));
  peer.phoneTransform.screenUp=normalized(rotate(peer.phoneTransform.orientation,{0,1,0}));
  peer.phoneTransform.screenNormal=normalized(rotate(peer.phoneTransform.orientation,{0,0,1}));
  peer.phoneTransform.screenCenter=peer.phoneTransform.position+
    peer.phoneTransform.screenNormal*(PHONE_SCREEN_Z_OFFSET+peer.phoneVisual.screenOffset);
  peer.phoneTransform.vacuumPullPoint=peer.phoneTransform.screenCenter;
}
}

DurableSectionHashes durableSectionHashes(const WorldSnapshot& s){
  DurableSectionHashes result;
  StableHash world;
  world.i32(s.roomIndex);world.i32(s.roomSeed);world.boolean(s.started);
  world.boolean(s.dead);world.scalar(s.roomHeat);
  world.i32(s.tvSignal);world.i32(s.tvDamage);world.i32(s.tvTolerance);
  world.boolean(s.tvBroken);world.boolean(s.tvAvailable);
  world.vec(s.tvEntrancePos);world.vec(s.tvEntranceNormal);
  world.i32(s.topology.currentTileIndex);world.i32(s.topology.previousTileIndex);
  world.boolean(s.topology.advancing);
  world.boolean(s.doorTransition.active);world.scalar(s.doorTransition.progress);
  world.byte(s.roomColliderCount);
  for(const auto& collider:s.roomColliders){
    world.scalar(collider.minX);world.scalar(collider.maxX);
    world.scalar(collider.minZ);world.scalar(collider.maxZ);
    world.scalar(collider.bottomY);world.scalar(collider.topY);
    world.scalar(collider.width);world.scalar(collider.depth);
    world.scalar(collider.height);world.vec(collider.center);
  }
  for(const auto& flower:s.flowers){
    world.boolean(flower.active);world.vec(flower.pos);world.scalar(flower.age);
  }
  result.world=world.value;

  StableHash players;
  for(const auto& player:s.players)hashPlayerGameplay(players,player);
  result.players=players.value;

  StableHash targets;
  for(const auto& target:s.targets)hashTargetGameplay(targets,target);
  result.targets=targets.value;

  StableHash projectiles;
  for(const auto& bullet:s.bullets){
    projectiles.boolean(bullet.active);projectiles.boolean(bullet.brute);
    projectiles.vec(bullet.pos);projectiles.vec(bullet.vel);
    projectiles.scalar(bullet.life);
  }
  result.projectiles=projectiles.value;

  StableHash progression;
  progression.i32(s.requiredSouls);progression.i32(s.depositedSouls);
  progression.boolean(s.roomClear);
  progression.i32(s.runRules.requiredSlotStacks);
  progression.i32(s.runRules.crowdedRoomStacks);
  progression.i32(s.runRules.fasterSlurpStacks);
  progression.i32(s.runRules.nextId);progression.i32(s.runRules.lastAdded);
  progression.boolean(s.upgradeMenuActive);
  for(auto value:s.temporaryUpgradeLevels)progression.i32(value);
  for(auto value:s.sharedPermanentUpgradeLevels)progression.i32(value);
  for(std::size_t i=0;i<s.captures.size();++i){
    progression.boolean(s.captures[i]);progression.vec(s.capturePositions[i]);
  }
  result.progression=progression.value;
  return result;
}

std::uint64_t authoritativeStateHash(const WorldSnapshot& s){
  StableHash h;
  h.i32(s.roomIndex);h.i32(s.roomSeed);h.i32(s.requiredSouls);h.i32(s.depositedSouls);
  h.boolean(s.roomClear);h.boolean(s.started);h.boolean(s.dead);
  h.i32(s.runRules.requiredSlotStacks);h.i32(s.runRules.crowdedRoomStacks);
  h.i32(s.runRules.fasterSlurpStacks);h.i32(s.runRules.nextId);h.i32(s.runRules.lastAdded);
  h.boolean(s.upgradeMenuActive);
  for(auto v:s.temporaryUpgradeLevels)h.i32(v);
  for(auto v:s.sharedPermanentUpgradeLevels)h.i32(v);
  h.scalar(s.roomHeat);h.i32(s.tvSignal);h.i32(s.tvDamage);h.i32(s.tvTolerance);
  h.boolean(s.tvBroken);h.boolean(s.tvAvailable);h.vec(s.tvEntrancePos);h.vec(s.tvEntranceNormal);
  h.i32(s.topology.currentTileIndex);h.i32(s.topology.previousTileIndex);h.boolean(s.topology.advancing);
  h.boolean(s.doorTransition.active);h.scalar(s.doorTransition.progress);
  for(const auto& p:s.players)hashPlayerGameplay(h,p);
  for(const auto& t:s.targets)hashTargetGameplay(h,t);
  for(std::size_t i=0;i<s.captures.size();++i){h.boolean(s.captures[i]);h.vec(s.capturePositions[i]);}
  for(const auto& b:s.bullets){h.boolean(b.active);h.boolean(b.brute);h.vec(b.pos);h.vec(b.vel);h.scalar(b.life);}
  for(const auto& f:s.flowers){h.boolean(f.active);h.vec(f.pos);h.scalar(f.age);}
  return h.value;
}

std::uint64_t visualStateHash(const WorldSnapshot& s){
  StableHash h;
  h.i32(s.roomIndex);h.boolean(s.doorTransition.active);h.scalar(s.doorTransition.progress);
  for(const auto& p:s.players){
    h.boolean(p.active);h.byte(p.id);h.vec(p.pos);h.vec(p.vel);h.scalar(p.yaw);
    h.byte(p.flags);h.byte(p.actionFlags);h.scalar(p.vacuumPose);h.scalar(p.meleeTimer);
    h.scalar(p.dischargeAmount);h.byte(p.phoneActionState);
  }
  for(const auto& t:s.targets){
    h.byte(t.flags);h.byte(static_cast<std::uint8_t>(t.soulState));h.vec(t.pos);
    h.scalar(t.visualYaw);h.scalar(t.animationTime);h.scalar(t.locomotionAmount);
    h.scalar(t.attackTimer);h.byte(t.attackVariant);h.scalar(t.hitFlash);
    h.scalar(t.armor);h.scalar(t.soulMorph);h.scalar(t.ingest);h.scalar(t.recoil);
  }
  for(const auto& b:s.bullets){h.boolean(b.active);h.vec(b.pos);h.scalar(b.spin);}
  for(const auto& f:s.flowers){h.boolean(f.active);h.vec(f.pos);h.scalar(f.rotation);}
  return h.value;
}

void SnapshotInterpolator::reset(){
  previous_=TimedSnapshot{};current_=TimedSnapshot{};hasPrevious_=hasCurrent_=false;
}

void SnapshotInterpolator::push(const WorldSnapshot& snapshot,std::int64_t receiveTimeMs){
  if(hasCurrent_&&(snapshot.tick<=current_.snapshot.tick||
      snapshot.roomIndex!=current_.snapshot.roomIndex||
      snapshot.topology.currentTileIndex!=current_.snapshot.topology.currentTileIndex||
      snapshot.dead!=current_.snapshot.dead)){
    reset();
  }
  if(hasCurrent_){previous_=current_;hasPrevious_=true;}
  current_.snapshot=snapshot;current_.receiveTimeMs=receiveTimeMs;hasCurrent_=true;
}

void SnapshotInterpolator::apply(GameState& state,std::uint8_t localPlayerId,
                                 std::int64_t renderTimeMs) const{
  if(!hasCurrent_)return;
  const WorldSnapshot& b=current_.snapshot;
  const WorldSnapshot& a=hasPrevious_?previous_.snapshot:b;
  const std::int64_t targetTime=renderTimeMs-REMOTE_INTERPOLATION_DELAY_MS;
  float alpha=1.0f;
  float extrapolationSeconds=0.0f;
  if(hasPrevious_&&current_.receiveTimeMs>previous_.receiveTimeMs){
    alpha=clampf(static_cast<float>(targetTime-previous_.receiveTimeMs)/
      static_cast<float>(current_.receiveTimeMs-previous_.receiveTimeMs),0.0f,1.0f);
    if(targetTime>current_.receiveTimeMs)
      extrapolationSeconds=static_cast<float>(std::min<std::int64_t>(
        targetTime-current_.receiveTimeMs,REMOTE_MAX_EXTRAPOLATION_MS))*0.001f;
  }
  for(int id=0;id<MAX_PLAYERS;++id){
    if(id==localPlayerId||!b.players[id].active)continue;
    const auto& from=a.players[id];const auto& to=b.players[id];
    auto& peer=state.multiplayer.peers[id];
    const bool reset=!hasPrevious_||!from.active||farApart(from.pos,to.pos)||
      ((from.flags^to.flags)&((1u<<2)|(1u<<3)|(1u<<4)))!=0||
      from.secretVisitRoom!=to.secretVisitRoom;
    const float t=reset?1.0f:alpha;
    peer.player.pos=vectorLerp(from.pos,to.pos,t)+(reset?Vec3{}:to.vel*extrapolationSeconds);
    peer.player.vel=vectorLerp(from.vel,to.vel,t);
    peer.player.yaw=reset?to.yaw:angleLerp(from.yaw,to.yaw,t);
    peer.phonePose.pitch=lerpf(from.phonePitch,to.phonePitch,t);
    peer.phonePose.roll=lerpf(from.phoneRoll,to.phoneRoll,t);
    peer.phonePose.yaw=lerpf(from.phoneYaw,to.phoneYaw,t);
    peer.phonePose.lift=lerpf(from.phoneLift,to.phoneLift,t);
    peer.phonePose.forward=lerpf(from.phoneForward,to.phoneForward,t);
    peer.phonePose.side=lerpf(from.phoneSide,to.phoneSide,t);
    peer.phonePose.orientation=reset?to.phoneOrientation:quatSlerp(from.phoneOrientation,to.phoneOrientation,t);
    peer.vacuum.pose=lerpf(from.vacuumPose,to.vacuumPose,t);
    peer.vacuum.power=lerpf(from.vacuumPower,to.vacuumPower,t);
    peer.energy.dischargePositionAmount=lerpf(from.dischargeAmount,to.dischargeAmount,t);
    peer.meleeVisual.visualTimer=lerpf(from.meleeTimer,to.meleeTimer,t);
    peer.phoneVisual=makePhoneVisualState(peer.vacuum.pose,peer.vacuum.power,0,state.time,false);
    const float action=std::max(peer.vacuum.pose,peer.energy.dischargePositionAmount);
    peer.phoneVisual.actionLift=action*0.65f;peer.phoneVisual.actionForward=action*0.25f;
    rebuildPeerTransform(peer);
  }
  for(int i=0;i<TARGET_COUNT;++i){
    const auto& from=a.targets[i];const auto& to=b.targets[i];auto& out=state.targets[i];
    const bool reset=!hasPrevious_||((from.flags^to.flags)&0x17u)!=0||
      from.soulState!=to.soulState||farApart(from.pos,to.pos);
    const float t=reset?1.0f:alpha;
    out.pos=vectorLerp(from.pos,to.pos,t)+(reset?Vec3{}:to.vel*extrapolationSeconds);
    out.vel=vectorLerp(from.vel,to.vel,t);
    out.visualYaw=reset?to.visualYaw:angleLerp(from.visualYaw,to.visualYaw,t);
    out.humanAnimationTime=lerpf(from.animationTime,to.animationTime,t);
    out.visualWalkPhase=lerpf(from.visualWalkPhase,to.visualWalkPhase,t);
    out.locomotionAmount=lerpf(from.locomotionAmount,to.locomotionAmount,t);
    out.attackTimer=lerpf(from.attackTimer,to.attackTimer,t);
    out.hitFlash=lerpf(from.hitFlash,to.hitFlash,t);
    out.armor=lerpf(from.armor,to.armor,t);out.health=lerpf(from.health,to.health,t);
    out.soulMorph=lerpf(from.soulMorph,to.soulMorph,t);
    out.capture=lerpf(from.capture,to.capture,t);out.ingestProgress=lerpf(from.ingest,to.ingest,t);
    out.recoilTime=lerpf(from.recoil,to.recoil,t);
    out.visualReaction=makeHumanReactionVisual(out.visualWalkPhase,out.locomotionAmount,
      out.hitFlash,out.hitDirectionLocal,out.vacuumPullAmount,out.captureCollapseAmount,
      out.soulMorph,out.visibility>0.5f,out.attackTimer,out.attackVariant);
  }
  for(int i=0;i<BULLET_COUNT;++i){
    const auto& from=a.bullets[i];const auto& to=b.bullets[i];auto& out=state.bullets[i];
    if(!to.active)continue;
    const bool reset=!hasPrevious_||!from.active||from.brute!=to.brute||farApart(from.pos,to.pos);
    const float t=reset?1.0f:alpha;
    out.pos=vectorLerp(from.pos,to.pos,t)+(reset?Vec3{}:to.vel*extrapolationSeconds);
    out.spin=lerpf(from.spin,to.spin,t);out.life=lerpf(from.life,to.life,t);
  }
  for(int i=0;i<FLOWER_POWERUP_COUNT;++i){
    const auto& from=a.flowers[i];const auto& to=b.flowers[i];auto& out=state.flowers[i];
    if(to.active&&hasPrevious_&&from.active&&!farApart(from.pos,to.pos)){
      out.pos=vectorLerp(from.pos,to.pos,alpha);
      out.rotationY=angleLerp(from.rotation,to.rotation,alpha);
    }
  }
  if(hasPrevious_&&a.doorTransition.active==b.doorTransition.active){
    state.doorTransition.progress=lerpf(a.doorTransition.progress,b.doorTransition.progress,alpha);
    state.doorTransition.frameMotion=vectorLerp(a.doorTransition.frameMotion,b.doorTransition.frameMotion,alpha);
  }
}
}
