#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "HumanVisual.hpp"
#include "VisualIdentity.hpp"
#include "Math.hpp"
#include "PhoneDisplay.hpp"
#include "EarlyBrowserVisuals.hpp"

constexpr int TARGET_COUNT = 32;
constexpr int CAPTURE_COUNT = 9;
constexpr int BULLET_COUNT = 30;
constexpr int FLOWER_POWERUP_COUNT = 32;
constexpr int PARTICLE_COUNT = 256;
constexpr int AUDIO_EVENT_COUNT = 64;
constexpr int ROOM_COLLIDER_COUNT = 15;
constexpr int PHONE_CAPACITY = 30;
constexpr int SOUL_LATTICE_NODE_COUNT = 27;
constexpr float PHONE_MODEL_HEIGHT = gameplay::WORLD_SCALE.phoneHeight;
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

    float lookDeltaX = 0.0f;
    float lookDeltaY = 0.0f;
    float wiggleAxis = 0.0f;
    int commSignalPressed = 0;
};

enum PlayerCommandButton : std::uint16_t {
    CommandForward = 1u << 0, CommandBack = 1u << 1,
    CommandLeft = 1u << 2, CommandRight = 1u << 3,
    CommandSprint = 1u << 4, CommandJump = 1u << 5,
    CommandVacuum = 1u << 6, CommandMelee = 1u << 7,
    CommandShoot = 1u << 8, CommandCameraToggle = 1u << 9,
    CommandWiggleLeft = 1u << 10, CommandWiggleRight = 1u << 11,
    CommandCommHelp = 1u << 12, CommandCommPing = 1u << 13,
    CommandCommGroup = 1u << 14, CommandCommOk = 1u << 15
};

// Canonical semantic input consumed by local play, host authority, prediction,
// replay, tests, and network serialization. Platform input remains InputState;
// this is the stable command boundary after keyboard/controller/touch merging.
struct PlayerCommand {
    std::uint32_t sequence = 0;
    std::uint32_t localTick = 0;
    float moveX = 0.0f;
    float moveZ = 0.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    std::uint16_t buttons = 0;
};

struct SoulRecord {
    std::uint64_t id = 0;
    bool brute = false;
    int originRoom = 0;
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
    std::array<SoulRecord, PHONE_CAPACITY> storedSouls{};
    int airJumpsRemaining = 1;
    float coyoteTimer = 0.12f;
    float jumpBufferTimer = 0.0f;
    bool alive = true;
    bool downed = false;
    float bleedoutTimer = 0.0f;
    float reviveCharge = 0.0f;
    int grabbedByTarget = -1;
    float grabEscape = 0.0f;
    int grabLastDirection = 0;
    bool soloSoulRebootUsed = false;
    bool inSecretRoom = false;
    int secretVisitRoom = -1;
    float secretVisitTimer = 0.0f;
    bool ledgeHanging = false;
    int ledgeCollider = -1;
    Vec3 ledgeNormal;
    Vec3 ledgeTangent;
    float ledgeShimmySpeed = 0.0f;
    float ledgeHangTime = 0.0f;
    float ledgeGrabCooldown = 0.0f;
    float ledgeMantleTimer = 0.0f;
    int commSignal = 0;
    float commSignalTimer = 0.0f;
};

enum class AudioCue : unsigned char {
    VcEnded, VcInvitation, ConnectPower, LowPower, NegativeAck, ReceivedMessage,
    SentMessage, PhoneAttack, PaymentSuccess, PaymentFailure, EndCallTone,
    SlurpRingtoneStart, SlurpRingtoneStop, Capture1, Capture2, Capture3, Capture4, Capture5,
    Headshot, HeadshotCritical, RewardWoah, RewardNice
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

struct EnvironmentVisualState {
    Vec3 latestShotOrigin;
    float latestShotAge = 9999.0f;
};

struct CameraState {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float verticalFovDegrees = 60.0f;
    bool firstPerson = false;
    int spectatedPlayerId = -1;
    Vec3 pos {0.0f, 1.18f, 3.0f};
    Vec3 forward {0.0f, 0.0f, -1.0f};
    Vec3 lookTarget {0.0f, 0.53f, 0.0f};
};

struct CinematicState {
    bool introActive = false;
    bool deathActive = false;
    float introElapsed = 0.0f;
    float deathElapsed = 0.0f;
    float textInteraction = 0.0f;
    float baseYaw = 0.0f;
    float attractCameraYaw = 0.0f;
    bool attractCameraYawValid = false;
    bool attractExitActive = false;
    float attractExitElapsed = 0.0f;
    bool menuEnterActive = false;
    float menuEnterElapsed = 0.0f;
    bool menuExitActive = false;
    float menuExitElapsed = 0.0f;
    Vec3 startCameraPos;
    Vec3 menuExitCameraPos;
    Vec3 menuExitLookTarget;
    float overlayFade = 0.0f;
    float restartAwaken = 0.0f;
    int deathChoice = 0;
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
    float armorRegenDelay = 0.0f;
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
    int grabbedPlayerId = -1;
    float grabCooldown = 0.0f;
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
    SoulRecord soul;
};

struct CapturePointState {
    Vec3 pos;
    bool filled = false;
    bool tokenAwarded = false;
};

struct BulletState {
    Vec3 pos;
    Vec3 vel;
    bool alive = false;
    float life = 0.0f;
    float spin = 0.0f;
    bool brute = false;
    bool depositNearMissPlayed = false;
    bool dropped = false;
    float contactCooldown = 0.0f;
    SoulRecord soul;
};

struct PendingShotState {
    bool active = false;
    bool brute = false;
    float age = 0.0f;
    SoulRecord soul;
};

struct FlowerPowerupState {
    Vec3 pos;
    Vec3 vacuumVelocity;
    float baseY = 0.38f;
    float age = 0.0f;
    float rotationY = 0.0f;
    bool vacuumAttracted = false;
    bool active = false;
};

struct ParticleState {
    Vec3 pos;
    Vec3 vel;
    float life = 0.0f;
    float maxLife = 0.0f;
    float size = 0.08f;
    unsigned char kind = 0;
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

enum class UpgradeTrack : unsigned char { Shot, Lunge, Attack, Count };

struct PermanentProgressionState {
    std::int64_t tokens = 0;
    std::array<int,3> levels{};
    std::uint64_t revision = 0;
};

struct RunProgressionState {
    std::array<int,3> temporaryLevels{};
    std::array<int,3> networkSharedPermanentLevels{};
    int accuracyStacks = 0;
    float accuracyMultiplier = 1.0f;
    float accuracyDecayTimer = 0.0f;
    float headshotRegenTax = 0.0f;
    float batteryRegenLock = 0.0f;
    float roomHeat = 0.0f;
    float roomElapsed = 0.0f;
    int roomCaptures = 0;
    int relayPrimerStacks = 0;
    float relayPrimerTimer = 0.0f;
    float impactGuardTimer = 0.0f;
    float lastStandCooldown = 0.0f;
    float lungeReboundTimer = 0.0f;
    float headshotRechargeBoost = 0.0f;
};

struct ProgressionState {
    PermanentProgressionState permanent;
    RunProgressionState run;
};

struct SecretTvState {
    int signal = 0;
    int damage = 0;
    int tolerance = 3;
    bool broken = false;
    bool available = false;
    Vec3 entrancePos {13.9f, PHONE_BODY_HEIGHT * 0.5f, 4.8f};
    Vec3 entranceNormal {-1.0f, 0.0f, 0.0f};
    float donationCooldown = 0.0f;
    float knockCueTimer = 0.0f;
    float knockVolume = 0.0f;
    float knockPulse = 0.0f;
    float knockPan = 0.0f;
};

enum class LocalMenuPage : unsigned char { Main, Online, JoinCode, Settings, Controls, Audio, Graphics };

struct LocalMenuHistoryEntry {
    LocalMenuPage page = LocalMenuPage::Main;
    int selection = 0;
    float scroll = 0.0f;
};

struct LocalSettingsState {
    static constexpr int MenuHistoryCapacity = 8;
    LocalMenuPage menuPage = LocalMenuPage::Main;
    float musicVolume = 0.70f;
    float sfxVolume = 0.55f;
    bool musicMuted = false;
    bool sfxMuted = false;
    int graphicsPreset = 1; // 0 legacy, 1 normal, 2 pretty
    bool shadows = true;
    bool portalWindow = true;
    bool particles = true;
    bool fpsCounter = false;
    float mouseLookSensitivity = 1.0f;
    float touchLookSensitivity = 1.0f;
    float controllerLookSensitivity = 1.15f;
    int controllerTriggerSensitivity = 1; // 0 deep, 1 balanced, 2 hair
    int controllerVibration = 1; // 0 off, 1 subtle, 2 strong
    bool mobileFraming = false;
    float menuScroll = 0.0f;
    std::array<LocalMenuHistoryEntry, MenuHistoryCapacity> menuHistory{};
    int menuHistoryDepth = 0;
    // GLFW key values are kept as local presentation/input preferences only.
    // They are intentionally absent from multiplayer snapshots.
    // Public desktop contract: WASD, Shift, Space, F attack, Q shoot, C camera.
    // Slot 9 is retained only for save-layout compatibility with older builds.
    std::array<int, 10> keyboardBindings{{87,83,65,68,340,32,70,81,67,0}};
    int rebindingAction = -1;
};

struct UpgradeMenuState {
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
    unsigned short actionSequence = 0;
    float visualTimer = 0.0f;
    float visualDuration = 0.20f;
    float dashTimer = 0.0f;
    float dashSpeed = 12.5f;
    bool airLungePending = false;
    bool airLungeLandingPending = false;
    bool locomotionLunge = false;
    float airLungeSpeed = 0.0f;
    float airLungeVerticalKick = 3.2f;
    float airLungeTimer = 0.0f;
    float airLungeRotation = 0.0f;
    float airLungeAngularVelocity = 0.0f;
    float airLungeCameraLag = 0.0f;
    float landingRecovery = 0.0f;
    float landingRecoveryDuration = 0.0f;
    float landingPosePitch = 0.0f;
    float wallGripTimer = 0.0f;
    Vec3 wallNormal;
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
    Vec3 previousContactPosition;
    bool contactPositionValid = false;
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

enum class RoomReviewRating : unsigned char { Keep, Tune, Redesign, Remove };

struct RoomInspectorReport {
    early_browser_visuals::RoomPremise premise=early_browser_visuals::RoomPremise::FieldOpen;
    early_browser_visuals::RoomSetting setting=early_browser_visuals::RoomSetting::Field;
    early_browser_visuals::RoomForm form=early_browser_visuals::RoomForm::Open;
    early_browser_visuals::RoomScale scale=early_browser_visuals::RoomScale::Standard;
    early_browser_visuals::RoomCondition condition=early_browser_visuals::RoomCondition::Normal;
    early_browser_visuals::RoomPlaystyle playstyle=early_browser_visuals::RoomPlaystyle::Playground;
    gameplay::TraversalDifficulty requiredBand=gameplay::TraversalDifficulty::Unknown;
    int seed=0;
    int roomIndex=0;
    int traversalSurfaceCount=0;
    int traversalEdgeCount=0;
    int requiredEdgeCount=0;
    int colliderCount=0;
    int presentationPropCount=0;
    std::array<int,static_cast<int>(early_browser_visuals::EnvironmentRole::Count)> environmentRoleCounts{};
    int enemyBudget=0;
    int enemyCount=0;
    int transparentPrimitiveCount=0;
    int visiblePrimitiveEstimate=0;
    int drawCallBucket=0;
    bool requiredRouteValid=false;
    bool seedSelectionValid=false;
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
    float crosshairOpacity = 0.0f;
    float shootJoinTimer = 0.0f;
    float supplementalFill = 0.0f;
    int flowerStacks = 0;
    bool hasTarget = false;
    bool lowBattery = false;
    bool gameOver = false;
    std::array<char,48> energyTicker{};
    float energyTickerUntil = 0.0f;
    int energyTickerType = 0;
    float headshotPulse = 0.0f;
    float perfectPulse = 0.0f;
    float headshotKillCharge = 0.0f;
    float criticalHitPulse = 0.0f;
    float critMarkerOpacity = 0.0f;
    std::array<char,32> buildLabel{};
    int menuSelection = 0;
};

constexpr int NETWORK_PLAYER_COUNT = 4;
constexpr float NETWORK_LOCAL_CORRECTION_SMOOTHING_RATE = 10.0f;
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
    bool hasWorldSnapshot = false;
    Vec3 localPredictionCorrection;
};

struct GameState {
    InputState input;
    PlayerState player;
    EnergyState energy;
    EnvironmentVisualState environmentVisual;
    CameraState camera;
    VacuumState vacuum;
    std::array<TargetState, TARGET_COUNT> targets;
    std::array<CapturePointState, CAPTURE_COUNT> captures;
    std::array<BulletState, BULLET_COUNT> bullets;
    std::array<PendingShotState, PHONE_CAPACITY> pendingShots;
    std::array<FlowerPowerupState, FLOWER_POWERUP_COUNT> flowers;
    std::array<ParticleState, PARTICLE_COUNT> particles;
    int nextParticle = 0;
    std::uint64_t nextSoulId = 1;
    AudioState audio;
    std::array<int, 5> captureSoundSlots{{0,1,2,3,4}};
    std::array<RoomCollider, ROOM_COLLIDER_COUNT> roomColliders;
    RoomTopologyState topology;
    RunRuleState runRules;
    ProgressionState progression;
    SecretTvState secretTv;
    LocalSettingsState localSettings;
    UpgradeMenuState upgradeMenu;
    std::array<HumanRespawnRequest, TARGET_COUNT> respawnQueue;
    DoorTransitionState doorTransition;
    PhonePoseState phonePose;
    PhoneTransformState phoneTransform;
    PhoneVisualState phoneVisual;
    PhoneDisplayState phoneDisplay;
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
    bool attractMode = false;
    bool traversalLab = false;
    bool roomInspector = false;
    bool roomInspectorEnemies = false;
    early_browser_visuals::RoomPremise roomInspectorPremise = early_browser_visuals::RoomPremise::FieldOpen;
    RoomInspectorReport roomInspectorReport;
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

struct HostRemotePeerSimulationIsolationAccess;
struct SoulProjectileLifecycleAccess;

class Game {
public:
    void reset();
    void restart();
    void prepareStartScreen();
    void prepareAttractScreen();
    void dismissAttractMode();
    // Local lab/exploit hook for desktop testing; not serialized into online snapshots.
    void debugStartSecretTvTest(bool enterRoom);
    void debugStartTraversalLab();
    void debugStartRoomInspector();
    void debugStepRoomInspector(int delta,bool newSeed=false);
    void debugToggleRoomInspectorEnemies();
    std::string debugRoomReviewLine(RoomReviewRating rating) const;
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
    void setWiggle(float axis);
    void setCommSignal(int signal);
    void configureNetworkHost();
    void configureNetworkGuest(int localPlayerId);
    void disableNetwork();
    void setNetworkRoom(const char* code, const char* status, bool connected);
    void setPersistentProgression(std::int64_t tokens, int shotLevel, int lungeLevel, int attackLevel);
    bool chooseTemporaryUpgrade(int track);
    bool purchasePermanentUpgrade(int track);
    void setNetworkPeerActive(int playerId, bool active);
    PlayerCommand capturePlayerCommand(unsigned int sequence, unsigned int localTick) const;
    void setNetworkPeerCommand(int playerId, const PlayerCommand& command);
    void setNetworkPeerInput(int playerId, unsigned int sequence, float moveX, float moveZ, float yaw, float pitch, unsigned short buttons);
    void applyNetworkPeerSnapshot(int playerId, const PlayerState& player, float pitch, float vacuumPower, float vacuumPose, int vacuumTarget, float meleeTimer, float dischargeAmount);

    const GameState& state() const { return state_; }
    GameState& networkMutableState() { return state_; }

private:
    friend struct HostRemotePeerSimulationIsolationAccess;
    friend struct SoulProjectileLifecycleAccess;
    enum class BatteryReason { Continuous, Jump, DoubleJump, Melee, Shoot, Hit, Climb, Ingest, NextRoom, Combo, Chain, Headshot, Loop };
    GameState state_;
    int simulationPlayerId_ = 0;

    void resetRoom();
    void buildRoomColliders();
    void chooseSecretTvEntrance();
    void updateInputActions(float dt);
    void updateNetworkPeers(float dt);
    void updateTeamRevival(float dt);
    void updateTargetGrab(int targetIndex, float dt);
    void releaseTargetGrab(int targetIndex);
    void updateSecretTv(float dt);
    void updateNetworkGuest(float dt);
    void savePlayerContext(NetworkPeerState& context) const;
    void loadPlayerContext(const NetworkPeerState& context);
    void updateCamera(float dt);
    void updateAttractInput(float dt);
    void updateIntroCamera(float dt);
    void updateDeathCamera(float dt);
    void updatePlayer(float dt);
    bool tryBeginLedgeHang();
    bool updateLedgeHang(float dt, float forwardAxis, float strafeAxis);
    void releaseLedgeHang(bool mantle);
    void updatePhoneGait(float dt, bool running);
    void updatePhoneActionPose(float dt, bool running, float forwardAxis, float strafeAxis);
    void updatePhoneTransform();
    void updatePhoneDisplay(float dt);
    void updateTargets(float dt);
    void chooseHumanWalkTarget(int index);
    Vec3 chooseHumanSpawnPoint(int index, const Vec3* avoid = nullptr) const;
    bool isHumanPointBlocked(float x, float z, float radius) const;
    void updateMeleeDash(float dt);
    void finishAirLungeLanding(float impactSpeed);
    int applyMeleeHits();
    bool damageSoulShell(int index, float amount);
    Vec3 targetHeadCenter(const TargetState& target) const;
    float headshotDamage(const TargetState& target) const;
    void continueLungeFromHeadshot();
    void rewardHeadshot(const Vec3& position, bool critical, bool fromLunge, float enemyAttackProgress, float killCharge);
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
    void refreshRoomInspectorReport(bool seedSelectionValid=true);
    void updateParticles(float dt);
    void spawnParticleBurst(const Vec3& position);
    void spawnFlameBurst(const Vec3& position, float strength);
    void spawnShellShatter(const TargetState& target);

    SoulRecord makeSoulRecord(bool brute, int originRoom);
    bool storeSoul(PlayerState& player, const SoulRecord& soul);

    float batteryDrainMultiplier() const;
    int upgradeLevel(UpgradeTrack track) const;
    int pairSynergyTier(UpgradeTrack a, UpgradeTrack b) const;
    int survivalSynergyTier() const;
    float outgoingDamageMultiplier() const;
    void updateBuildLabel();
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
    void updateRunProgressionTimers(float dt);
    void triggerRunDeath();
    void clearPlayerLifecycleActions();
    void emitAudio(AudioCue cue, float volume);
    void updateSlurpAudio();
    void updateBatteryAudio(float beforeValue);

    void tryJump();
    void startGroundJump();
    void startAirJump();
    void triggerMelee(bool authoritativeDamage = true);
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
    void resolveDoorwayCollisions(float previousX, float previousZ);
    void applyWallClimb(float dt);
    void updateRoomTopology(float previousZ, float currentZ);
    void chargeClosedDoorLoop();
    void awardGoalToken(CapturePointState& capture);
    float getSegmentAabbHitT(const Vec3& from, const Vec3& to, const RoomCollider& box, float pad) const;
    void constrainThirdPersonCamera(Vec3& desired, const Vec3& lookBase) const;
    bool isInsideDoorAperture(const Vec3& position, float pad = 0.0f) const;
    void clampRoom(Vec3& pos);
    Vec3 cameraForwardFlat() const;
    Vec3 cameraRightFlat() const;
    Vec3 assistedActionDirection(const Vec3& origin, const Vec3& direction, float maxDistance, float minDot, float maxBlend, bool preferHead) const;
};
