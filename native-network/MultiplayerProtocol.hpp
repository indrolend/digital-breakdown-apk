#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Game.hpp"

namespace dbnet {

constexpr std::uint32_t MAGIC = 0x504d4244u;
constexpr std::uint16_t PROTOCOL_VERSION = 3;
constexpr std::uint16_t GAMEPLAY_VERSION = 3;
constexpr std::size_t HEADER_BYTES = 20;
constexpr std::size_t MAX_PACKET_BYTES = 64u * 1024u;
constexpr int MAX_PLAYERS = 4;

enum class MessageType : std::uint8_t { Input = 1, Snapshot = 2, Event = 3, Ping = 4, Pong = 5 };
enum InputButton : std::uint16_t {
    Forward = 1u << 0, Back = 1u << 1, Left = 1u << 2, Right = 1u << 3,
    Sprint = 1u << 4, Jump = 1u << 5, Vacuum = 1u << 6, Melee = 1u << 7,
    Shoot = 1u << 8, CameraToggle = 1u << 9
};

struct PacketHeader {
    MessageType type = MessageType::Input;
    std::uint8_t playerId = 0;
    std::uint32_t sequence = 0;
    std::uint32_t tick = 0;
    std::uint32_t payloadBytes = 0;
};

struct InputCommand {
    std::uint32_t sequence = 0;
    std::uint32_t tick = 0;
    float moveX = 0.0f;
    float moveZ = 0.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    std::uint16_t buttons = 0;
};

struct PlayerSnapshot {
    bool active = false;
    std::uint8_t id = 0;
    Vec3 pos;
    Vec3 vel;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float battery = 100.0f;
    std::uint8_t souls = 0;
    std::uint8_t flags = 0;
    float vacuumPower = 0.0f;
    float vacuumPose = 0.0f;
    std::int8_t vacuumTarget = -1;
    float meleeTimer = 0.0f;
    float dischargeAmount = 0.0f;
};

struct TargetSnapshot {
    std::uint8_t flags = 0;
    SoulState soulState = SoulState::Free;
    Vec3 pos;
    Vec3 vel;
    float armor = 0.0f;
    float health = 0.0f;
    float capture = 0.0f;
    float ingest = 0.0f;
    float recoil = 0.0f;
    float scale = 1.0f;
    float visualYaw = 0.0f;
    float soulMorph = 0.0f;
    float attackTimer = 0.0f;
    float attackCooldown = 0.0f;
    std::int8_t ownerPlayerId = -1;
};

struct BulletSnapshot {
    bool active = false;
    bool brute = false;
    Vec3 pos;
    Vec3 vel;
    float life = 0.0f;
    float spin = 0.0f;
};

struct FlowerSnapshot {
    bool active = false;
    Vec3 pos;
    float age = 0.0f;
    float rotation = 0.0f;
};

struct WorldSnapshot {
    std::uint32_t tick = 0;
    float time = 0.0f;
    std::int32_t roomIndex = 1;
    std::int32_t roomSeed = 12345;
    std::int32_t requiredSouls = 5;
    std::int32_t depositedSouls = 0;
    bool roomClear = false;
    RunRuleState runRules;
    bool upgradeMenuActive = false;
    std::array<std::int32_t,3> temporaryUpgradeLevels{};
    std::array<std::int32_t,3> sharedPermanentUpgradeLevels{};
    float roomHeat = 0.0f;
    std::array<PlayerSnapshot, MAX_PLAYERS> players{};
    std::array<TargetSnapshot, TARGET_COUNT> targets{};
    std::array<bool, CAPTURE_COUNT> captures{};
    std::array<BulletSnapshot, BULLET_COUNT> bullets{};
    std::array<FlowerSnapshot, FLOWER_POWERUP_COUNT> flowers{};
};

bool decodeHeader(const std::uint8_t* data, std::size_t size, PacketHeader& header);
std::vector<std::uint8_t> encodeInput(std::uint8_t playerId, const InputCommand& input);
bool decodeInput(const std::uint8_t* data, std::size_t size, PacketHeader& header, InputCommand& input);
std::vector<std::uint8_t> encodeSnapshot(std::uint8_t playerId, const WorldSnapshot& snapshot, std::uint32_t sequence);
bool decodeSnapshot(const std::uint8_t* data, std::size_t size, PacketHeader& header, WorldSnapshot& snapshot);

WorldSnapshot captureWorld(const GameState& state, const std::array<PlayerSnapshot, MAX_PLAYERS>& players, std::uint32_t tick);
std::array<PlayerSnapshot, MAX_PLAYERS> capturePlayers(const GameState& state);
void applyWorld(GameState& state, const WorldSnapshot& snapshot, std::uint8_t localPlayerId);

}
