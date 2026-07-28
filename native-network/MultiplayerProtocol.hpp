#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Game.hpp"

namespace dbnet {

constexpr std::uint32_t MAGIC = 0x504d4244u;
constexpr std::uint16_t PROTOCOL_VERSION = 7;
constexpr std::uint16_t GAMEPLAY_VERSION = 5;
constexpr std::size_t HEADER_BYTES = 20;
constexpr std::size_t MAX_PACKET_BYTES = 64u * 1024u;
constexpr std::size_t MAX_SNAPSHOT_BYTES = 12u * 1024u;
constexpr int MAX_PLAYERS = 4;
constexpr std::int64_t REMOTE_INTERPOLATION_DELAY_MS = 50;
constexpr std::int64_t REMOTE_MAX_EXTRAPOLATION_MS = 100;
constexpr float REMOTE_TELEPORT_RESET_DISTANCE = 4.0f;
constexpr float LOCAL_PREDICTION_SNAP_DISTANCE = 1.5f;
constexpr float LOCAL_PREDICTION_LOG_DISTANCE = 0.05f;

enum class MessageType : std::uint8_t { Input = 1, Snapshot = 2, Event = 3, Ping = 4, Pong = 5 };
enum InputButton : std::uint16_t {
    Forward=CommandForward, Back=CommandBack, Left=CommandLeft, Right=CommandRight,
    Sprint=CommandSprint, Jump=CommandJump, Vacuum=CommandVacuum, Melee=CommandMelee,
    Shoot=CommandShoot, CameraToggle=CommandCameraToggle,
    WiggleLeft=CommandWiggleLeft, WiggleRight=CommandWiggleRight,
    CommHelp=CommandCommHelp, CommPing=CommandCommPing,
    CommGroup=CommandCommGroup, CommOk=CommandCommOk
};

struct PacketHeader {
    MessageType type = MessageType::Input;
    std::uint8_t playerId = 0;
    std::uint32_t sequence = 0;
    std::uint32_t tick = 0;
    std::uint32_t payloadBytes = 0;
};

struct NetworkWorldContext {
    std::uint32_t sessionId = 0;
    std::uint32_t runGeneration = 0;
    std::uint32_t roomGeneration = 0;
    std::uint16_t roomIndex = 0;
    bool operator==(const NetworkWorldContext& other) const {
        return sessionId == other.sessionId &&
               runGeneration == other.runGeneration &&
               roomGeneration == other.roomGeneration &&
               roomIndex == other.roomIndex;
    }
    bool operator!=(const NetworkWorldContext& other) const { return !(*this == other); }
};

enum class WorldContextCompatibility : std::uint8_t {
    Compatible, Older, NewerRoom, NewerRun, Incompatible
};

using InputCommand = PlayerCommand;

enum class NetLocomotionState : std::uint8_t { Idle, Walking, Sprinting, Airborne, LedgeHang, LedgeMantle, Dead };
enum class NetActionState : std::uint8_t { None, Melee, AirLunge, Vacuum, Discharge, DamageReaction, Grabbed, Revive };
enum class NetActionPhase : std::uint8_t { None, Startup, Active, Contact, Recovery };

struct PlayerSnapshot {
    bool active = false;
    std::uint8_t id = 0;
    Vec3 pos;
    Vec3 vel;
    float yaw = 0.0f;
    float targetYaw = 0.0f;
    float pitch = 0.0f;
    float jumpVel = 0.0f;
    float battery = 100.0f;
    std::uint8_t souls = 0;
    std::uint8_t flags = 0;
    std::uint8_t actionFlags = 0;
    NetLocomotionState locomotion = NetLocomotionState::Idle;
    NetActionState action = NetActionState::None;
    NetActionPhase actionPhase = NetActionPhase::None;
    std::uint16_t actionSequence = 0;
    std::uint16_t actionTargetId = 0xffffu;
    float actionProgress = 0.0f;
    std::uint8_t storedSoulBruteMask = 0;
    std::int8_t airJumpsRemaining = 0;
    std::int8_t ledgeCollider = -1;
    Vec3 ledgeNormal;
    float ledgeHangTime = 0.0f;
    float ledgeMantleTimer = 0.0f;
    float vacuumPower = 0.0f;
    float vacuumPose = 0.0f;
    float vacuumFieldStrength = 0.0f;
    float vacuumConeTightness = 0.0f;
    float vacuumLockStrength = 0.0f;
    std::int8_t vacuumTarget = -1;
    float meleeTimer = 0.0f;
    float dischargeAmount = 0.0f;
    float dischargeTimer = 0.0f;
    float supplementalValue = 0.0f;
    float supplementalMax = 85.0f;
    std::uint8_t flowerStacks = 0;
    float phonePitch = 0.0f;
    float phoneRoll = 0.0f;
    float phoneYaw = 0.0f;
    float phoneLift = 0.0f;
    float phoneForward = 0.0f;
    float phoneSide = 0.0f;
    float doubleJumpTimer = 0.0f;
    float doubleJumpFlipYaw = 0.0f;
    float doubleJumpFlip = 0.0f;
    Quat phoneOrientation;
    std::uint8_t phoneActionState = 0;
    std::uint8_t meleeVariant = 0;
    std::uint8_t meleeComboIndex = 0;
    Vec3 meleeDirection{0.0f,0.0f,-1.0f};
    float airLungeRotation = 0.0f;
    float landingRecovery = 0.0f;
    float bleedoutTimer = 0.0f;
    float reviveCharge = 0.0f;
    float grabEscape = 0.0f;
    std::int8_t grabbedByTarget = -1;
    std::int32_t secretVisitRoom = -1;
    float secretVisitTimer = 0.0f;
    std::uint8_t commSignal = 0;
    float commSignalTimer = 0.0f;
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
    float animationTime = 0.0f;
    float visualWalkPhase = 0.0f;
    float locomotionAmount = 0.0f;
    Vec3 attackDirection{0.0f,0.0f,-1.0f};
    std::uint8_t attackVariant = 0;
    std::uint8_t visualFlags = 0;
    float hitFlash = 0.0f;
    float phase = 0.0f;
    float floatOffset = 0.0f;
    float spinSpeed = 0.8f;
    float hitDirectionLocal = 0.0f;
    float vacuumPullAmount = 0.0f;
    float captureCollapseAmount = 0.0f;
    float visibility = 1.0f;
    float armorRegenDelay = 0.0f;
    float respawnTimer = 0.0f;
    std::int8_t ownerPlayerId = -1;
    std::int8_t grabbedPlayerId = -1;
    float grabCooldown = 0.0f;
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
    NetworkWorldContext world;
    std::uint32_t tick = 0;
    float time = 0.0f;
    std::int32_t roomIndex = 1;
    std::int32_t roomSeed = 12345;
    std::int32_t requiredSouls = 5;
    std::int32_t depositedSouls = 0;
    bool roomClear = false;
    bool started = false;
    bool dead = false;
    RunRuleState runRules;
    bool upgradeMenuActive = false;
    std::array<std::int32_t,3> temporaryUpgradeLevels{};
    std::array<std::int32_t,3> sharedPermanentUpgradeLevels{};
    float roomHeat = 0.0f;
    std::int32_t tvSignal = 0;
    std::int32_t tvDamage = 0;
    std::int32_t tvTolerance = 3;
    bool tvBroken = false;
    bool tvAvailable = false;
    Vec3 tvEntrancePos;
    Vec3 tvEntranceNormal;
    RoomTopologyState topology;
    DoorTransitionState doorTransition;
    std::uint8_t roomColliderCount = 0;
    std::array<RoomCollider, ROOM_COLLIDER_COUNT> roomColliders{};
    std::array<PlayerSnapshot, MAX_PLAYERS> players{};
    std::array<TargetSnapshot, TARGET_COUNT> targets{};
    std::array<bool, CAPTURE_COUNT> captures{};
    std::array<Vec3, CAPTURE_COUNT> capturePositions{};
    std::array<BulletSnapshot, BULLET_COUNT> bullets{};
    std::array<FlowerSnapshot, FLOWER_POWERUP_COUNT> flowers{};
};

enum class GameplayEventType : std::uint8_t {
    PlayerActionStarted,
    PlayerActionContact,
    EnemyHitConfirmed,
    EnemyShellBroken,
    SoulEmergenceStarted,
    VacuumStarted,
    SoulAttractionStarted,
    SoulLatched,
    SoulIngestionStarted,
    SoulCaptureCompleted,
    DischargeStarted,
    ProjectileSpawned,
    ProjectileImpacted,
    ProjectileDespawned,
    PlayerDowned,
    PlayerRevived,
    PlayerDied
};

struct GameplayEvent {
    NetworkWorldContext world;
    std::uint32_t authoritativeTick = 0;
    std::uint32_t eventId = 0;
    GameplayEventType type = GameplayEventType::PlayerActionStarted;
    std::uint16_t sourceEntityId = 0;
    std::uint16_t targetEntityId = 0xffffu;
    Vec3 position;
    Vec3 direction;
    std::uint16_t flags = 0;
};

class GameplayEventTracker {
public:
    void reset(const NetworkWorldContext& world);
    bool accept(const GameplayEvent& event);
    std::uint32_t lastEventId() const { return lastEventId_; }
private:
    NetworkWorldContext world_{};
    std::uint32_t lastEventId_ = 0;
};

bool decodeHeader(const std::uint8_t* data, std::size_t size, PacketHeader& header);
WorldContextCompatibility compareWorldContext(const NetworkWorldContext& packet, const NetworkWorldContext& current);
std::vector<std::uint8_t> encodeInput(std::uint8_t playerId, const NetworkWorldContext& world, const InputCommand& input);
bool decodeInput(const std::uint8_t* data, std::size_t size, PacketHeader& header, NetworkWorldContext& world, InputCommand& input);
std::vector<std::uint8_t> encodeInput(std::uint8_t playerId, const InputCommand& input);
bool decodeInput(const std::uint8_t* data, std::size_t size, PacketHeader& header, InputCommand& input);
std::vector<std::uint8_t> encodeSnapshot(std::uint8_t playerId, const WorldSnapshot& snapshot, std::uint32_t sequence);
bool decodeSnapshot(const std::uint8_t* data, std::size_t size, PacketHeader& header, WorldSnapshot& snapshot);
std::vector<std::uint8_t> encodeEvent(std::uint8_t playerId, const GameplayEvent& event);
bool decodeEvent(const std::uint8_t* data, std::size_t size, PacketHeader& header, GameplayEvent& event);
std::vector<GameplayEvent> deriveMeleeEvents(
    const WorldSnapshot& previous, const WorldSnapshot& current,
    std::uint32_t& nextEventId);
struct GameplayEventDerivationState {
    std::uint32_t nextEventId = 0;
    std::uint16_t lastDischargeSource = 0;
    std::array<std::uint16_t, BULLET_COUNT> projectileSources{};
};
std::vector<GameplayEvent> deriveGameplayEvents(
    const WorldSnapshot& previous, const WorldSnapshot& current,
    GameplayEventDerivationState& state);

WorldSnapshot captureWorld(const GameState& state, const std::array<PlayerSnapshot, MAX_PLAYERS>& players, std::uint32_t tick);
std::array<PlayerSnapshot, MAX_PLAYERS> capturePlayers(const GameState& state);
void applyWorld(GameState& state, const WorldSnapshot& snapshot, std::uint8_t localPlayerId);
void prepareForAuthoritativeWorldReplacement(GameState& state);

struct DurableSectionHashes {
    std::uint64_t world = 0;
    std::uint64_t players = 0;
    std::uint64_t targets = 0;
    std::uint64_t projectiles = 0;
    std::uint64_t progression = 0;
    bool operator==(const DurableSectionHashes& other) const {
        return world == other.world && players == other.players &&
               targets == other.targets && projectiles == other.projectiles &&
               progression == other.progression;
    }
    bool operator!=(const DurableSectionHashes& other) const { return !(*this == other); }
};

DurableSectionHashes durableSectionHashes(const WorldSnapshot& snapshot);
std::uint64_t authoritativeStateHash(const WorldSnapshot& snapshot);
std::uint64_t visualStateHash(const WorldSnapshot& snapshot);

class SnapshotInterpolator {
public:
    void reset();
    void push(const WorldSnapshot& snapshot, std::int64_t receiveTimeMs);
    bool ready() const { return hasCurrent_; }
    void apply(GameState& renderState, std::uint8_t localPlayerId,
               std::int64_t renderTimeMs) const;
    std::uint32_t currentTick() const {
        return hasCurrent_ ? current_.snapshot.tick : 0;
    }
private:
    struct TimedSnapshot {
        WorldSnapshot snapshot;
        std::int64_t receiveTimeMs = 0;
    };
    TimedSnapshot previous_{};
    TimedSnapshot current_{};
    bool hasPrevious_ = false;
    bool hasCurrent_ = false;
};

}
