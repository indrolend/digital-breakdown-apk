#pragma once

#include <array>

#include "Math.hpp"

constexpr int TARGET_COUNT = 12;
constexpr int CAPTURE_COUNT = 3;
constexpr int BULLET_COUNT = 8;

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
    Vec3 pos {0.0f, 0.55f, 12.0f};
    Vec3 vel {0.0f, 0.0f, 0.0f};
    float jumpVel = 0.0f;
    float yaw = DB_PI;
    float targetYaw = DB_PI;
    bool grounded = true;
    float battery = 100.0f;
    int souls = 0;
    int airJumpsRemaining = 1;
    float coyoteTimer = 0.0f;
    float jumpBufferTimer = 0.0f;
    bool alive = true;
};

struct CameraState {
    float yaw = DB_PI;
    float pitch = 0.28f;
    bool firstPerson = false;
    Vec3 pos {0.0f, 2.0f, 7.0f};
    Vec3 forward {0.0f, 0.0f, -1.0f};
};

struct VacuumState {
    bool active = false;
    float power = 0.0f;
    float pose = 0.0f;
    int target = -1;
};

struct TargetState {
    Vec3 pos;
    Vec3 vel;
    bool alive = false;
    bool slurpable = false;
    float armor = 1.0f;
    float capture = 0.0f;
    float scale = 1.0f;
    float phase = 0.0f;
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

struct GameState {
    InputState input;
    PlayerState player;
    CameraState camera;
    VacuumState vacuum;
    std::array<TargetState, TARGET_COUNT> targets;
    std::array<CapturePointState, CAPTURE_COUNT> captures;
    std::array<BulletState, BULLET_COUNT> bullets;
    float time = 0.0f;
    int frame = 0;
    int roomIndex = 1;
    bool roomClear = false;
    float meleeCooldown = 0.0f;
    float meleePose = 0.0f;
};

class Game {
public:
    void reset();
    void update(float dt);
    void setKey(int keyCode, bool down);
    void setTouch(int action, float x, float y, int pointerCount);
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
    void updateInputActions(float dt);
    void updateCamera(float dt);
    void updatePlayer(float dt);
    void updateTargets(float dt);
    void updateVacuum(float dt);
    void updateBullets(float dt);
    void updateCaptures(float dt);

    void tryJump();
    void startGroundJump();
    void startAirJump();
    void triggerMelee();
    void shootStoredSoul();
    void clampRoom(Vec3& pos);
    Vec3 cameraForwardFlat() const;
    Vec3 cameraRightFlat() const;
};
