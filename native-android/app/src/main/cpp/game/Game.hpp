#pragma once

#include <array>

#include "HumanVisual.hpp"
#include "VisualIdentity.hpp"
#include "Math.hpp"

constexpr int TARGET_COUNT = 32;
constexpr int CAPTURE_COUNT = 9;
constexpr int BULLET_COUNT = 30;
constexpr int FLOWER_POWERUP_COUNT = 32;
constexpr int PARTICLE_COUNT = 256;
constexpr int AUDIO_EVENT_COUNT = 64;
constexpr int ROOM_COLLIDER_COUNT = 15;
constexpr int PHONE_CAPACITY = 30;
constexpr int SOUL_LATTICE_NODE_COUNT = 27;
constexpr float PHONE_MODEL_HEIGHT = 0.16f;
constexpr float PHONE_BODY_WIDTH = 0.08f;
constexpr float PHONE_BODY_HEIGHT = PHONE_MODEL_HEIGHT;
constexpr float PHONE_BODY_DEPTH = 0.012f;
constexpr float PHONE_SCREEN_WIDTH = 0.07f;
constexpr float PHONE_SCREEN_HEIGHT = 0.125f;
constexpr float PHONE_SCREEN_DEPTH = 0.002f;
constexpr float PHONE_SCREEN_Z_OFFSET = 0.007f;

enum class SoulState : unsigned char {
    Free,
    Attracted,
    Latched,
    Ingesting,
    Recoiling,
    Revolving
};

struct InputState {
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool sprint = false;
    bool jumpPressed = false;
    bool jumpHeld = false;
    bool primaryHeld = false;
    bool meleePressed = false;
    bool shootPressed = false;
    bool cameraTogglePressed = false;

    float touchMoveX = 0.0f;
    float touchMoveZ = 0.0f;
    bool touchSprint = false;
    bool touchPrimaryHeld = false;

    float touchX = 0.0f;
    float touchY = 0.0f;
    float lastTouchX = 0.0f;
    float lastTouchY = 0.0f;
    float lookDeltaX = 0.0f;
    float lookDeltaY = 0.0f;
    bool touching = false;
};

struct PlayerState {
    Vec3 pos {0.0f, 0.08f, 0.0f};
    Vec3 vel {0.0f, 0.0f, 0.0f};
    float jumpVel = 0.0f;
    float yaw = 0.0f;
    float targetYaw = 0.0f;
    bool grounded = true;
    float battery = 100.0f;
    int souls = 0;
    std::array<bool, PHONE_CAPACITY> storedSoulBrute{};
    int airJumpsRemaining = 1;
    float coyoteTimer = 0.12f;
    float jumpBufferTimer = 0.0f;
    bool alive = true;
};

enum class AudioCue : unsigned char {
    VcEnded, VcInvitation, ConnectPower, LowPower, NegativeAck, ReceivedMessage,
    SentMessage, PhoneAttack, PaymentSuccess, PaymentFailure, EndCallTone,
    SlurpRingtoneStart, SlurpRingtoneStop, Capture1, Capture2, Capture3, Capture4, Capture5
};

struct AudioEventState {
    AudioCue cue = AudioCue::VcInvitation;
    unsigned int serial = 0;
    float volume = 0.55f;
};

struct AudioState {
    std::array<AudioEventState, AUDIO_EVENT_COUNT> events;
    unsigned int nextSerial = 1;
    bool slurpPlaying = false;
    bool lowPowerArmed = true;
    bool connectPowerArmed = false;
    float lastDamageAckTime = -9999.0f;
};

struct EnergyState {
    bool supplementalActive = false;
    float supplementalValue = 0.0f;
    float supplementalMax = 85.0f;
    int flowerStacks = 0;
    int comboHits = 0;
    float comboMultiplier = 1.0f;
    float lastComboHitTime = -9999.0f;
    float dischargeTimer = 0.0f;
    float dischargePositionAmount = 0.0f;
};

struct CameraState {
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool firstPerson = false;
    Vec3 pos {0.0f, 1.18f, 3.0f};
    Vec3 forward {0.0f, 0.0f, -1.0f};
    Vec3 lookTarget {0.0f, 0.53f, 0.0f};
};

struct CinematicState {
    bool introActive = false;
    bool deathActive = false;
    float introElapsed = 0.0f;
    float deathElapsed = 0.0f;
    float baseYaw = 0.0f;
    Vec3 startCameraPos;
};

struct VacuumState {
    bool active = false;
    float power = 0.0f;
    float pose = 0.0f;
    float fieldStrength = 0.0f;
    float coneTightness = 0.0f;
    float lockStrength = 0.0f;
    int target = -1;
};

struct TargetState {
    Vec3 pos;
    Vec3 vel;
    Vec3 latchPoint;
    bool alive = false;
    bool slurpable = false;
    float armor = 2.0f;
    float health = 1.0f;
    float capture = 0.0f;
    float ingestProgress = 0.0f;
    float recoilTime = 0.0f;
    bool captureQueued = false;
    bool captureCommitted = false;
    bool latchedToScreen = false;
    float respawnTimer = 0.0f;
    float scale = 1.0f;
    float phase = 0.0f;
    float floatOffset = 0.0f;
    float spinSpeed = 0.8f;
    float visualYaw = 0.0f;
    float visualWalkPhase = 0.0f;
    float humanAnimationTime = 0.0f;
    float locomotionAmount = 0.0f;
    float hitFlash = 0.0f;
    float hitDirectionLocal = 0.0f;
    float vacuumPullAmount = 0.0f;
    float captureCollapseAmount = 0.0f;
    float visibility = 1.0f;
    float soulCubeAmount = 0.0f;
    float soulMorph = 0.0f;
    Vec3 walkTarget;
    int walkTargetSequence = 0;
    float attackTimer = 0.0f;
    float attackCooldown = 0.0f;
    int attackVariant = 0;
    bool attackHit = false;
    Vec3 attackDirection{0.0f,0.0f,-1.0f};
    int attackTargetPlayerId = 0;
    HumanReactionVisual visualReaction;
    bool brute = false;
    SoulState soulState = SoulState::Free;
    SoulVisualState soulVisual;
    std::array<Vec3,SOUL_LATTICE_NODE_COUNT> latticePos{};
    std::array<Vec3,SOUL_LATTICE_NODE_COUNT> latticeVel{};
    std::array<Vec3,SOUL_LATTICE_NODE_COUNT> latticeSurfacePos{};
    float latticeVisualPull = 0.0f;
    float latticeVisualPullVelocity = 0.0f;
    Vec3 tetherAnchor;
    Vec3 tetherDestination;
    float tetherWidth = 0.0f;
    bool tetherVisible = false;
    int networkOwnerPlayerId = -1;
};

struct CapturePointState {
    Vec3 pos;
    bool filled = false;
};

struct BulletState {
    Vec3 pos;
    Vec3 vel;
    bool alive = false;
    float life = 0.0f;
    float spin = 0.0f;
    bool brute = false;
    bool depositNearMissPlayed = false;
};

struct PendingShotState {
    bool active = false;
    bool brute = false;
    float age = 0.0f;
};

struct FlowerPowerupState {
    Vec3 pos;
    float baseY = 0.38f;
    float age = 0.0f;
    float rotationY = 0.0f;
    bool active = false;
};

struct ParticleState {
    Vec3 pos;
    Vec3 vel;
    float life = 0.0f;
    float maxLife = 0.0f;
};

struct RoomCollider {
    float minX = 0.0f;
    float maxX = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    float bottomY = 0.0f;
    float topY = 0.0f;
    float width = 0.0f;
    float depth = 0.0f;
    float height = 0.0f;
    Vec3 center;
};

struct RoomTopologyState {
    int currentTileIndex = 0;
    int previousTileIndex = 0;
    bool advancing = false;
};

struct PhonePoseState {
    float phase = 0.0f;
    float rollEnergy = 0.0f;
    float energy = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    float yaw = 0.0f;
    float lift = 0.0f;
    float forward = 0.0f;
    float side = 0.0f;
    Quat orientation;
    float screenForwardTurn = 0.0f;
    float doubleJumpTimer = 0.0f;
    float doubleJumpVacuumPause = 0.0f;
    float doubleJumpFlipYaw = 0.0f;
    float doubleJumpFlip = 0.0f;
    int actionState = 0;
};

struct RunRuleState {
    int requiredSlotStacks = 0;
    int crowdedRoomStacks = 0;
    int fasterSlurpStacks = 0;
    int nextId = 1;
    int lastAdded = -1;
};

struct HumanRespawnRequest {
    float delay = 0.0f;
    Vec3 avoid;
    bool active = false;
};

struct DoorTransitionState {
    bool active = false;
    float progress = 0.0f;
    float distanceTravelled = 0.0f;
    Vec3 lastPlayerPos;
    Vec3 frameMotion;
};

struct PhoneTransformState {
    Vec3 position;
    Quat orientation;
    Vec3 screenCenter;
    Vec3 screenRight{1.0f, 0.0f, 0.0f};
    Vec3 screenUp{0.0f, 1.0f, 0.0f};
    Vec3 screenNormal{0.0f, 0.0f, 1.0f};
    Vec3 vacuumPullPoint;
};

struct MeleeVisualState {
    float visualTimer = 0.0f;
    float visualDuration = 0.20f;
    float dashTimer = 0.0f;
    float dashSpeed = 12.5f;
    bool airLungePending = false;
    bool airLungeLandingPending = false;
    bool locomotionLunge = false;
    float airLungeSpeed = 0.0f;
    float airLungeTimer = 0.0f;
    float airLungeRotation = 0.0f;
    float airLungeAngularVelocity = 0.0f;
    float airLungeCameraLag = 0.0f;
    float landingRecovery = 0.0f;
    float landingRecoveryDuration = 0.0f;
    float landingPosePitch = 0.0f;
    float wallGripTimer = 0.0f;
    Vec3 wallNormal;
    float wallClimbRemaining = 0.48f;
    float travel = 0.0f;
    float lunge = 0.15f;
    float recoilDistance = 0.08f;
    float recoilSpeed = 1.25f;
    float range = 2.35f;
    float hitRadius = 0.78f;
    float damage = 0.82f;
    Vec3 direction{0.0f, 0.0f, -1.0f};
    Vec3 origin;
    Vec3 impact;
    int variant = 0;
    int comboIndex = 0;
    bool visualHit = false;
    unsigned int hitMask = 0;
};

struct PlayerDebugState {
    float supportY = 0.08f;
    float localZ = 0.0f;
    float horizontalSpeed = 0.0f;
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;
    int cameraMode = 0;
    float phoneYaw = 0.0f;
    float phonePitch = 0.0f;
    float phoneRoll = 0.0f;
    float phoneLift = 0.0f;
    float phoneForward = 0.0f;
    float phoneSide = 0.0f;
    int colliderCount = 0;
};

struct HudState {
    float batteryFill = 1.0f;
    int storedSouls = 0;
    int filledGoals = 0;
    int requiredGoals = CAPTURE_COUNT;
    float vacuumField = 0.0f;
    float lockStrength = 0.0f;
    float crosshairRotationDegrees = 0.0f;
    float crosshairSpreadPixels = 8.0f;
    float shootJoinTimer = 0.0f;
    float supplementalFill = 0.0f;
    int flowerStacks = 0;
    bool hasTarget = false;
    bool hasAimTarget = false;
    bool lowBattery = false;
    bool gameOver = false;
    std::array<char,48> energyTicker{};
    float energyTickerUntil = 0.0f;
    int energyTickerType = 0;
};

constexpr int NETWORK_PLAYER_COUNT = 4;
struct NetworkPeerState {
    bool active = false;
    int playerId = -1;
    unsigned int lastInputSequence = 0;
    unsigned short inputButtons = 0;
    InputState input;
    PlayerState player;
    EnergyState energy;
    CameraState camera;
    VacuumState vacuum;
    std::array<PendingShotState, PHONE_CAPACITY> pendingShots{};
    PhonePoseState phonePose;
    PhoneTransformState phoneTransform;
    PhoneVisualState phoneVisual;
    HudState hud;
    MeleeVisualState meleeVisual;
    float meleeCooldown = 0.0f;
    float meleePose = 0.0f;
    float meleeComboWindow = 0.0f;
};

struct MultiplayerRuntimeState {
    bool enabled = false;
    bool authoritativeHost = false;
    bool connected = false;
    int localPlayerId = 0;
    std::array<char, 7> roomCode{};
    std::array<char, 64> status{};
    std::array<NetworkPeerState, NETWORK_PLAYER_COUNT> peers{};
};

struct GameState {
    InputState input;
    PlayerState player;
    EnergyState energy;
    CameraState camera;
    VacuumState vacuum;
    std::array<TargetState, TARGET_COUNT> targets;
    std::array<CapturePointState, CAPTURE_COUNT> captures;
    std::array<BulletState, BULLET_COUNT> bullets;
    std::array<PendingShotState, PHONE_CAPACITY> pendingShots;
    std::array<FlowerPowerupState, FLOWER_POWERUP_COUNT> flowers;
    std::array<ParticleState, PARTICLE_COUNT> particles;
    int nextParticle = 0;
    AudioState audio;
    std::array<int, 5> captureSoundSlots{{0,1,2,3,4}};
    std::array<RoomCollider, ROOM_COLLIDER_COUNT> roomColliders;
    RoomTopologyState topology;
    RunRuleState runRules;
    std::array<HumanRespawnRequest, TARGET_COUNT> respawnQueue;
    DoorTransitionState doorTransition;
    PhonePoseState phonePose;
    PhoneTransformState phoneTransform;
    PhoneVisualState phoneVisual;
    HudState hud;
    PlayerDebugState debug;
    float time = 0.0f;
    int frame = 0;
    int roomIndex = 1;
    int roomSeed = 12345;
    int requiredSouls = 5;
    int depositedSouls = 0;
    bool roomClear = false;
    bool started = true;
    bool dead = false;
    bool uiPaused = false;
    CinematicState cinematic;
    unsigned int flowerRandomState = 0x9e3779b9u;
    float meleeCooldown = 0.0f;
    float meleePose = 0.0f;
    MeleeVisualState meleeVisual;
    float meleeComboWindow = 0.0f;
    int enemyAttackOwner = -1;
    float enemyAttackCadence = 0.0f;
    MultiplayerRuntimeState multiplayer;
};

class Game {
public:
    void reset();
    void restart();
    void prepareStartScreen();
    void setUiPaused(bool paused);
    void update(float dt);
    void setKey(int keyCode, bool down);
    void setTouch(int action, float x, float y, int pointerCount);
    void clearInputState();
    void setTouchControls(
        float moveX,
        float moveZ,
        float lookDeltaX,
        float lookDeltaY,
        bool vacuumHeld,
        bool sprintHeld,
        bool jumpPressed,
        bool meleePressed,
        bool shootPressed,
        bool cameraTogglePressed
    );
    void configureNetworkHost();
    void configureNetworkGuest(int localPlayerId);
    void disableNetwork();
    void setNetworkRoom(const char* code, const char* status, bool connected);
    void setNetworkPeerActive(int playerId, bool active);
    void setNetworkPeerInput(int playerId, unsigned int sequence, float moveX, float moveZ, float yaw, float pitch, unsigned short buttons);
    void applyNetworkPeerSnapshot(int playerId, const PlayerState& player, float pitch, float vacuumPower, float vacuumPose, int vacuumTarget, float meleeTimer, float dischargeAmount);

    const GameState& state() const { return state_; }
    GameState& networkMutableState() { return state_; }

private:
    enum class BatteryReason { Continuous, Jump, DoubleJump, Melee, Shoot, Hit, Climb, Ingest, NextRoom, Combo, Chain };
    GameState state_;
    int simulationPlayerId_ = 0;

    void resetRoom();
    void buildRoomColliders();
    void updateInputActions(float dt);
    void updateNetworkPeers(float dt);
    void updateNetworkGuest(float dt);
    void savePlayerContext(NetworkPeerState& context) const;
    void loadPlayerContext(const NetworkPeerState& context);
    void updateCamera(float dt);
    void updateIntroCamera(float dt);
    void updateDeathCamera(float dt);
    void updatePlayer(float dt);
    void updatePhoneGait(float dt, bool running);
    void updatePhoneActionPose(float dt, bool running, float forwardAxis, float strafeAxis);
    void updatePhoneTransform();
    void updateTargets(float dt);
    void chooseHumanWalkTarget(int index);
    Vec3 chooseHumanSpawnPoint(int index, const Vec3* avoid = nullptr) const;
    bool isHumanPointBlocked(float x, float z, float radius) const;
    void updateMeleeDash(float dt);
    void finishAirLungeLanding(float impactSpeed);
    int applyMeleeHits();
    bool damageSoulShell(int index, float amount);
    void updateVacuum(float dt);
    void updateCrosshair(float dt);
    void updateSoulLattices();
    void resetSoulLattice(TargetState& target);
    void updateBullets(float dt);
    void processPendingShots(float dt);
    void updateCaptures(float dt);
    void updateRoomPopulation(float dt);
    void updateDoorTransition();
    void updateFlowerPowerups(float dt);
    void updateParticles(float dt);
    void spawnParticleBurst(const Vec3& position);
    void spawnFlameBurst(const Vec3& position, float strength);

    float batteryDrainMultiplier() const;
    float consumeSupplementalBattery(float cost);
    bool spendBattery(float amount,BatteryReason reason=BatteryReason::Continuous);
    void gainBattery(float amount,BatteryReason reason=BatteryReason::Continuous);
    void setEnergyTicker(const char* text,int type);
    bool feedSupplementalBattery(float amount);
    void clearActivePowerups();
    void addFlowerPowerupStack(float amount);
    float nextFlowerRandom();
    void spawnFlowerPowerup(float x, float y, float z);
    void registerMeleeBatteryHit(int hitCount);
    void updateBattery(float dt);
    void triggerRunDeath();
    void emitAudio(AudioCue cue, float volume);
    void updateSlurpAudio();
    void updateBatteryAudio(float beforeValue);

    void tryJump();
    void startGroundJump();
    void startAirJump();
    void triggerMelee();
    void shootStoredSoul();
    void releaseSoul(int index);
    void captureSoul(int index);
    void queueSoulCapture(int index);
    void processQueuedSoulCaptures();
    void respawnTarget(int index);
    int activeHumanTarget() const;
    void queueHumanRespawn(const Vec3& avoid);
    void advanceRunRulesForRoom();

    float seededRoomValue(float offset) const;
    float seededRoomValue(int offset) const { return seededRoomValue(static_cast<float>(offset)); }
    int getRoomTileIndex(float z) const;
    float getRoomTileOriginZ(int tileIndex) const;
    float wrapZ(float z) const;
    float getPlayerCeilingLimit() const;
    float getPlayerSupportY(float x, float z) const;
    void resolvePlayerObstacleCollisions();
    void applyWallClimb(float dt);
    void updateRoomTopology(float previousZ, float currentZ);
    float getSegmentAabbHitT(const Vec3& from, const Vec3& to, const RoomCollider& box, float pad) const;
    void constrainThirdPersonCamera(Vec3& desired, const Vec3& lookBase) const;
    bool isInsideDoorAperture(const Vec3& position, float pad = 0.0f) const;
    void clampRoom(Vec3& pos);
    Vec3 cameraForwardFlat() const;
    Vec3 cameraRightFlat() const;
};
