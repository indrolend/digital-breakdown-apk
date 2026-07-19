#include "MultiplayerProtocol.hpp"

#include <algorithm>
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

void writePlayer(Writer& w,const PlayerSnapshot& p){w.u8(p.active?1:0);w.u8(p.id);w.vec(p.pos);w.vec(p.vel);w.f32(p.yaw);w.f32(p.pitch);w.f32(p.battery);w.u8(p.souls);w.u8(p.flags);w.f32(p.vacuumPower);w.f32(p.vacuumPose);w.i8(p.vacuumTarget);w.f32(p.meleeTimer);w.f32(p.dischargeAmount);}
bool readPlayer(Reader& r,PlayerSnapshot& p){std::uint8_t active=0;return r.u8(active)&&((p.active=active!=0),true)&&r.u8(p.id)&&r.vec(p.pos)&&r.vec(p.vel)&&r.f32(p.yaw)&&r.f32(p.pitch)&&r.f32(p.battery)&&r.u8(p.souls)&&r.u8(p.flags)&&r.f32(p.vacuumPower)&&r.f32(p.vacuumPose)&&r.i8(p.vacuumTarget)&&r.f32(p.meleeTimer)&&r.f32(p.dischargeAmount);}
void writeTarget(Writer& w,const TargetSnapshot& t){w.u8(t.flags);w.u8(static_cast<std::uint8_t>(t.soulState));w.vec(t.pos);w.vec(t.vel);w.f32(t.armor);w.f32(t.health);w.f32(t.capture);w.f32(t.ingest);w.f32(t.recoil);w.f32(t.scale);w.f32(t.visualYaw);w.f32(t.soulMorph);w.f32(t.attackTimer);w.f32(t.attackCooldown);w.i8(t.ownerPlayerId);}
bool readTarget(Reader& r,TargetSnapshot& t){std::uint8_t soul=0;if(!r.u8(t.flags)||!r.u8(soul)||soul>static_cast<std::uint8_t>(SoulState::Revolving))return false;t.soulState=static_cast<SoulState>(soul);return r.vec(t.pos)&&r.vec(t.vel)&&r.f32(t.armor)&&r.f32(t.health)&&r.f32(t.capture)&&r.f32(t.ingest)&&r.f32(t.recoil)&&r.f32(t.scale)&&r.f32(t.visualYaw)&&r.f32(t.soulMorph)&&r.f32(t.attackTimer)&&r.f32(t.attackCooldown)&&r.i8(t.ownerPlayerId);}
void writeBullet(Writer& w,const BulletSnapshot& b){w.u8(b.active?1:0);w.u8(b.brute?1:0);w.vec(b.pos);w.vec(b.vel);w.f32(b.life);w.f32(b.spin);}
bool readBullet(Reader& r,BulletSnapshot& b){std::uint8_t active=0,brute=0;return r.u8(active)&&r.u8(brute)&&((b.active=active!=0),(b.brute=brute!=0),true)&&r.vec(b.pos)&&r.vec(b.vel)&&r.f32(b.life)&&r.f32(b.spin);}
void writeFlower(Writer& w,const FlowerSnapshot& f){w.u8(f.active?1:0);w.vec(f.pos);w.f32(f.age);w.f32(f.rotation);}
bool readFlower(Reader& r,FlowerSnapshot& f){std::uint8_t active=0;return r.u8(active)&&((f.active=active!=0),true)&&r.vec(f.pos)&&r.f32(f.age)&&r.f32(f.rotation);}

}

bool decodeHeader(const std::uint8_t* data,std::size_t size,PacketHeader& out){if(!data||size<HEADER_BYTES||size>MAX_PACKET_BYTES)return false;Reader r(data,size);std::uint32_t magic=0;std::uint16_t version=0;std::uint8_t type=0;if(!r.u32(magic)||!r.u16(version)||!r.u8(type)||!r.u8(out.playerId)||!r.u32(out.sequence)||!r.u32(out.tick)||!r.u32(out.payloadBytes))return false;if(magic!=MAGIC||version!=PROTOCOL_VERSION||type<1||type>5||out.playerId>=MAX_PLAYERS||out.payloadBytes!=size-HEADER_BYTES)return false;out.type=static_cast<MessageType>(type);return true;}

std::vector<std::uint8_t> encodeInput(std::uint8_t playerId,const InputCommand& input){Writer payload;payload.f32(input.moveX);payload.f32(input.moveZ);payload.f32(input.yaw);payload.f32(input.pitch);payload.u16(input.buttons);Writer packet;header(packet,MessageType::Input,playerId,input.sequence,input.tick,static_cast<std::uint32_t>(payload.data.size()));packet.data.insert(packet.data.end(),payload.data.begin(),payload.data.end());return packet.data;}
bool decodeInput(const std::uint8_t* data,std::size_t size,PacketHeader& h,InputCommand& input){if(!decodeHeader(data,size,h)||h.type!=MessageType::Input)return false;Reader r(data+HEADER_BYTES,h.payloadBytes);input.sequence=h.sequence;input.tick=h.tick;return r.f32(input.moveX)&&r.f32(input.moveZ)&&r.f32(input.yaw)&&r.f32(input.pitch)&&r.u16(input.buttons)&&r.done();}

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
  p.i32(s.runRules.requiredSlotStacks);
  p.i32(s.runRules.crowdedRoomStacks);
  p.i32(s.runRules.fasterSlurpStacks);
  p.i32(s.runRules.nextId);
  p.i32(s.runRules.lastAdded);
  p.u8(s.upgradeMenuActive ? 1 : 0);
  for (auto level : s.temporaryUpgradeLevels)
    p.i32(level);
  p.f32(s.roomHeat);
  for (const auto &player : s.players)
    writePlayer(p, player);
  for (const auto &target : s.targets)
    writeTarget(p, target);
  for (bool capture : s.captures)
    p.u8(capture ? 1 : 0);
  for (const auto &bullet : s.bullets)
    writeBullet(p, bullet);
  for (const auto &flower : s.flowers)
    writeFlower(p, flower);
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
  std::uint8_t clear = 0, upgradeMenu = 0;
  if (!r.f32(s.time) || !r.i32(s.roomIndex) || !r.i32(s.roomSeed) ||
      !r.i32(s.requiredSouls) || !r.i32(s.depositedSouls) || !r.u8(clear))
    return false;
  s.roomClear = clear != 0;
  if (!r.i32(s.runRules.requiredSlotStacks) ||
      !r.i32(s.runRules.crowdedRoomStacks) ||
      !r.i32(s.runRules.fasterSlurpStacks) || !r.i32(s.runRules.nextId) ||
      !r.i32(s.runRules.lastAdded) || !r.u8(upgradeMenu))
    return false;
  s.upgradeMenuActive = upgradeMenu != 0;
  for (auto &level : s.temporaryUpgradeLevels)
    if (!r.i32(level))
      return false;
  if (!r.f32(s.roomHeat))
    return false;
  for (auto &player : s.players)
    if (!readPlayer(r, player))
      return false;
  for (auto &target : s.targets)
    if (!readTarget(r, target))
      return false;
  for (std::size_t i = 0; i < s.captures.size(); ++i) {
    std::uint8_t value = 0;
    if (!r.u8(value))
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
                 const MeleeVisualState &melee, const EnergyState &energy) {
    out.active = true;
    out.id = static_cast<std::uint8_t>(id);
    out.pos = player.pos;
    out.vel = player.vel;
    out.yaw = player.yaw;
    out.pitch = camera.pitch;
    out.battery = player.battery;
    out.souls = static_cast<std::uint8_t>(
        std::max(0, std::min(PHONE_CAPACITY, player.souls)));
    out.flags = (player.grounded ? 1 : 0) | (camera.firstPerson ? 2 : 0) |
                (player.alive ? 4 : 0);
    out.vacuumPower = vacuum.power;
    out.vacuumPose = vacuum.pose;
    out.vacuumTarget = static_cast<std::int8_t>(vacuum.target);
    out.meleeTimer = melee.visualTimer;
    out.dischargeAmount = energy.dischargePositionAmount;
  };
  const int local =
      state.multiplayer.enabled ? state.multiplayer.localPlayerId : 0;
  fill(players[local], local, state.player, state.camera, state.vacuum,
       state.meleeVisual, state.energy);
  for (const auto &peer : state.multiplayer.peers)
    if (peer.active && peer.playerId >= 0 && peer.playerId < MAX_PLAYERS &&
        peer.playerId != local)
      fill(players[peer.playerId], peer.playerId, peer.player, peer.camera,
           peer.vacuum, peer.meleeVisual, peer.energy);
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
  s.runRules = state.runRules;
  s.upgradeMenuActive = state.upgradeMenu.active;
  for (int i = 0; i < 3; ++i)
    s.temporaryUpgradeLevels[i] = state.progression.run.temporaryLevels[i];
  s.roomHeat = state.progression.run.roomHeat;
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
    b.ownerPlayerId = static_cast<std::int8_t>(a.networkOwnerPlayerId);
  }
  for (int i = 0; i < CAPTURE_COUNT; ++i)
    s.captures[i] = state.captures[i].filled;
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
  state.time = s.time;
  state.frame = static_cast<int>(s.tick);
  state.roomIndex = s.roomIndex;
  state.roomSeed = s.roomSeed;
  state.requiredSouls = s.requiredSouls;
  state.depositedSouls = s.depositedSouls;
  state.roomClear = s.roomClear;
  state.runRules = s.runRules;
  state.upgradeMenu.active = s.upgradeMenuActive;
  state.uiPaused = s.upgradeMenuActive;
  for (int i = 0; i < 3; ++i)
    state.progression.run.temporaryLevels[i] = s.temporaryUpgradeLevels[i];
  state.progression.run.roomHeat = s.roomHeat;
  for (auto &peer : state.multiplayer.peers)
    peer = NetworkPeerState{};
  for (const auto &p : s.players)
    if (p.active) {
      if (p.id == localPlayerId) {
        state.player.pos = p.pos;
        state.player.vel = p.vel;
        state.player.yaw = p.yaw;
        state.player.battery = p.battery;
        state.player.souls = p.souls;
        state.player.grounded = (p.flags & 1) != 0;
        state.player.alive = (p.flags & 4) != 0;
        state.camera.pitch = p.pitch;
        state.camera.firstPerson = (p.flags & 2) != 0;
        state.vacuum.power = p.vacuumPower;
        state.vacuum.pose = p.vacuumPose;
        state.vacuum.target = p.vacuumTarget;
        state.meleeVisual.visualTimer = p.meleeTimer;
        state.energy.dischargePositionAmount = p.dischargeAmount;
      } else if (p.id < MAX_PLAYERS) {
        auto &peer = state.multiplayer.peers[p.id];
        peer.active = true;
        peer.playerId = p.id;
        peer.player.pos = p.pos;
        peer.player.vel = p.vel;
        peer.player.yaw = p.yaw;
        peer.player.battery = p.battery;
        peer.player.souls = p.souls;
        peer.player.grounded = (p.flags & 1) != 0;
        peer.player.alive = (p.flags & 4) != 0;
        peer.camera.pitch = p.pitch;
        peer.vacuum.power = p.vacuumPower;
        peer.vacuum.pose = p.vacuumPose;
        peer.vacuum.target = p.vacuumTarget;
        peer.meleeVisual.visualTimer = p.meleeTimer;
        peer.energy.dischargePositionAmount = p.dischargeAmount;
        peer.phoneVisual =
            makePhoneVisualState(p.vacuumPose, p.vacuumPower, 0, s.time, false);
        peer.phoneTransform.position = p.pos + Vec3{0, 0.54f, 0};
        peer.phoneTransform.orientation = quatAxisAngle({0, 1, 0}, p.yaw);
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
    b.networkOwnerPlayerId = a.ownerPlayerId;
  }
  const bool initialSnapshot = !state.multiplayer.hasWorldSnapshot;
  for (int i = 0; i < CAPTURE_COUNT; ++i) {
    auto &capture = state.captures[i];
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
