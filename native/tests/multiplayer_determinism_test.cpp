#include "Game.hpp"
#include "MultiplayerConnectionState.hpp"
#include "MultiplayerProtocol.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float FIXED_DT = 1.0f / 60.0f;
constexpr std::uint32_t SCENARIO_TICKS = 300;
constexpr std::uint32_t SETTLE_TICKS = 180;
constexpr std::uint32_t SNAPSHOT_INTERVAL = 3;
constexpr std::uint32_t CHECKPOINT_INTERVAL = 60;
constexpr std::uint32_t EXPLICIT_SEED = 0x51a7c0deu;

enum class Direction { GuestToHost, HostToGuest };

struct NetworkProfile {
  const char* name;
  std::uint32_t latencyTicks;
  std::uint32_t dropEvery;
};

struct Packet {
  Direction direction = Direction::GuestToHost;
  dbnet::MessageType type = dbnet::MessageType::Input;
  std::uint32_t deliverTick = 0;
  std::uint64_t order = 0;
  std::vector<std::uint8_t> bytes;
};

struct DeliverySummary {
  std::array<std::uint32_t, 3> sent{};
  std::array<std::uint32_t, 3> dropped{};
  std::array<std::uint32_t, 3> delivered{};
  std::uint32_t lastInput = 0;
  std::uint32_t lastSnapshot = 0;
  std::uint32_t lastEvent = 0;
  bool operator==(const DeliverySummary& other) const {
    return sent == other.sent && dropped == other.dropped &&
           delivered == other.delivered && lastInput == other.lastInput &&
           lastSnapshot == other.lastSnapshot && lastEvent == other.lastEvent;
  }
};

std::size_t packetIndex(dbnet::MessageType type) {
  if (type == dbnet::MessageType::Input) return 0;
  if (type == dbnet::MessageType::Snapshot) return 1;
  return 2;
}

class DeterministicPacketQueue {
 public:
  DeterministicPacketQueue(NetworkProfile profile, std::uint32_t seed)
      : profile_(profile), seed_(seed) {}

  void send(Direction direction, dbnet::MessageType type, std::uint32_t tick,
            std::vector<std::uint8_t> bytes) {
    const std::size_t index = packetIndex(type);
    const std::uint32_t serial = ++summary_.sent[index];
    const std::uint32_t salt = static_cast<std::uint32_t>(index * 17u) +
                               (direction == Direction::HostToGuest ? 7u : 0u);
    if (profile_.dropEvery != 0 &&
        (serial + seed_ + salt) % profile_.dropEvery == 0) {
      ++summary_.dropped[index];
      return;
    }
    packets_.push_back(
        {direction, type, tick + profile_.latencyTicks, nextOrder_++, std::move(bytes)});
  }

  template <typename Receiver>
  void deliver(std::uint32_t tick, Receiver&& receiver) {
    std::stable_sort(packets_.begin(), packets_.end(),
                     [](const Packet& a, const Packet& b) {
                       if (a.deliverTick != b.deliverTick)
                         return a.deliverTick < b.deliverTick;
                       return a.order < b.order;
                     });
    std::vector<Packet> pending;
    for (auto& packet : packets_) {
      if (packet.deliverTick > tick) {
        pending.push_back(std::move(packet));
        continue;
      }
      ++summary_.delivered[packetIndex(packet.type)];
      receiver(packet);
    }
    packets_ = std::move(pending);
  }

  bool empty() const { return packets_.empty(); }
  const DeliverySummary& summary() const { return summary_; }
  DeliverySummary& summary() { return summary_; }

 private:
  NetworkProfile profile_;
  std::uint32_t seed_ = 0;
  std::uint64_t nextOrder_ = 0;
  std::vector<Packet> packets_;
  DeliverySummary summary_;
};

struct RunResult {
  DeliverySummary delivery;
  std::vector<std::pair<std::uint32_t, dbnet::DurableSectionHashes>> checkpoints;
  dbnet::DurableSectionHashes hostFinal;
  dbnet::DurableSectionHashes guestFinal;
  std::uint64_t hostFinalHash = 0;
  std::uint64_t guestFinalHash = 0;
  bool eventApplied = false;
};

void setSeed(Game& game, std::uint32_t seed) {
  game.networkMutableState().roomSeed = static_cast<int>(seed);
}

void scheduleMovement(Game& guest, std::uint32_t tick) {
  const bool moving = tick >= 20 && tick < 150;
  const bool jump = tick == 55;
  guest.setTouchControls(moving ? 0.35f : 0.0f, moving ? 0.9f : 0.0f,
                         0.0f, 0.0f, false, false, jump, false, false, false);
}

std::string firstDifferentSection(const dbnet::DurableSectionHashes& a,
                                  const dbnet::DurableSectionHashes& b) {
  if (a.world != b.world) return "world";
  if (a.players != b.players) return "players";
  if (a.targets != b.targets) return "targets";
  if (a.projectiles != b.projectiles) return "projectiles";
  if (a.progression != b.progression) return "progression";
  return {};
}

void printHashes(const char* label, const dbnet::DurableSectionHashes& hashes) {
  std::printf("%s world=%llu players=%llu targets=%llu projectiles=%llu progression=%llu\n",
              label, static_cast<unsigned long long>(hashes.world),
              static_cast<unsigned long long>(hashes.players),
              static_cast<unsigned long long>(hashes.targets),
              static_cast<unsigned long long>(hashes.projectiles),
              static_cast<unsigned long long>(hashes.progression));
}

RunResult runScenario(const NetworkProfile& profile) {
  Game host;
  Game guest;
  host.reset();
  guest.reset();
  host.configureNetworkHost();
  host.setNetworkPeerActive(1, true);
  guest.configureNetworkGuest(1);
  setSeed(host, EXPLICIT_SEED);
  setSeed(guest, EXPLICIT_SEED);

  dbnet::NetworkWorldContext world{EXPLICIT_SEED, 1, 1, 1};
  dbnet::GameplayEventTracker eventTracker;
  eventTracker.reset(world);
  DeterministicPacketQueue queue(profile, EXPLICIT_SEED);
  RunResult result;
  std::uint32_t inputSequence = 0;
  std::uint32_t snapshotSequence = 0;

  auto receive = [&](const Packet& packet) {
    dbnet::PacketHeader header;
    if (packet.direction == Direction::GuestToHost) {
      dbnet::NetworkWorldContext inputWorld;
      dbnet::InputCommand input;
      if (dbnet::decodeInput(packet.bytes.data(), packet.bytes.size(), header,
                             inputWorld, input) &&
          inputWorld == world) {
        host.setNetworkPeerCommand(header.playerId, input);
        queue.summary().lastInput = header.sequence;
      }
      return;
    }
    if (packet.type == dbnet::MessageType::Snapshot) {
      dbnet::WorldSnapshot snapshot;
      if (dbnet::decodeSnapshot(packet.bytes.data(), packet.bytes.size(), header,
                                snapshot) &&
          snapshot.world == world) {
        dbnet::applyWorld(guest.networkMutableState(), snapshot, 1);
        queue.summary().lastSnapshot = header.sequence;
      }
      return;
    }
    dbnet::GameplayEvent event;
    if (dbnet::decodeEvent(packet.bytes.data(), packet.bytes.size(), header, event) &&
        eventTracker.accept(event)) {
      result.eventApplied = true;
      queue.summary().lastEvent = event.eventId;
    }
  };

  const std::uint32_t totalTicks = SCENARIO_TICKS + SETTLE_TICKS;
  for (std::uint32_t tick = 1; tick <= totalTicks; ++tick) {
    queue.deliver(tick, receive);
    scheduleMovement(guest, tick);
    const auto command = guest.capturePlayerCommand(++inputSequence, tick);
    queue.send(Direction::GuestToHost, dbnet::MessageType::Input, tick,
               dbnet::encodeInput(1, world, command));
    guest.update(FIXED_DT);
    host.update(FIXED_DT);

    if (tick == 1) {
      dbnet::GameplayEvent event;
      event.world = world;
      event.authoritativeTick = tick;
      event.eventId = 1;
      event.type = dbnet::GameplayEventType::PlayerActionStarted;
      event.sourceEntityId = 1;
      queue.send(Direction::HostToGuest, dbnet::MessageType::Event, tick,
                 dbnet::encodeEvent(0, event));
    }
    if (tick % SNAPSHOT_INTERVAL == 0 || tick == totalTicks) {
      auto snapshot = dbnet::captureWorld(
          host.state(), dbnet::capturePlayers(host.state()), tick);
      snapshot.world = world;
      queue.send(Direction::HostToGuest, dbnet::MessageType::Snapshot, tick,
                 dbnet::encodeSnapshot(0, snapshot, ++snapshotSequence));
    }
    if (tick % CHECKPOINT_INTERVAL == 0) {
      auto checkpoint = dbnet::captureWorld(
          host.state(), dbnet::capturePlayers(host.state()), tick);
      checkpoint.world = world;
      result.checkpoints.push_back({tick, dbnet::durableSectionHashes(checkpoint)});
    }
  }

  for (std::uint32_t tick = totalTicks + 1;
       !queue.empty() && tick <= totalTicks + profile.latencyTicks + 2; ++tick) {
    queue.deliver(tick, receive);
  }

  auto hostFinal = dbnet::captureWorld(
      host.state(), dbnet::capturePlayers(host.state()), totalTicks);
  hostFinal.world = world;
  auto guestFinal = dbnet::captureWorld(
      guest.state(), dbnet::capturePlayers(guest.state()), totalTicks);
  guestFinal.world = world;
  result.delivery = queue.summary();
  result.hostFinal = dbnet::durableSectionHashes(hostFinal);
  result.guestFinal = dbnet::durableSectionHashes(guestFinal);
  result.hostFinalHash = dbnet::authoritativeStateHash(hostFinal);
  result.guestFinalHash = dbnet::authoritativeStateHash(guestFinal);
  return result;
}

bool verifyProfile(const NetworkProfile& profile) {
  const RunResult first = runScenario(profile);
  const RunResult second = runScenario(profile);
  bool ok = first.delivery == second.delivery &&
            first.checkpoints == second.checkpoints &&
            first.hostFinal == second.hostFinal &&
            first.guestFinal == second.guestFinal &&
            first.hostFinalHash == second.hostFinalHash &&
            first.guestFinalHash == second.guestFinalHash &&
            first.hostFinal == first.guestFinal &&
            first.eventApplied && second.eventApplied;
  if (!ok) {
    std::uint32_t divergentTick = 0;
    std::string section;
    const std::size_t count = std::min(first.checkpoints.size(), second.checkpoints.size());
    for (std::size_t i = 0; i < count; ++i) {
      if (first.checkpoints[i] != second.checkpoints[i]) {
        divergentTick = first.checkpoints[i].first;
        section = firstDifferentSection(first.checkpoints[i].second,
                                       second.checkpoints[i].second);
        break;
      }
    }
    if (section.empty()) section = firstDifferentSection(first.hostFinal, first.guestFinal);
    std::fprintf(stderr,
                 "DETERMINISM_FAILURE profile=%s first_divergent_tick=%u section=%s "
                 "last_input=%u last_snapshot=%u last_event=%u\n",
                 profile.name, divergentTick, section.c_str(),
                 first.delivery.lastInput, first.delivery.lastSnapshot,
                 first.delivery.lastEvent);
    printHashes("host", first.hostFinal);
    printHashes("guest", first.guestFinal);
    return false;
  }
  std::printf(
      "MULTIPLAYER_DETERMINISM_OK profile=%s seed=%u inputs=%u/%u snapshots=%u/%u "
      "events=%u/%u host=%llu guest=%llu\n",
      profile.name, EXPLICIT_SEED, first.delivery.delivered[0],
      first.delivery.dropped[0], first.delivery.delivered[1],
      first.delivery.dropped[1], first.delivery.delivered[2],
      first.delivery.dropped[2],
      static_cast<unsigned long long>(first.hostFinalHash),
      static_cast<unsigned long long>(first.guestFinalHash));
  printHashes("sections", first.hostFinal);
  return true;
}

struct MeleeEventSummary {
  std::vector<dbnet::GameplayEventType> acceptedTypes;
  std::uint32_t accepted = 0;
  std::uint32_t duplicateRejected = 0;
  std::uint32_t staleRejected = 0;
  std::uint32_t wrongWorldRejected = 0;
  bool operator==(const MeleeEventSummary& other) const {
    return acceptedTypes == other.acceptedTypes &&
           accepted == other.accepted &&
           duplicateRejected == other.duplicateRejected &&
           staleRejected == other.staleRejected &&
           wrongWorldRejected == other.wrongWorldRejected;
  }
};

struct MeleeRunResult {
  DeliverySummary delivery;
  MeleeEventSummary events;
  std::vector<std::pair<std::uint32_t, dbnet::DurableSectionHashes>> checkpoints;
  dbnet::DurableSectionHashes hostFinal;
  dbnet::DurableSectionHashes guestFinal;
  std::uint64_t hostFinalHash = 0;
  std::uint64_t guestFinalHash = 0;
  float initialArmor = 0.0f;
  float guestArmorAfterPrediction = 0.0f;
  float hostArmorAfterContact = 0.0f;
  bool guestPredictedStartup = false;
  bool hostConfirmedAction = false;
  bool missingEventInjected = false;
};

void configureMeleeFixture(Game& game, bool host) {
  GameState& state = game.networkMutableState();
  for (auto& target : state.targets) target.alive = false;
  state.player.pos = host ? Vec3{8.0f, 0.08f, 8.0f}
                          : Vec3{0.0f, 0.08f, 0.0f};
  if (host) {
    auto& peer = state.multiplayer.peers[1];
    peer.player.pos = {0.0f, 0.08f, 0.0f};
    peer.player.vel = {};
    peer.player.battery = 100.0f;
    peer.player.grounded = true;
    peer.camera.yaw = 0.0f;
  }
  TargetState& enemy = state.targets[0];
  enemy = TargetState{};
  enemy.alive = true;
  enemy.armor = 0.5f;
  enemy.armorRegenDelay = 1000.0f;
  enemy.pos = {0.0f, 0.08f, -0.7f};
  enemy.walkTarget = enemy.pos;
  enemy.attackCooldown = 1000.0f;
}

bool containsMeleeEvents(const MeleeEventSummary& summary) {
  const std::array<dbnet::GameplayEventType, 5> expected{
      dbnet::GameplayEventType::PlayerActionStarted,
      dbnet::GameplayEventType::PlayerActionContact,
      dbnet::GameplayEventType::EnemyHitConfirmed,
      dbnet::GameplayEventType::EnemyShellBroken,
      dbnet::GameplayEventType::SoulEmergenceStarted};
  std::size_t next = 0;
  for (const auto type : summary.acceptedTypes)
    if (next < expected.size() && type == expected[next]) ++next;
  return next == expected.size();
}

MeleeRunResult runMeleeScenario(const NetworkProfile& profile) {
  constexpr std::uint32_t meleeTick = 20;
  constexpr std::uint32_t activeTicks = 180;
  constexpr std::uint32_t settleTicks = 180;
  Game host;
  Game guest;
  host.reset();
  guest.reset();
  host.configureNetworkHost();
  host.setNetworkPeerActive(1, true);
  guest.configureNetworkGuest(1);
  setSeed(host, EXPLICIT_SEED + 1);
  setSeed(guest, EXPLICIT_SEED + 1);
  configureMeleeFixture(host, true);
  configureMeleeFixture(guest, false);

  const dbnet::NetworkWorldContext world{EXPLICIT_SEED + 1, 1, 1, 1};
  dbnet::GameplayEventTracker tracker;
  tracker.reset(world);
  dbnet::GameplayEventTracker lossyPresentationTracker;
  lossyPresentationTracker.reset(world);
  DeterministicPacketQueue queue(profile, EXPLICIT_SEED + 1);
  MeleeRunResult result;
  result.initialArmor = host.state().targets[0].armor;
  std::set<std::uint32_t> seenEventIds;
  std::uint32_t inputSequence = 0;
  std::uint32_t snapshotSequence = 0;
  std::uint32_t nextEventId = 0;
  auto previous = dbnet::captureWorld(
      host.state(), dbnet::capturePlayers(host.state()), 0);
  previous.world = world;

  auto receive = [&](const Packet& packet) {
    dbnet::PacketHeader header;
    if (packet.direction == Direction::GuestToHost) {
      dbnet::NetworkWorldContext inputWorld;
      dbnet::InputCommand input;
      if (dbnet::decodeInput(packet.bytes.data(), packet.bytes.size(), header,
                             inputWorld, input) &&
          inputWorld == world) {
        host.setNetworkPeerCommand(header.playerId, input);
        queue.summary().lastInput = header.sequence;
      }
      return;
    }
    if (packet.type == dbnet::MessageType::Snapshot) {
      dbnet::WorldSnapshot snapshot;
      if (dbnet::decodeSnapshot(packet.bytes.data(), packet.bytes.size(), header,
                                snapshot) &&
          snapshot.world == world) {
        dbnet::applyWorld(guest.networkMutableState(), snapshot, 1);
        queue.summary().lastSnapshot = header.sequence;
      }
      return;
    }
    dbnet::GameplayEvent event;
    if (!dbnet::decodeEvent(packet.bytes.data(), packet.bytes.size(), header, event))
      return;
    const bool duplicate = seenEventIds.count(event.eventId) != 0;
    if (event.world != world) {
      ++result.events.wrongWorldRejected;
    } else if (!tracker.accept(event)) {
      if (duplicate) ++result.events.duplicateRejected;
      else ++result.events.staleRejected;
    } else {
      ++result.events.accepted;
      result.events.acceptedTypes.push_back(event.type);
      queue.summary().lastEvent = event.eventId;
      if (event.type == dbnet::GameplayEventType::EnemyHitConfirmed)
        result.missingEventInjected = true;
      else
        lossyPresentationTracker.accept(event);
    }
    seenEventIds.insert(event.eventId);
  };

  const std::uint32_t totalTicks = activeTicks + settleTicks;
  for (std::uint32_t tick = 1; tick <= totalTicks; ++tick) {
    queue.deliver(tick, receive);
    const bool melee = tick == meleeTick;
    guest.setTouchControls(0.0f, 0.0f, 0.0f, 0.0f,
                           false, false, false, melee, false, false);
    const auto command = guest.capturePlayerCommand(++inputSequence, tick);
    queue.send(Direction::GuestToHost, dbnet::MessageType::Input, tick,
               dbnet::encodeInput(1, world, command));
    guest.update(FIXED_DT);
    if (melee) {
      result.guestPredictedStartup =
          guest.state().meleeVisual.actionSequence != 0 &&
          guest.state().meleeVisual.visualTimer > 0.0f;
      result.guestArmorAfterPrediction = guest.state().targets[0].armor;
    }
    host.update(FIXED_DT);

    auto current = dbnet::captureWorld(
        host.state(), dbnet::capturePlayers(host.state()), tick);
    current.world = world;
    const auto events = dbnet::deriveMeleeEvents(previous, current, nextEventId);
    for (const auto& event : events) {
      const auto packet = dbnet::encodeEvent(0, event);
      queue.send(Direction::HostToGuest, dbnet::MessageType::Event, tick, packet);
      queue.send(Direction::HostToGuest, dbnet::MessageType::Event, tick, packet);
      queue.send(Direction::HostToGuest, dbnet::MessageType::Event, tick, packet);
    }
    previous = current;
    if (current.players[1].actionSequence != 0)
      result.hostConfirmedAction = true;
    if (current.targets[0].armor < result.initialArmor)
      result.hostArmorAfterContact = current.targets[0].armor;

    if (tick == 90) {
      dbnet::GameplayEvent stale;
      stale.world = world;
      stale.authoritativeTick = tick;
      stale.eventId = 0;
      stale.type = dbnet::GameplayEventType::EnemyHitConfirmed;
      auto wrong = stale;
      wrong.eventId = nextEventId + 100;
      ++wrong.world.roomGeneration;
      for (int repeat = 0; repeat < 3; ++repeat) {
        queue.send(Direction::HostToGuest, dbnet::MessageType::Event, tick,
                   dbnet::encodeEvent(0, stale));
        queue.send(Direction::HostToGuest, dbnet::MessageType::Event, tick,
                   dbnet::encodeEvent(0, wrong));
      }
    }
    if (tick % SNAPSHOT_INTERVAL == 0 || tick == totalTicks) {
      queue.send(Direction::HostToGuest, dbnet::MessageType::Snapshot, tick,
                 dbnet::encodeSnapshot(0, current, ++snapshotSequence));
    }
    if (tick % CHECKPOINT_INTERVAL == 0)
      result.checkpoints.push_back(
          {tick, dbnet::durableSectionHashes(current)});
  }

  auto finalSnapshot = dbnet::captureWorld(
      host.state(), dbnet::capturePlayers(host.state()), totalTicks);
  finalSnapshot.world = world;
  for (int repeat = 0; repeat < 5; ++repeat)
    queue.send(Direction::HostToGuest, dbnet::MessageType::Snapshot, totalTicks,
               dbnet::encodeSnapshot(0, finalSnapshot, ++snapshotSequence));
  for (std::uint32_t tick = totalTicks + 1;
       !queue.empty() && tick <= totalTicks + profile.latencyTicks + 2; ++tick)
    queue.deliver(tick, receive);

  auto hostFinal = finalSnapshot;
  auto guestFinal = dbnet::captureWorld(
      guest.state(), dbnet::capturePlayers(guest.state()), totalTicks);
  guestFinal.world = world;
  result.delivery = queue.summary();
  result.hostFinal = dbnet::durableSectionHashes(hostFinal);
  result.guestFinal = dbnet::durableSectionHashes(guestFinal);
  result.hostFinalHash = dbnet::authoritativeStateHash(hostFinal);
  result.guestFinalHash = dbnet::authoritativeStateHash(guestFinal);
  return result;
}

bool verifyMeleeProfile(const NetworkProfile& profile) {
  const auto first = runMeleeScenario(profile);
  const auto second = runMeleeScenario(profile);
  const bool repeated =
      first.delivery == second.delivery &&
      first.events == second.events &&
      first.checkpoints == second.checkpoints &&
      first.hostFinal == second.hostFinal &&
      first.guestFinal == second.guestFinal &&
      first.hostFinalHash == second.hostFinalHash &&
      first.guestFinalHash == second.guestFinalHash;
  const bool authority =
      first.guestPredictedStartup && first.hostConfirmedAction &&
      first.guestArmorAfterPrediction == first.initialArmor &&
      first.hostArmorAfterContact < first.initialArmor;
  const bool eventChecks =
      containsMeleeEvents(first.events) &&
      first.events.duplicateRejected >= 5 &&
      first.events.staleRejected >= 1 &&
      first.events.wrongWorldRejected >= 1 &&
      first.missingEventInjected;
  const bool converged =
      first.hostFinal == first.guestFinal &&
      first.hostFinalHash == first.guestFinalHash;
  if (!(repeated && authority && eventChecks && converged)) {
    const std::string section =
        firstDifferentSection(first.hostFinal, first.guestFinal);
    std::fprintf(
        stderr,
        "MELEE_DETERMINISM_FAILURE profile=%s section=%s last_input=%u "
        "last_snapshot=%u last_event=%u accepted=%u duplicate=%u stale=%u wrong=%u "
        "repeated=%d authority=%d events=%d converged=%d predicted=%d confirmed=%d "
        "armor=%.3f/%.3f/%.3f\n",
        profile.name, section.c_str(), first.delivery.lastInput,
        first.delivery.lastSnapshot, first.delivery.lastEvent,
        first.events.accepted, first.events.duplicateRejected,
        first.events.staleRejected, first.events.wrongWorldRejected,
        repeated ? 1 : 0, authority ? 1 : 0, eventChecks ? 1 : 0,
        converged ? 1 : 0, first.guestPredictedStartup ? 1 : 0,
        first.hostConfirmedAction ? 1 : 0, first.initialArmor,
        first.guestArmorAfterPrediction, first.hostArmorAfterContact);
    printHashes("host", first.hostFinal);
    printHashes("guest", first.guestFinal);
    return false;
  }
  std::printf(
      "MULTIPLAYER_DETERMINISTIC_MELEE_OK profile=%s enemy=0 "
      "action=confirmed damage=host_only final_hash=%llu "
      "accepted=%u duplicate=%u stale=%u wrong_world=%u\n",
      profile.name, static_cast<unsigned long long>(first.hostFinalHash),
      first.events.accepted, first.events.duplicateRejected,
      first.events.staleRejected, first.events.wrongWorldRejected);
  return true;
}

struct VacuumRunResult {
  DeliverySummary delivery;
  MeleeEventSummary events;
  dbnet::DurableSectionHashes hostFinal;
  dbnet::DurableSectionHashes guestFinal;
  std::uint64_t finalHash = 0;
  bool guestVacuumPredicted = false;
  bool guestDidNotCapture = false;
  bool hostCaptured = false;
  bool guestDischargePredicted = false;
  bool hostConsumed = false;
  bool projectileSpawned = false;
  bool projectileTerminal = false;
  bool projectileOwnedByGuest = false;
  bool operator==(const VacuumRunResult& other) const {
    return delivery == other.delivery && events == other.events &&
      hostFinal == other.hostFinal && guestFinal == other.guestFinal &&
      finalHash == other.finalHash &&
      guestVacuumPredicted == other.guestVacuumPredicted &&
      guestDidNotCapture == other.guestDidNotCapture &&
      hostCaptured == other.hostCaptured &&
      guestDischargePredicted == other.guestDischargePredicted &&
      hostConsumed == other.hostConsumed &&
      projectileSpawned == other.projectileSpawned &&
      projectileTerminal == other.projectileTerminal &&
      projectileOwnedByGuest == other.projectileOwnedByGuest;
  }
};

VacuumRunResult runVacuumDischarge(const NetworkProfile& profile) {
  constexpr std::uint32_t vacuumTick = 20;
  constexpr std::uint32_t vacuumReleaseTick = 250;
  constexpr std::uint32_t dischargeTick = 280;
  constexpr std::uint32_t totalTicks = 600;
  Game host;Game guest;host.reset();guest.reset();
  host.configureNetworkHost();host.setNetworkPeerActive(1,true);
  guest.configureNetworkGuest(1);
  setSeed(host,EXPLICIT_SEED+2);setSeed(guest,EXPLICIT_SEED+2);
  for(auto& target:host.networkMutableState().targets)target.alive=false;
  for(auto& target:guest.networkMutableState().targets)target.alive=false;
  auto& peer=host.networkMutableState().multiplayer.peers[1];
  peer.player.pos={0,0.08f,0};peer.player.battery=100;peer.player.grounded=true;
  peer.camera.yaw=0;host.networkMutableState().player.pos={8,0.08f,8};
  for(Game* game:{&host,&guest}){
    auto& state=game->networkMutableState();
    state.player.pos={0,0.08f,0};state.player.battery=100;
    auto& target=state.targets[0];target=TargetState{};target.alive=true;
    target.slurpable=true;target.soulMorph=1.0f;
    target.pos={0,0.55f,-0.72f};target.walkTarget=target.pos;
    target.attackCooldown=1000.0f;
  }
  host.networkMutableState().player.pos={8,0.08f,8};
  const dbnet::NetworkWorldContext world{EXPLICIT_SEED+2,1,1,1};
  DeterministicPacketQueue queue(profile,EXPLICIT_SEED+2);
  dbnet::GameplayEventTracker tracker;tracker.reset(world);
  dbnet::GameplayEventDerivationState derivation;
  VacuumRunResult result;std::set<std::uint32_t> seen;
  std::uint32_t inputSequence=0,snapshotSequence=0;
  auto previous=dbnet::captureWorld(host.state(),dbnet::capturePlayers(host.state()),0);
  previous.world=world;
  auto receive=[&](const Packet& packet){
    dbnet::PacketHeader header;
    if(packet.direction==Direction::GuestToHost){
      dbnet::NetworkWorldContext inputWorld;dbnet::InputCommand input;
      if(dbnet::decodeInput(packet.bytes.data(),packet.bytes.size(),header,inputWorld,input)&&inputWorld==world){
        host.setNetworkPeerCommand(1,input);queue.summary().lastInput=header.sequence;
      }return;
    }
    if(packet.type==dbnet::MessageType::Snapshot){
      dbnet::WorldSnapshot snapshot;
      if(dbnet::decodeSnapshot(packet.bytes.data(),packet.bytes.size(),header,snapshot)&&snapshot.world==world){
        dbnet::applyWorld(guest.networkMutableState(),snapshot,1);
        queue.summary().lastSnapshot=header.sequence;
      }return;
    }
    dbnet::GameplayEvent event;
    if(!dbnet::decodeEvent(packet.bytes.data(),packet.bytes.size(),header,event))return;
    const bool duplicate=seen.count(event.eventId)!=0;
    if(event.world!=world)++result.events.wrongWorldRejected;
    else if(!tracker.accept(event)){
      if(duplicate)++result.events.duplicateRejected;else ++result.events.staleRejected;
    }else{
      ++result.events.accepted;result.events.acceptedTypes.push_back(event.type);
      queue.summary().lastEvent=event.eventId;
      if(event.type==dbnet::GameplayEventType::ProjectileSpawned){
        result.projectileSpawned=true;
        result.projectileOwnedByGuest=event.sourceEntityId==1;
      }
      if(event.type==dbnet::GameplayEventType::ProjectileImpacted||
         event.type==dbnet::GameplayEventType::ProjectileDespawned)
        result.projectileTerminal=true;
    }
    seen.insert(event.eventId);
  };
  for(std::uint32_t tick=1;tick<=totalTicks;++tick){
    queue.deliver(tick,receive);
    const bool vacuum=tick>=vacuumTick&&tick<vacuumReleaseTick;
    const bool shoot=tick==dischargeTick;
    guest.setTouchControls(0,0,0,0,vacuum,false,false,false,shoot,false);
    const auto command=guest.capturePlayerCommand(++inputSequence,tick);
    queue.send(Direction::GuestToHost,dbnet::MessageType::Input,tick,
      dbnet::encodeInput(1,world,command));
    const int guestSoulsBefore=guest.state().player.souls;
    const float guestIngestBefore=guest.state().targets[0].ingestProgress;
    guest.update(FIXED_DT);
    if(tick==vacuumTick){
      result.guestVacuumPredicted=guest.state().vacuum.active&&guest.state().vacuum.pose>0;
      result.guestDidNotCapture=guest.state().player.souls==guestSoulsBefore&&
        guest.state().targets[0].ingestProgress==guestIngestBefore;
    }
    if(shoot)result.guestDischargePredicted=
      guest.state().energy.dischargeTimer>0&&guest.state().bullets[0].alive==false;
    const int hostSoulsBefore=peer.player.souls;
    host.update(FIXED_DT);
    if(peer.player.souls>0)result.hostCaptured=true;
    if(hostSoulsBefore>peer.player.souls)result.hostConsumed=true;
    auto current=dbnet::captureWorld(host.state(),dbnet::capturePlayers(host.state()),tick);
    current.world=world;
    for(const auto& event:dbnet::deriveGameplayEvents(previous,current,derivation)){
      const auto bytes=dbnet::encodeEvent(0,event);
      queue.send(Direction::HostToGuest,dbnet::MessageType::Event,tick,bytes);
      queue.send(Direction::HostToGuest,dbnet::MessageType::Event,tick,bytes);
      queue.send(Direction::HostToGuest,dbnet::MessageType::Event,tick,bytes);
    }
    previous=current;
    if(tick==400){
      dbnet::GameplayEvent stale;stale.world=world;stale.authoritativeTick=tick;
      stale.eventId=0;stale.type=dbnet::GameplayEventType::VacuumStarted;
      auto wrong=stale;wrong.eventId=derivation.nextEventId+100;
      ++wrong.world.runGeneration;
      for(int repeat=0;repeat<3;++repeat){
        queue.send(Direction::HostToGuest,dbnet::MessageType::Event,tick,
          dbnet::encodeEvent(0,stale));
        queue.send(Direction::HostToGuest,dbnet::MessageType::Event,tick,
          dbnet::encodeEvent(0,wrong));
      }
    }
    if(tick%SNAPSHOT_INTERVAL==0)
      queue.send(Direction::HostToGuest,dbnet::MessageType::Snapshot,tick,
        dbnet::encodeSnapshot(0,current,++snapshotSequence));
  }
  auto finalSnapshot=previous;
  for(int repeat=0;repeat<5;++repeat)
    queue.send(Direction::HostToGuest,dbnet::MessageType::Snapshot,totalTicks,
      dbnet::encodeSnapshot(0,finalSnapshot,++snapshotSequence));
  for(std::uint32_t tick=totalTicks+1;
      !queue.empty()&&tick<=totalTicks+profile.latencyTicks+2;++tick)queue.deliver(tick,receive);
  auto guestFinal=dbnet::captureWorld(guest.state(),dbnet::capturePlayers(guest.state()),totalTicks);
  guestFinal.world=world;result.delivery=queue.summary();
  result.hostFinal=dbnet::durableSectionHashes(finalSnapshot);
  result.guestFinal=dbnet::durableSectionHashes(guestFinal);
  result.finalHash=dbnet::authoritativeStateHash(finalSnapshot);
  return result;
}

bool hasEvent(const MeleeEventSummary& events,dbnet::GameplayEventType type){
  return std::find(events.acceptedTypes.begin(),events.acceptedTypes.end(),type)!=
    events.acceptedTypes.end();
}

bool verifyVacuumDischarge(const NetworkProfile& profile){
  const auto first=runVacuumDischarge(profile);
  const auto second=runVacuumDischarge(profile);
  const bool vacuum=first.guestVacuumPredicted&&first.guestDidNotCapture&&
    first.hostCaptured&&hasEvent(first.events,dbnet::GameplayEventType::VacuumStarted)&&
    hasEvent(first.events,dbnet::GameplayEventType::SoulAttractionStarted)&&
    hasEvent(first.events,dbnet::GameplayEventType::SoulLatched)&&
    hasEvent(first.events,dbnet::GameplayEventType::SoulIngestionStarted)&&
    hasEvent(first.events,dbnet::GameplayEventType::SoulCaptureCompleted);
  const bool discharge=first.guestDischargePredicted&&first.hostConsumed&&
    first.projectileSpawned&&first.projectileOwnedByGuest&&!first.projectileTerminal&&
    hasEvent(first.events,dbnet::GameplayEventType::DischargeStarted)&&
    first.events.duplicateRejected>0&&first.events.staleRejected>0&&
    first.events.wrongWorldRejected>0;
  const bool converged=first.hostFinal==first.guestFinal;
  if(!(first==second&&vacuum&&discharge&&converged)){
    std::fprintf(stderr,"VACUUM_DISCHARGE_FAILURE profile=%s vacuum=%d discharge=%d convergence=%d "
      "events=%u duplicate=%u last_input=%u last_snapshot=%u last_event=%u\n",
      profile.name,vacuum?1:0,discharge?1:0,converged?1:0,first.events.accepted,
      first.events.duplicateRejected,first.delivery.lastInput,
      first.delivery.lastSnapshot,first.delivery.lastEvent);
    printHashes("host",first.hostFinal);printHashes("guest",first.guestFinal);return false;
  }
  std::printf("MULTIPLAYER_DETERMINISTIC_VACUUM_OK profile=%s target=0 final_hash=%llu "
    "input=%u/%u snapshot=%u/%u event=%u/%u accepted=%u duplicate=%u stale=%u wrong=%u\n",
    profile.name,static_cast<unsigned long long>(first.finalHash),
    first.delivery.delivered[0],first.delivery.dropped[0],
    first.delivery.delivered[1],first.delivery.dropped[1],
    first.delivery.delivered[2],first.delivery.dropped[2],
    first.events.accepted,first.events.duplicateRejected,
    first.events.staleRejected,first.events.wrongWorldRejected);
  std::printf("MULTIPLAYER_DETERMINISTIC_DISCHARGE_OK profile=%s projectile=0 owner=1 final_hash=%llu\n",
    profile.name,static_cast<unsigned long long>(first.finalHash));
  return true;
}

struct RoomRunResult {
  DeliverySummary delivery;
  std::vector<std::pair<std::uint32_t,dbnet::DurableSectionHashes>> checkpoints;
  dbnet::DurableSectionHashes hostFinal;
  dbnet::DurableSectionHashes guestFinal;
  std::uint64_t finalHash=0;
  std::uint64_t initialWorldHash=0;
  std::uint64_t nextWorldHash=0;
  std::uint32_t transitionTick=0;
  dbnet::NetworkWorldContext oldWorld;
  dbnet::NetworkWorldContext newWorld;
  int nextRoomSeed=0;
  std::uint32_t staleSnapshots=0;
  std::uint32_t staleEvents=0;
  std::uint32_t staleInputs=0;
  std::uint32_t acceptedNewEvents=0;
  bool initialConverged=false;
  bool hostOnlyCompletion=false;
  bool doorObserved=false;
  bool resetInvariants=false;
  bool operator==(const RoomRunResult& other) const {
    return delivery==other.delivery&&checkpoints==other.checkpoints&&
      hostFinal==other.hostFinal&&guestFinal==other.guestFinal&&
      finalHash==other.finalHash&&initialWorldHash==other.initialWorldHash&&
      nextWorldHash==other.nextWorldHash&&transitionTick==other.transitionTick&&
      oldWorld==other.oldWorld&&newWorld==other.newWorld&&
      nextRoomSeed==other.nextRoomSeed&&
      staleSnapshots==other.staleSnapshots&&staleEvents==other.staleEvents&&
      staleInputs==other.staleInputs&&acceptedNewEvents==other.acceptedNewEvents&&
      initialConverged==other.initialConverged&&
      hostOnlyCompletion==other.hostOnlyCompletion&&
      doorObserved==other.doorObserved&&resetInvariants==other.resetInvariants;
  }
};

RoomRunResult runRoomRollover(const NetworkProfile& profile,std::uint32_t seed){
  constexpr std::uint32_t goalTick=20;
  constexpr std::uint32_t totalTicks=240;
  Game host;Game guest;host.reset();guest.reset();
  host.configureNetworkHost();host.setNetworkPeerActive(1,true);
  guest.configureNetworkGuest(1);
  auto& hostState=host.networkMutableState();
  hostState.roomIndex=1;hostState.roomSeed=static_cast<int>(seed);
  hostState.requiredSouls=1;hostState.depositedSouls=0;hostState.roomClear=false;
  for(auto& capture:hostState.captures)capture=CapturePointState{};
  hostState.captures[0].pos={0,3.05f,-19.43f};
  hostState.player.pos={0,0.08f,-19.8f};hostState.player.grounded=true;
  hostState.multiplayer.peers[1].player.pos={0.8f,0.08f,-18.5f};
  hostState.targets[TARGET_COUNT-1].pos={7.25f,0.08f,6.5f};
  hostState.targets[TARGET_COUNT-1].alive=true;
  hostState.bullets[BULLET_COUNT-1].alive=true;
  hostState.bullets[BULLET_COUNT-1].pos={8,10,8};
  hostState.bullets[BULLET_COUNT-1].life=100;
  hostState.flowers[0].active=true;hostState.flowers[0].pos={6,0.38f,6};
  dbnet::NetworkWorldContext hostWorld{seed,1,1,1};
  dbnet::NetworkWorldContext guestWorld=hostWorld;
  auto initial=dbnet::captureWorld(host.state(),dbnet::capturePlayers(host.state()),0);
  initial.world=hostWorld;
  dbnet::applyWorld(guest.networkMutableState(),initial,1);
  guest.networkMutableState().multiplayer.localPredictionCorrection={4,0,4};

  RoomRunResult result;result.oldWorld=hostWorld;
  result.initialWorldHash=dbnet::durableSectionHashes(initial).world;
  auto initialGuest=dbnet::captureWorld(
    guest.state(),dbnet::capturePlayers(guest.state()),0);
  initialGuest.world=guestWorld;
  result.initialConverged=
    dbnet::durableSectionHashes(initial)==dbnet::durableSectionHashes(initialGuest);

  DeterministicPacketQueue queue(profile,seed);
  dbnet::GameplayEventTracker tracker;tracker.reset(guestWorld);
  dbnet::SnapshotInterpolator interpolator;
  std::uint32_t inputSequence=0,snapshotSequence=0;
  std::vector<std::uint8_t> retainedSnapshot;
  std::vector<std::uint8_t> retainedInput;
  std::vector<std::uint8_t> retainedEvent;
  bool injected=false;

  auto receive=[&](const Packet& packet){
    dbnet::PacketHeader header;
    if(packet.direction==Direction::GuestToHost){
      dbnet::NetworkWorldContext packetWorld;dbnet::InputCommand input;
      if(!dbnet::decodeInput(packet.bytes.data(),packet.bytes.size(),header,
                             packetWorld,input))return;
      if(dbnet::compareWorldContext(packetWorld,hostWorld)!=
         dbnet::WorldContextCompatibility::Compatible){
        ++result.staleInputs;return;
      }
      host.setNetworkPeerCommand(header.playerId,input);
      queue.summary().lastInput=header.sequence;return;
    }
    if(packet.type==dbnet::MessageType::Snapshot){
      dbnet::WorldSnapshot snapshot;
      if(!dbnet::decodeSnapshot(packet.bytes.data(),packet.bytes.size(),header,
                                snapshot))return;
      const auto compatibility=dbnet::compareWorldContext(snapshot.world,guestWorld);
      if(compatibility==dbnet::WorldContextCompatibility::Older){
        ++result.staleSnapshots;return;
      }
      if(compatibility==dbnet::WorldContextCompatibility::NewerRoom){
        guestWorld=snapshot.world;tracker.reset(guestWorld);interpolator.reset();
      }else if(compatibility!=dbnet::WorldContextCompatibility::Compatible){
        ++result.staleSnapshots;return;
      }
      interpolator.push(snapshot,static_cast<std::int64_t>(snapshot.tick)*17);
      dbnet::applyWorld(guest.networkMutableState(),snapshot,1);
      queue.summary().lastSnapshot=header.sequence;return;
    }
    dbnet::GameplayEvent event;
    if(!dbnet::decodeEvent(packet.bytes.data(),packet.bytes.size(),header,event))return;
    if(event.world!=guestWorld){++result.staleEvents;return;}
    if(tracker.accept(event)){
      ++result.acceptedNewEvents;queue.summary().lastEvent=event.eventId;
    }else ++result.staleEvents;
  };

  for(std::uint32_t tick=1;tick<=totalTicks;++tick){
    queue.deliver(tick,receive);
    guest.setTouchControls(0,0,0,0,false,false,false,false,false,false);
    const auto command=guest.capturePlayerCommand(++inputSequence,tick);
    auto inputBytes=dbnet::encodeInput(1,guestWorld,command);
    if(tick==3)retainedInput=inputBytes;
    queue.send(Direction::GuestToHost,dbnet::MessageType::Input,tick,
               std::move(inputBytes));
    guest.update(FIXED_DT);

    if(tick==goalTick){
      hostState.captures[0].filled=true;
      result.hostOnlyCompletion=!guest.state().captures[0].filled&&
        guest.state().depositedSouls==0;
    }
    const bool crossing=result.transitionTick==0&&tick>goalTick+1;
    host.setTouchControls(0,crossing?1.0f:0.0f,0,0,
                          false,false,false,false,false,false);
    host.update(FIXED_DT);
    result.doorObserved|=host.state().roomClear;
    if(result.transitionTick==0&&host.state().roomIndex!=hostWorld.roomIndex){
      result.transitionTick=tick;
      ++hostWorld.roomGeneration;
      hostWorld.roomIndex=static_cast<std::uint16_t>(host.state().roomIndex);
      result.newWorld=hostWorld;result.nextRoomSeed=host.state().roomSeed;
    }

    auto snapshot=dbnet::captureWorld(
      host.state(),dbnet::capturePlayers(host.state()),tick);
    snapshot.world=hostWorld;
    if(tick==3){
      retainedSnapshot=dbnet::encodeSnapshot(0,snapshot,9001);
      dbnet::GameplayEvent oldEvent;oldEvent.world=result.oldWorld;
      oldEvent.authoritativeTick=tick;oldEvent.eventId=1;
      oldEvent.type=dbnet::GameplayEventType::PlayerActionStarted;
      retainedEvent=dbnet::encodeEvent(0,oldEvent);
    }
    if(tick%SNAPSHOT_INTERVAL==0)
      queue.send(Direction::HostToGuest,dbnet::MessageType::Snapshot,tick,
        dbnet::encodeSnapshot(0,snapshot,++snapshotSequence));
    if(result.transitionTick!=0&&tick%CHECKPOINT_INTERVAL==0)
      result.checkpoints.push_back(
        {tick,dbnet::durableSectionHashes(snapshot)});

    if(!injected&&guestWorld==hostWorld&&hostWorld!=result.oldWorld){
      injected=true;
      receive({Direction::HostToGuest,dbnet::MessageType::Snapshot,tick,0,
               retainedSnapshot});
      receive({Direction::HostToGuest,dbnet::MessageType::Event,tick,1,
               retainedEvent});
      receive({Direction::HostToGuest,dbnet::MessageType::Event,tick,2,
               retainedEvent});
      receive({Direction::GuestToHost,dbnet::MessageType::Input,tick,3,
               retainedInput});
      dbnet::GameplayEvent valid;valid.world=hostWorld;valid.authoritativeTick=tick;
      valid.eventId=1;valid.type=dbnet::GameplayEventType::PlayerActionStarted;
      for(int repeat=0;repeat<3;++repeat)
        queue.send(Direction::HostToGuest,dbnet::MessageType::Event,tick,
                   dbnet::encodeEvent(0,valid));
      ++valid.eventId;
      for(int repeat=0;repeat<3;++repeat)
        queue.send(Direction::HostToGuest,dbnet::MessageType::Event,tick,
                   dbnet::encodeEvent(0,valid));
      auto wrongRun=valid;++wrongRun.eventId;++wrongRun.world.runGeneration;
      for(int repeat=0;repeat<3;++repeat)
        queue.send(Direction::HostToGuest,dbnet::MessageType::Event,tick,
                   dbnet::encodeEvent(0,wrongRun));
    }
  }
  auto finalSnapshot=dbnet::captureWorld(
    host.state(),dbnet::capturePlayers(host.state()),totalTicks);
  finalSnapshot.world=hostWorld;
  for(int repeat=0;repeat<5;++repeat)
    queue.send(Direction::HostToGuest,dbnet::MessageType::Snapshot,totalTicks,
      dbnet::encodeSnapshot(0,finalSnapshot,++snapshotSequence));
  for(std::uint32_t tick=totalTicks+1;
      !queue.empty()&&tick<=totalTicks+profile.latencyTicks+2;++tick)
    queue.deliver(tick,receive);
  auto guestFinal=dbnet::captureWorld(
    guest.state(),dbnet::capturePlayers(guest.state()),totalTicks);
  guestFinal.world=guestWorld;
  const auto& guestState=guest.state();
  const Vec3 oldTarget={7.25f,0.08f,6.5f};
  result.resetInvariants=result.transitionTick!=0&&
    guestWorld==hostWorld&&!guestState.flowers[0].active&&
    !guestState.bullets[BULLET_COUNT-1].alive&&
    length(guestState.targets[TARGET_COUNT-1].pos-oldTarget)>0.01f&&
    guestState.vacuum.target==-1&&!guestState.vacuum.active&&
    guestState.meleeVisual.visualTimer==0.0f&&
    guestState.energy.dischargeTimer==0.0f&&
    length(guestState.multiplayer.localPredictionCorrection)<0.0001f;
  result.delivery=queue.summary();
  result.hostFinal=dbnet::durableSectionHashes(finalSnapshot);
  result.guestFinal=dbnet::durableSectionHashes(guestFinal);
  result.finalHash=dbnet::authoritativeStateHash(finalSnapshot);
  result.nextWorldHash=result.hostFinal.world;
  return result;
}

bool verifyRoomRollover(const NetworkProfile& profile){
  const auto first=runRoomRollover(profile,EXPLICIT_SEED+3);
  const auto second=runRoomRollover(profile,EXPLICIT_SEED+3);
  bool seedChangesWorld=true;
  if(std::string(profile.name)=="baseline"){
    const auto alternate=runRoomRollover(profile,EXPLICIT_SEED+4);
    seedChangesWorld=alternate.nextWorldHash!=first.nextWorldHash&&
      alternate.hostFinal==alternate.guestFinal;
  }
  const bool stale=first.staleSnapshots>=1&&first.staleEvents>=3&&
    first.staleInputs>=1&&first.acceptedNewEvents==2;
  const bool converged=first.hostFinal==first.guestFinal;
  if(!(first==second&&first.initialConverged&&first.hostOnlyCompletion&&
       first.doorObserved&&first.resetInvariants&&stale&&converged&&
       seedChangesWorld)){
    std::fprintf(stderr,"ROOM_ROLLOVER_FAILURE profile=%s transition=%u "
      "authority=%d reset=%d stale=%d convergence=%d accepted=%u "
      "snapshot_reject=%u event_reject=%u input_reject=%u\n",
      profile.name,first.transitionTick,first.hostOnlyCompletion?1:0,
      first.resetInvariants?1:0,stale?1:0,converged?1:0,
      first.acceptedNewEvents,first.staleSnapshots,first.staleEvents,
      first.staleInputs);
    printHashes("host",first.hostFinal);printHashes("guest",first.guestFinal);
    return false;
  }
  std::printf("MULTIPLAYER_DETERMINISTIC_ROOM_OK profile=%s old_room=%u "
    "new_room=%u transition_tick=%u seed=%d world_hash=%llu final_hash=%llu "
    "input=%u/%u snapshot=%u/%u event=%u/%u\n",
    profile.name,first.oldWorld.roomIndex,first.newWorld.roomIndex,
    first.transitionTick,first.nextRoomSeed,
    static_cast<unsigned long long>(first.nextWorldHash),
    static_cast<unsigned long long>(first.finalHash),
    first.delivery.delivered[0],first.delivery.dropped[0],
    first.delivery.delivered[1],first.delivery.dropped[1],
    first.delivery.delivered[2],first.delivery.dropped[2]);
  std::printf("MULTIPLAYER_STALE_WORLD_REJECT_OK profile=%s snapshots=%u "
    "events=%u inputs=%u\n",profile.name,first.staleSnapshots,
    first.staleEvents,first.staleInputs);
  printHashes("room_sections",first.hostFinal);
  return true;
}

struct LifecycleRunResult {
  DeliverySummary delivery;
  MeleeEventSummary events;
  std::vector<std::pair<std::uint32_t,dbnet::DurableSectionHashes>> checkpoints;
  dbnet::DurableSectionHashes hostFinal;
  dbnet::DurableSectionHashes guestFinal;
  std::uint64_t finalHash=0;
  std::uint32_t damageTick=0;
  std::uint32_t downTick=0;
  std::uint32_t reviveTick=0;
  std::uint32_t deathTick=0;
  std::uint32_t restartTick=0;
  dbnet::NetworkWorldContext oldWorld;
  dbnet::NetworkWorldContext newWorld;
  std::uint32_t staleSnapshots=0;
  std::uint32_t staleEvents=0;
  std::uint32_t staleInputs=0;
  bool hostOnlyDamage=false;
  bool soulsPreserved=true;
  bool reviveInterrupted=false;
  bool deadMovementBlocked=false;
  bool guestRestartRejected=false;
  bool resetInvariants=false;
  bool operator==(const LifecycleRunResult& other) const {
    return delivery==other.delivery&&events==other.events&&
      checkpoints==other.checkpoints&&hostFinal==other.hostFinal&&
      guestFinal==other.guestFinal&&finalHash==other.finalHash&&
      damageTick==other.damageTick&&downTick==other.downTick&&
      reviveTick==other.reviveTick&&deathTick==other.deathTick&&
      restartTick==other.restartTick&&oldWorld==other.oldWorld&&
      newWorld==other.newWorld&&staleSnapshots==other.staleSnapshots&&
      staleEvents==other.staleEvents&&staleInputs==other.staleInputs&&
      hostOnlyDamage==other.hostOnlyDamage&&
      soulsPreserved==other.soulsPreserved&&
      reviveInterrupted==other.reviveInterrupted&&
      deadMovementBlocked==other.deadMovementBlocked&&
      guestRestartRejected==other.guestRestartRejected&&
      resetInvariants==other.resetInvariants;
  }
};

void armLifecycleEnemy(GameState& state){
  auto& target=state.targets[0];
  target=TargetState{};
  target.alive=true;
  target.pos={0,0.08f,-1.0f};
  target.walkTarget=target.pos;
  target.attackCooldown=0.0f;
  state.enemyAttackOwner=-1;
  state.enemyAttackCadence=0.0f;
}

LifecycleRunResult runLifecycle(const NetworkProfile& profile){
  constexpr std::uint32_t totalTicks=1700;
  constexpr std::uint32_t explicitRestartTick=1450;
  const std::uint32_t seed=EXPLICIT_SEED+5;
  Game host;Game guest;host.reset();guest.reset();
  host.configureNetworkHost();host.setNetworkPeerActive(1,true);
  guest.configureNetworkGuest(1);
  auto& hostState=host.networkMutableState();
  hostState.roomSeed=static_cast<int>(seed);
  hostState.progression.run.batteryRegenLock=100.0f;
  hostState.player.pos={0,0.08f,1.0f};
  hostState.camera.yaw=0.0f;
  auto& peer=hostState.multiplayer.peers[1];
  peer.player.pos={0,0.08f,0};peer.player.grounded=true;
  peer.player.battery=80.0f;peer.player.souls=2;
  peer.player.storedSoulBrute[0]=false;peer.player.storedSoulBrute[1]=true;
  for(auto& target:hostState.targets)target=TargetState{};
  armLifecycleEnemy(hostState);

  dbnet::NetworkWorldContext hostWorld{seed,1,1,1};
  dbnet::NetworkWorldContext guestWorld=hostWorld;
  auto initial=dbnet::captureWorld(host.state(),dbnet::capturePlayers(host.state()),0);
  initial.world=hostWorld;
  dbnet::applyWorld(guest.networkMutableState(),initial,1);
  LifecycleRunResult result;result.oldWorld=hostWorld;
  DeterministicPacketQueue queue(profile,seed);
  dbnet::GameplayEventTracker tracker;tracker.reset(guestWorld);
  dbnet::GameplayEventDerivationState derivation;
  dbnet::SnapshotInterpolator interpolator;
  std::set<std::uint32_t> seen;
  std::uint32_t inputSequence=0,snapshotSequence=0;
  auto previous=initial;
  std::vector<std::uint8_t> retainedSnapshot;
  std::vector<std::uint8_t> retainedInput;
  std::vector<std::uint8_t> retainedDeathEvent;
  std::vector<std::uint8_t> retainedReviveEvent;
  bool secondDamageArmed=false,finalDamageArmed=false,injected=false;
  float interruptedCharge=0.0f;
  Vec3 deadPosition;

  auto receive=[&](const Packet& packet){
    dbnet::PacketHeader header;
    if(packet.direction==Direction::GuestToHost){
      dbnet::NetworkWorldContext packetWorld;dbnet::InputCommand input;
      if(!dbnet::decodeInput(packet.bytes.data(),packet.bytes.size(),header,
                             packetWorld,input))return;
      if(dbnet::compareWorldContext(packetWorld,hostWorld)!=
         dbnet::WorldContextCompatibility::Compatible){
        ++result.staleInputs;return;
      }
      host.setNetworkPeerCommand(header.playerId,input);
      queue.summary().lastInput=header.sequence;return;
    }
    if(packet.type==dbnet::MessageType::Snapshot){
      dbnet::WorldSnapshot snapshot;
      if(!dbnet::decodeSnapshot(packet.bytes.data(),packet.bytes.size(),header,
                                snapshot))return;
      const auto compatibility=dbnet::compareWorldContext(snapshot.world,guestWorld);
      if(compatibility==dbnet::WorldContextCompatibility::Older){
        ++result.staleSnapshots;return;
      }
      if(compatibility==dbnet::WorldContextCompatibility::NewerRun||
         compatibility==dbnet::WorldContextCompatibility::NewerRoom){
        guestWorld=snapshot.world;tracker.reset(guestWorld);interpolator.reset();
      }else if(compatibility!=dbnet::WorldContextCompatibility::Compatible){
        ++result.staleSnapshots;return;
      }
      interpolator.push(snapshot,static_cast<std::int64_t>(snapshot.tick)*17);
      dbnet::applyWorld(guest.networkMutableState(),snapshot,1);
      queue.summary().lastSnapshot=header.sequence;return;
    }
    dbnet::GameplayEvent event;
    if(!dbnet::decodeEvent(packet.bytes.data(),packet.bytes.size(),header,event))return;
    if(event.world!=guestWorld){++result.staleEvents;return;}
    const bool duplicate=seen.count(event.eventId)!=0;
    if(!tracker.accept(event)){
      if(duplicate)++result.events.duplicateRejected;
      else ++result.events.staleRejected;
    }else{
      ++result.events.accepted;result.events.acceptedTypes.push_back(event.type);
      queue.summary().lastEvent=event.eventId;
    }
    seen.insert(event.eventId);
  };

  for(std::uint32_t tick=1;tick<=totalTicks;++tick){
    queue.deliver(tick,receive);
    const bool dead=result.deathTick!=0&&tick<explicitRestartTick;
    guest.setTouchControls(0,dead?1.0f:0.0f,0,0,
                           false,false,false,false,false,false);
    const auto command=guest.capturePlayerCommand(++inputSequence,tick);
    auto inputBytes=dbnet::encodeInput(1,guestWorld,command);
    if(tick==10)retainedInput=inputBytes;
    queue.send(Direction::GuestToHost,dbnet::MessageType::Input,tick,
               std::move(inputBytes));
    guest.update(FIXED_DT);

    if(result.damageTick!=0&&!secondDamageArmed&&tick>=result.damageTick+30){
      peer.player.battery=25.0f;armLifecycleEnemy(hostState);
      secondDamageArmed=true;
    }
    bool reviveHeld=false;
    if(result.downTick!=0&&result.reviveTick==0){
      const auto elapsed=tick-result.downTick;
      reviveHeld=(elapsed>=10&&elapsed<40)||(elapsed>=60);
      if(elapsed==40)interruptedCharge=peer.player.reviveCharge;
      if(elapsed==59)result.reviveInterrupted=
        interruptedCharge>0.0f&&peer.player.reviveCharge==interruptedCharge;
    }
    if(result.reviveTick!=0&&!finalDamageArmed&&tick>=result.reviveTick+30){
      peer.player.battery=25.0f;armLifecycleEnemy(hostState);
      finalDamageArmed=true;
    }
    host.setTouchControls(0,0,0,0,reviveHeld,false,false,false,false,false);

    if(tick==explicitRestartTick){
      const auto guestBefore=dbnet::durableSectionHashes(dbnet::captureWorld(
        guest.state(),dbnet::capturePlayers(guest.state()),tick));
      guest.restart();
      const auto guestAfter=dbnet::durableSectionHashes(dbnet::captureWorld(
        guest.state(),dbnet::capturePlayers(guest.state()),tick));
      result.guestRestartRejected=guestBefore==guestAfter;
      host.restart();
      ++hostWorld.runGeneration;hostWorld.roomGeneration=1;
      hostWorld.roomIndex=static_cast<std::uint16_t>(host.state().roomIndex);
      result.newWorld=hostWorld;result.restartTick=tick;
      derivation={};
    }
    const float guestBatteryBefore=guest.state().player.battery;
    const int guestSoulsBefore=guest.state().player.souls;
    host.update(FIXED_DT);

    if(result.damageTick==0&&peer.player.battery<80.0f){
      result.damageTick=tick;
      result.hostOnlyDamage=guest.state().player.battery==guestBatteryBefore;
      result.soulsPreserved=peer.player.souls==2&&
        guest.state().player.souls==guestSoulsBefore;
      hostState.targets[0]=TargetState{};
    }
    if(result.downTick==0&&peer.player.downed){
      result.downTick=tick;hostState.targets[0]=TargetState{};
    }
    if(result.downTick!=0&&result.reviveTick==0&&!peer.player.downed&&
       peer.player.alive&&peer.player.battery>0.0f){
      result.reviveTick=tick;
    }
    if(result.reviveTick!=0&&result.deathTick==0&&!peer.player.alive){
      result.deathTick=tick;deadPosition=peer.player.pos;
      result.deadMovementBlocked=true;
    }
    if(result.deathTick!=0&&tick>result.deathTick&&tick<explicitRestartTick)
      result.deadMovementBlocked&=length(peer.player.pos-deadPosition)<0.0001f;

    auto current=dbnet::captureWorld(
      host.state(),dbnet::capturePlayers(host.state()),tick);
    current.world=hostWorld;
    if(tick==explicitRestartTick){
      previous=current;
    }else{
      for(const auto& event:dbnet::deriveGameplayEvents(previous,current,derivation)){
        const auto bytes=dbnet::encodeEvent(0,event);
        if(event.type==dbnet::GameplayEventType::PlayerDied)
          retainedDeathEvent=bytes;
        if(event.type==dbnet::GameplayEventType::PlayerRevived)
          retainedReviveEvent=bytes;
        for(int repeat=0;repeat<3;++repeat)
          queue.send(Direction::HostToGuest,dbnet::MessageType::Event,tick,bytes);
      }
      previous=current;
    }
    if(tick==explicitRestartTick-1)
      retainedSnapshot=dbnet::encodeSnapshot(0,current,9002);
    if(tick%SNAPSHOT_INTERVAL==0)
      queue.send(Direction::HostToGuest,dbnet::MessageType::Snapshot,tick,
        dbnet::encodeSnapshot(0,current,++snapshotSequence));
    if(tick%CHECKPOINT_INTERVAL==0)
      result.checkpoints.push_back({tick,dbnet::durableSectionHashes(current)});

    if(!injected&&result.restartTick!=0&&guestWorld==hostWorld){
      injected=true;
      receive({Direction::HostToGuest,dbnet::MessageType::Snapshot,tick,0,
               retainedSnapshot});
      receive({Direction::GuestToHost,dbnet::MessageType::Input,tick,1,
               retainedInput});
      for(const auto* bytes:{&retainedDeathEvent,&retainedDeathEvent,
                             &retainedReviveEvent})
        if(!bytes->empty())receive(
          {Direction::HostToGuest,dbnet::MessageType::Event,tick,2,*bytes});
      dbnet::GameplayEvent valid;valid.world=hostWorld;valid.authoritativeTick=tick;
      valid.eventId=1;valid.type=dbnet::GameplayEventType::PlayerActionStarted;
      for(int repeat=0;repeat<3;++repeat)
        queue.send(Direction::HostToGuest,dbnet::MessageType::Event,tick,
                   dbnet::encodeEvent(0,valid));
    }
  }
  auto finalSnapshot=dbnet::captureWorld(
    host.state(),dbnet::capturePlayers(host.state()),totalTicks);
  finalSnapshot.world=hostWorld;
  for(int repeat=0;repeat<5;++repeat)
    queue.send(Direction::HostToGuest,dbnet::MessageType::Snapshot,totalTicks,
      dbnet::encodeSnapshot(0,finalSnapshot,++snapshotSequence));
  for(std::uint32_t tick=totalTicks+1;
      !queue.empty()&&tick<=totalTicks+profile.latencyTicks+2;++tick)
    queue.deliver(tick,receive);
  auto guestFinal=dbnet::captureWorld(
    guest.state(),dbnet::capturePlayers(guest.state()),totalTicks);
  guestFinal.world=guestWorld;
  const auto& hs=host.state();const auto& gs=guest.state();
  result.resetInvariants=result.restartTick==explicitRestartTick&&
    hostWorld.runGeneration==2&&guestWorld==hostWorld&&
    hs.multiplayer.authoritativeHost&&hs.multiplayer.peers[1].active&&
    hs.player.alive&&hs.multiplayer.peers[1].player.alive&&
    hs.player.souls==0&&hs.multiplayer.peers[1].player.souls==0&&
    hs.player.battery==100.0f&&
    !hs.vacuum.active&&hs.meleeVisual.visualTimer==0.0f&&
    hs.energy.dischargeTimer==0.0f&&!hs.bullets[0].alive&&
    !hs.flowers[0].active&&!hs.captures[0].filled&&
    length(gs.multiplayer.localPredictionCorrection)<0.0001f;
  result.delivery=queue.summary();
  result.hostFinal=dbnet::durableSectionHashes(finalSnapshot);
  result.guestFinal=dbnet::durableSectionHashes(guestFinal);
  result.finalHash=dbnet::authoritativeStateHash(finalSnapshot);
  return result;
}

bool verifyLifecycle(const NetworkProfile& profile){
  const auto first=runLifecycle(profile);
  const auto second=runLifecycle(profile);
  const bool transitions=hasEvent(first.events,dbnet::GameplayEventType::PlayerDowned)&&
    hasEvent(first.events,dbnet::GameplayEventType::PlayerRevived)&&
    hasEvent(first.events,dbnet::GameplayEventType::PlayerDied);
  const bool stale=first.staleSnapshots>=1&&first.staleEvents>=2&&
    first.staleInputs>=1&&first.events.duplicateRejected>0;
  const bool converged=first.hostFinal==first.guestFinal;
  if(!(first==second&&first.damageTick>0&&first.downTick>first.damageTick&&
       first.reviveTick>first.downTick&&first.deathTick>first.reviveTick&&
       first.hostOnlyDamage&&first.soulsPreserved&&first.reviveInterrupted&&
       first.deadMovementBlocked&&first.guestRestartRejected&&
       first.resetInvariants&&transitions&&stale&&converged)){
    std::fprintf(stderr,"LIFECYCLE_FAILURE profile=%s damage=%u down=%u "
      "revive=%u death=%u restart=%u authority=%d souls=%d interruption=%d "
      "blocked=%d guest_restart=%d reset=%d transitions=%d stale=%d\n",
      profile.name,first.damageTick,first.downTick,first.reviveTick,
      first.deathTick,first.restartTick,first.hostOnlyDamage?1:0,
      first.soulsPreserved?1:0,first.reviveInterrupted?1:0,
      first.deadMovementBlocked?1:0,first.guestRestartRejected?1:0,
      first.resetInvariants?1:0,transitions?1:0,stale?1:0);
    printHashes("host",first.hostFinal);printHashes("guest",first.guestFinal);
    return false;
  }
  std::printf("MULTIPLAYER_DETERMINISTIC_DEATH_OK profile=%s player=1 "
    "damage_tick=%u down_tick=%u revive_tick=%u death_tick=%u final_hash=%llu\n",
    profile.name,first.damageTick,first.downTick,first.reviveTick,
    first.deathTick,static_cast<unsigned long long>(first.finalHash));
  std::printf("MULTIPLAYER_DETERMINISTIC_RESTART_OK profile=%s old_run=%u "
    "new_run=%u restart_tick=%u final_hash=%llu input=%u/%u snapshot=%u/%u "
    "event=%u/%u accepted=%u duplicate=%u\n",profile.name,
    first.oldWorld.runGeneration,first.newWorld.runGeneration,
    first.restartTick,static_cast<unsigned long long>(first.finalHash),
    first.delivery.delivered[0],first.delivery.dropped[0],
    first.delivery.delivered[1],first.delivery.dropped[1],
    first.delivery.delivered[2],first.delivery.dropped[2],
    first.events.accepted,first.events.duplicateRejected);
  std::printf("MULTIPLAYER_STALE_RUN_REJECT_OK profile=%s snapshots=%u "
    "events=%u inputs=%u\n",profile.name,first.staleSnapshots,
    first.staleEvents,first.staleInputs);
  printHashes("lifecycle_sections",first.hostFinal);
  return true;
}

struct SessionRunResult {
  DeliverySummary delivery;
  std::uint32_t guestDisconnectTick=0;
  std::uint32_t hostDisconnectTick=0;
  std::uint32_t timeoutTick=0;
  std::uint32_t rejectedDisconnectedInputs=0;
  std::uint32_t rejectedStaleSnapshots=0;
  std::uint32_t rejectedStaleEvents=0;
  bool guestInactive=false;
  bool hostContinued=false;
  bool guestTerminal=false;
  bool timeoutTerminal=false;
  dbnet::DurableSectionHashes hostFinal;
  bool operator==(const SessionRunResult& other) const {
    return delivery==other.delivery&&
      guestDisconnectTick==other.guestDisconnectTick&&
      hostDisconnectTick==other.hostDisconnectTick&&
      timeoutTick==other.timeoutTick&&
      rejectedDisconnectedInputs==other.rejectedDisconnectedInputs&&
      rejectedStaleSnapshots==other.rejectedStaleSnapshots&&
      rejectedStaleEvents==other.rejectedStaleEvents&&
      guestInactive==other.guestInactive&&hostContinued==other.hostContinued&&
      guestTerminal==other.guestTerminal&&timeoutTerminal==other.timeoutTerminal&&
      hostFinal==other.hostFinal;
  }
};

SessionRunResult runSessionContinuity(const NetworkProfile& profile){
  constexpr std::uint32_t guestDisconnectTick=90;
  constexpr std::uint32_t hostDisconnectTick=180;
  constexpr std::uint32_t timeoutTick=240;
  constexpr std::uint32_t endTick=260;
  const std::uint32_t seed=EXPLICIT_SEED+6;
  Game host;Game guest;host.reset();guest.reset();
  host.configureNetworkHost();host.setNetworkPeerActive(1,true);
  guest.configureNetworkGuest(1);
  dbnet::NetworkWorldContext world{seed,1,1,1};
  dbnet::NetworkWorldContext staleWorld{seed-1,1,1,1};
  DeterministicPacketQueue queue(profile,seed);
  SessionRunResult result;
  dbmultiplayer::Phase guestPhase=dbmultiplayer::Phase::Playing;
  dbmultiplayer::Phase timeoutPhase=dbmultiplayer::Phase::Connecting;
  bool guestConnected=true,hostConnected=true;
  std::uint32_t inputSequence=0,snapshotSequence=0,lastInputSequence=0;
  std::vector<std::uint8_t> retainedInput;

  auto receive=[&](const Packet& packet){
    dbnet::PacketHeader header;
    if(packet.direction==Direction::GuestToHost){
      dbnet::NetworkWorldContext packetWorld;dbnet::InputCommand input;
      if(!dbnet::decodeInput(packet.bytes.data(),packet.bytes.size(),header,
                             packetWorld,input))return;
      if(!guestConnected||packetWorld!=world||header.sequence<=lastInputSequence){
        ++result.rejectedDisconnectedInputs;return;
      }
      lastInputSequence=header.sequence;
      host.setNetworkPeerCommand(header.playerId,input);
      queue.summary().lastInput=header.sequence;return;
    }
    if(!hostConnected){
      if(packet.type==dbnet::MessageType::Snapshot)
        ++result.rejectedStaleSnapshots;
      else if(packet.type==dbnet::MessageType::Event)
        ++result.rejectedStaleEvents;
      return;
    }
    if(packet.type==dbnet::MessageType::Snapshot){
      dbnet::WorldSnapshot snapshot;
      if(!dbnet::decodeSnapshot(packet.bytes.data(),packet.bytes.size(),header,
                                snapshot)||snapshot.world!=world){
        ++result.rejectedStaleSnapshots;return;
      }
      dbnet::applyWorld(guest.networkMutableState(),snapshot,1);
      queue.summary().lastSnapshot=header.sequence;
    }
  };

  for(std::uint32_t tick=1;tick<=endTick;++tick){
    queue.deliver(tick,receive);
    if(tick<guestDisconnectTick){
      guest.setTouchControls(0,0.7f,0,0,false,false,false,false,false,false);
      auto input=dbnet::encodeInput(1,world,
        guest.capturePlayerCommand(++inputSequence,tick));
      if(tick==30)retainedInput=input;
      queue.send(Direction::GuestToHost,dbnet::MessageType::Input,tick,
                 std::move(input));
      guest.update(FIXED_DT);
    }
    if(tick==guestDisconnectTick){
      guestConnected=false;host.setNetworkPeerActive(1,false);
      result.guestDisconnectTick=tick;
    }
    if(tick==guestDisconnectTick+10&&!retainedInput.empty())
      receive({Direction::GuestToHost,dbnet::MessageType::Input,tick,0,
               retainedInput});
    if(hostConnected)host.update(FIXED_DT);
    if(hostConnected&&tick%SNAPSHOT_INTERVAL==0){
      auto snapshot=dbnet::captureWorld(
        host.state(),dbnet::capturePlayers(host.state()),tick);
      snapshot.world=world;
      queue.send(Direction::HostToGuest,dbnet::MessageType::Snapshot,tick,
        dbnet::encodeSnapshot(0,snapshot,++snapshotSequence));
    }
    if(tick==hostDisconnectTick){
      hostConnected=false;
      guestPhase=dbmultiplayer::transition(
        guestPhase,dbmultiplayer::Event::HostDisconnected);
      guest.prepareStartScreen();
      result.hostDisconnectTick=tick;
      auto stale=dbnet::captureWorld(
        host.state(),dbnet::capturePlayers(host.state()),tick);
      stale.world=staleWorld;
      receive({Direction::HostToGuest,dbnet::MessageType::Snapshot,tick,0,
        dbnet::encodeSnapshot(0,stale,++snapshotSequence)});
      dbnet::GameplayEvent oldEvent;
      oldEvent.world=staleWorld;oldEvent.eventId=1;
      oldEvent.authoritativeTick=tick;
      oldEvent.type=dbnet::GameplayEventType::PlayerActionStarted;
      receive({Direction::HostToGuest,dbnet::MessageType::Event,tick,1,
        dbnet::encodeEvent(0,oldEvent)});
    }
    if(tick==timeoutTick){
      timeoutPhase=dbmultiplayer::transition(
        timeoutPhase,dbmultiplayer::Event::Failure);
      result.timeoutTick=tick;
    }
  }
  result.guestInactive=!host.state().multiplayer.peers[1].active;
  result.hostContinued=host.state().frame>=static_cast<int>(
    hostDisconnectTick-1);
  result.guestTerminal=guestPhase==dbmultiplayer::Phase::HostLeft&&
    !guest.state().started&&!guest.state().multiplayer.enabled;
  result.timeoutTerminal=timeoutPhase==dbmultiplayer::Phase::Failed;
  auto final=dbnet::captureWorld(
    host.state(),dbnet::capturePlayers(host.state()),hostDisconnectTick);
  final.world=world;
  result.hostFinal=dbnet::durableSectionHashes(final);
  result.delivery=queue.summary();
  return result;
}

bool verifySessionContinuity(const NetworkProfile& profile){
  const auto first=runSessionContinuity(profile);
  const auto second=runSessionContinuity(profile);
  if(!(first==second&&first.guestDisconnectTick==90&&
       first.hostDisconnectTick==180&&first.timeoutTick==240&&
       first.rejectedDisconnectedInputs>0&&
       first.rejectedStaleSnapshots>0&&first.rejectedStaleEvents>0&&
       first.guestInactive&&
       first.hostContinued&&first.guestTerminal&&first.timeoutTerminal)){
    std::fprintf(stderr,"SESSION_FAILURE profile=%s guest=%u host=%u timeout=%u "
      "input_reject=%u stale_reject=%u inactive=%d continued=%d terminal=%d/%d\n",
      profile.name,first.guestDisconnectTick,first.hostDisconnectTick,
      first.timeoutTick,first.rejectedDisconnectedInputs,
      first.rejectedStaleSnapshots+first.rejectedStaleEvents,
      first.guestInactive?1:0,
      first.hostContinued?1:0,first.guestTerminal?1:0,
      first.timeoutTerminal?1:0);
    return false;
  }
  std::printf("MULTIPLAYER_SESSION_END role=guest reason=host_disconnected "
    "profile=%s tick=%u\n",profile.name,first.hostDisconnectTick);
  std::printf("MULTIPLAYER_SESSION_CONTINUITY_OK profile=%s "
    "guest_disconnect_tick=%u timeout_tick=%u input_reject=%u "
    "stale_snapshot_reject=%u stale_event_reject=%u "
    "input=%u/%u snapshot=%u/%u event=%u/%u "
    "final_players_hash=%llu\n",profile.name,
    first.guestDisconnectTick,first.timeoutTick,
    first.rejectedDisconnectedInputs,first.rejectedStaleSnapshots,
    first.rejectedStaleEvents,
    first.delivery.delivered[0],first.delivery.dropped[0],
    first.delivery.delivered[1],first.delivery.dropped[1],
    first.delivery.delivered[2],first.delivery.dropped[2],
    static_cast<unsigned long long>(first.hostFinal.players));
  return true;
}

}  // namespace

int main() {
  const NetworkProfile baseline{"baseline", 0, 0};
  const NetworkProfile impaired{"impaired", 4, 11};
  const NetworkProfile moderate{"moderate", 4, 11};
  const NetworkProfile poorRecoverable{"poor_recoverable", 8, 5};
  return verifyProfile(baseline) && verifyProfile(impaired) &&
         verifyMeleeProfile(baseline) && verifyMeleeProfile(moderate) &&
         verifyMeleeProfile(poorRecoverable) &&
         verifyVacuumDischarge(baseline) &&
         verifyVacuumDischarge(moderate) &&
         verifyVacuumDischarge(poorRecoverable) &&
         verifyRoomRollover(baseline) &&
         verifyRoomRollover(moderate) &&
         verifyRoomRollover(poorRecoverable) &&
         verifyLifecycle(baseline) &&
         verifyLifecycle(moderate) &&
         verifyLifecycle(poorRecoverable) &&
         verifySessionContinuity(baseline) &&
         verifySessionContinuity(moderate) &&
         verifySessionContinuity(poorRecoverable) ? 0 : 1;
}
