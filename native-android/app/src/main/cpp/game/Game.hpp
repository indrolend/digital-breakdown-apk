#pragma once

#include <array>

#include "HumanVisual.hpp"
#include "VisualIdentity.hpp"
#include "Math.hpp"

constexpr int TARGET_COUNT = 32;
constexpr int CAPTURE_COUNT = 5;
constexpr int BULLET_COUNT = 30;
constexpr int ROOM_COLLIDER_COUNT = 15;
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
    int airJumpsRemaining = 1;
    float coyoteTimer = 0.12f;
    float jumpBufferTimer = 0.0f;
    bool alive = true;
};

struct CameraState {
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool firstPerson = false;
    Vec3 pos {0.0f, 1.18f, 3.0f};
    Vec3 forward {0.0f, 0.0f, -1.0f};
    Vec3 lookTarget {0.0f, 0.53f, 0.0f};
};

struct VacuumState {
    bool active = false;
    float power = 0.0f;
    float pose = 0.0f;
    float fieldStrength = 0.0f;
    float coneTightness = 0.0f;
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
    float respawnTimer = 0.0f;
    float scale = 1.0f;
    float phase = 0.0f;
    float visualYaw = 0.0f;
    float visualWalkPhase = 0.0f;
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
    HumanReactionVisual visualReaction;
    bool brute = false;
    SoulState soulState = SoulState::Free;
    SoulVisualState soulVisual;
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

struct MeleeVisualState {
    float visualTimer = 0.0f;
    float visualDuration = 0.20f;
    float dashTimer = 0.0f;
    float dashSpeed = 12.5f;
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

struct GameState {
    InputState input;
    PlayerState player;
    CameraState camera;
    VacuumState vacuum;
    std::array<TargetState, TARGET_COUNT> targets;
    std::array<CapturePointState, CAPTURE_COUNT> captures;
    std::array<BulletState, BULLET_COUNT> bullets;
    std::array<RoomCollider, ROOM_COLLIDER_COUNT> roomColliders;
    RoomTopologyState topology;
    PhonePoseState phonePose;
    PhoneVisualState phoneVisual;
    PlayerDebugState debug;
    float time = 0.0f;
    int frame = 0;
    int roomIndex = 1;
    int roomSeed = 73452;
    bool roomClear = false;
    float meleeCooldown = 0.0f;
    float meleePose = 0.0f;
    MeleeVisualState meleeVisual;
    float meleeComboWindow = 0.0f;
};

class Game {
public:
    void reset();
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

    const GameState& state() const { return state_; }

private:
    GameState state_;

    void resetRoom();
    void buildRoomColliders();
    void updateInputActions(float dt);
    void updateCamera(float dt);
    void updatePlayer(float dt);
    void updatePhoneGait(float dt, bool running);
    void updatePhoneActionPose(float dt, bool running, float forwardAxis, float strafeAxis);
    void updateTargets(float dt);
    void chooseHumanWalkTarget(int index);
    bool isHumanPointBlocked(float x, float z, float radius) const;
    void updateMeleeDash(float dt);
    void updateVacuum(float dt);
    void updateBullets(float dt);
    void updateCaptures(float dt);

    void tryJump();
    void startGroundJump();
    void startAirJump();
    void triggerMelee();
    void shootStoredSoul();
    void releaseSoul(int index);
    void captureSoul(int index);
    void respawnTarget(int index);

    float seededRoomValue(int offset) const;
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
