#include "Game.hpp"
#include "MultiplayerProtocol.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
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

}  // namespace

int main() {
  const NetworkProfile baseline{"baseline", 0, 0};
  const NetworkProfile impaired{"impaired", 4, 11};
  return verifyProfile(baseline) && verifyProfile(impaired) ? 0 : 1;
}
