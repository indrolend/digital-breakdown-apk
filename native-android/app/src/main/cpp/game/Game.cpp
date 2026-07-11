#include "Game.hpp"

#include <cmath>

namespace {
constexpr float ROOM_WIDTH = 30.0f;
constexpr float ROOM_DEPTH = 42.0f;
constexpr float GROUND_Y = 0.55f;

// Movement values are ported from Data-game-chicken-animation-split-pass7(4).
constexpr float WALK_ACCEL = 16.0f;
constexpr float RUN_ACCEL = 42.0f;
constexpr float WALK_MAX_SPEED = 18.0f;
constexpr float RUN_MAX_SPEED = 42.0f;
constexpr float GROUND_FRICTION = 0.88f;
constexpr float AIR_FRICTION = 0.985f;
constexpr float AIR_ACCEL_MULT = 0.62f;
constexpr float AIR_MAX_SPEED_MULT = 1.08f;
constexpr float GRAVITY = 14.0f;
constexpr float JUMP_SPEED = 4.5f;
constexpr float AIR_JUMP_SPEED = 4.25f;
constexpr float COYOTE_TIME = 0.12f;
constexpr float JUMP_BUFFER = 0.12f;
constexpr float LANDING_MOMENTUM_BOOST = 1.04f;

constexpr float BATTERY_MAX = 100.0f;
constexpr float BATTERY_IDLE_REGEN = 22.0f;
constexpr float BATTERY_ACTIVE_REGEN = 3.0f;
constexpr float BATTERY_WALK_DRAIN = 0.45f;
constexpr float BATTERY_SPRINT_DRAIN = 3.0f;
constexpr float BATTERY_AIR_DRAIN = 0.9f;
constexpr float BATTERY_VACUUM_DRAIN = 1.35f;
constexpr float BATTERY_JUMP_COST = 4.5f;
constexpr float BATTERY_DOUBLE_JUMP_COST = 6.5f;
constexpr float BATTERY_MELEE_COST = 6.0f;
constexpr float BATTERY_CAPTURE_GAIN = 6.0f;

constexpr float VACUUM_MOVE_MULT = 0.35f;
constexpr float VACUUM_CHARGE_SPEED = 3.5f;
constexpr float VACUUM_DECAY_SPEED = 6.0f;
constexpr float VACUUM_RANGE = 15.5f;
constexpr float VACUUM_LATCH_RADIUS = 1.75f;
constexpr float VACUUM_CAPTURE_TIME = 0.72f;
constexpr float VACUUM_PULL = 13.5f;

constexpr float MELEE_RANGE = 2.35f;
constexpr float MELEE_RADIUS = 1.15f;
constexpr float MELEE_COOLDOWN = 0.28f;
constexpr float MELEE_DAMAGE = 1.0f;
constexpr float BULLET_SPEED = 14.0f;
constexpr float BULLET_LIFE = 3.0f;
constexpr int MAX_STORED_SOULS = 5;

constexpr int KEY_W = 51;
constexpr int KEY_A = 29;
constexpr int KEY_S = 47;
constexpr int KEY_D = 32;
constexpr int KEY_Q = 45;
constexpr int KEY_C = 31;
constexpr int KEY_F = 34;
constexpr int KEY_SHIFT_LEFT = 59;
constexpr int KEY_SHIFT_RIGHT = 60;
constexpr int KEY_SPACE = 62;

constexpr int TOUCH_DOWN = 0;
constexpr int TOUCH_UP = 1;
constexpr int TOUCH_MOVE = 2;
constexpr int TOUCH_CANCEL = 3;

float batteryPower(const PlayerState& player) {
    return clampf(0.35f + player.battery / BATTERY_MAX * 0.65f, 0.35f, 1.0f);
}

float distXZ(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}
}

void Game::reset() {
    state_ = GameState{};
    resetRoom();
}

void Game::resetRoom() {
    state_.roomClear = false;
    state_.player = PlayerState{};
    state_.camera = CameraState{};
    state_.vacuum = VacuumState{};

    const Vec3 spots[TARGET_COUNT] = {
        {-8.0f, GROUND_Y, -12.0f}, {8.0f, GROUND_Y, -13.0f}, {-9.0f, GROUND_Y, 0.0f},
        {9.0f, GROUND_Y, -1.0f}, {0.0f, GROUND_Y, -17.0f}, {-4.5f, GROUND_Y, -6.0f},
        {4.5f, GROUND_Y, -7.5f}, {-11.0f, GROUND_Y, 8.0f}, {11.0f, GROUND_Y, 7.5f},
        {-3.0f, GROUND_Y, 13.0f}, {3.0f, GROUND_Y, 13.0f}, {0.0f, GROUND_Y, -2.0f}
    };

    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& target = state_.targets[i];
        target = TargetState{};
        target.pos = spots[i];
        target.alive = i < 8;
        target.slurpable = false;
        target.armor = (i == 4) ? 2.0f : 1.0f;
        target.scale = (i == 4) ? 1.55f : 1.0f;
        target.phase = static_cast<float>(i) * 0.77f;
    }

    const Vec3 captureSpots[CAPTURE_COUNT] = {
        {-8.0f, 3.6f, -20.35f},
        {-4.0f, 2.45f, -20.35f},
        {0.0f, 3.6f, -20.35f},
        {4.0f, 2.45f, -20.35f},
        {8.0f, 3.6f, -20.35f}
    };
    for (int i = 0; i < CAPTURE_COUNT; ++i) {
        state_.captures[i] = CapturePointState{};
        state_.captures[i].pos = captureSpots[i];
    }

    for (auto& bullet : state_.bullets) bullet = BulletState{};
}

void Game::setKey(int keyCode, bool down) {
    InputState& input = state_.input;

    if (keyCode == KEY_W) input.forward = down;
    if (keyCode == KEY_S) input.back = down;
    if (keyCode == KEY_A) input.left = down;
    if (keyCode == KEY_D) input.right = down;
    if (keyCode == KEY_SHIFT_LEFT || keyCode == KEY_SHIFT_RIGHT) input.sprint = down;

    if (keyCode == KEY_SPACE) {
        if (down && !input.jumpHeld) input.jumpPressed = true;
        input.jumpHeld = down;
    }
    if (keyCode == KEY_F && down) input.meleePressed = true;
    if (keyCode == KEY_Q && down) input.shootPressed = true;
    if (keyCode == KEY_C && down) input.cameraTogglePressed = true;
}

void Game::setTouch(int action, float x, float y, int pointerCount) {
    (void)pointerCount;
    InputState& input = state_.input;

    if (action == TOUCH_DOWN) {
        input.touching = true;
        input.primaryHeld = true;
        input.touchX = input.lastTouchX = x;
        input.touchY = input.lastTouchY = y;
    } else if (action == TOUCH_MOVE) {
        input.lookDeltaX += x - input.lastTouchX;
        input.lookDeltaY += y - input.lastTouchY;
        input.touchX = x;
        input.touchY = y;
        input.lastTouchX = x;
        input.lastTouchY = y;
    } else if (action == TOUCH_UP || action == TOUCH_CANCEL) {
        input.touching = false;
        input.primaryHeld = false;
    }
}

void Game::setTouchControls(
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
) {
    InputState& input = state_.input;
    input.touchMoveX = clampf(moveX, -1.0f, 1.0f);
    input.touchMoveZ = clampf(moveZ, -1.0f, 1.0f);
    input.lookDeltaX += lookDeltaX;
    input.lookDeltaY += lookDeltaY;
    input.touchPrimaryHeld = vacuumHeld;
    input.touchSprint = sprintHeld;
    if (jumpPressed) input.jumpPressed = true;
    if (meleePressed) input.meleePressed = true;
    if (shootPressed) input.shootPressed = true;
    if (cameraTogglePressed) input.cameraTogglePressed = true;
}

void Game::update(float dt) {
    dt = clampf(dt, 0.0f, 0.033f);
    state_.time += dt;
    state_.frame += 1;

    updateInputActions(dt);
    updateCamera(dt);
    updatePlayer(dt);
    updateTargets(dt);
    updateVacuum(dt);
    updateBullets(dt);
    updateCaptures(dt);
}

void Game::updateInputActions(float dt) {
    InputState& input = state_.input;
    GameState& s = state_;

    if (input.cameraTogglePressed) {
        s.camera.firstPerson = !s.camera.firstPerson;
    }
    if (input.jumpPressed) {
        s.player.jumpBufferTimer = JUMP_BUFFER;
        tryJump();
    }
    if (input.meleePressed) triggerMelee();
    if (input.shootPressed) shootStoredSoul();

    input.cameraTogglePressed = false;
    input.jumpPressed = false;
    input.meleePressed = false;
    input.shootPressed = false;

    s.meleeCooldown = std::max(0.0f, s.meleeCooldown - dt);
    s.meleePose = std::max(0.0f, s.meleePose - dt * 5.5f);

    s.vacuum.active = (input.primaryHeld || input.touchPrimaryHeld) && s.player.battery > 1.0f;
}

Vec3 Game::cameraForwardFlat() const {
    return normalized({-std::sin(state_.camera.yaw), 0.0f, -std::cos(state_.camera.yaw)});
}

Vec3 Game::cameraRightFlat() const {
    return normalized({std::cos(state_.camera.yaw), 0.0f, -std::sin(state_.camera.yaw)});
}

void Game::updateCamera(float dt) {
    InputState& input = state_.input;
    CameraState& camera = state_.camera;
    const PlayerState& player = state_.player;

    camera.yaw -= input.lookDeltaX * 0.0065f;
    camera.pitch = clampf(camera.pitch - input.lookDeltaY * 0.0045f, -0.85f, 0.85f);
    input.lookDeltaX = 0.0f;
    input.lookDeltaY = 0.0f;

    const float cp = std::cos(camera.pitch);
    camera.forward = normalized({-std::sin(camera.yaw) * cp, std::sin(camera.pitch), -std::cos(camera.yaw) * cp});

    if (camera.firstPerson) {
        camera.pos = player.pos + Vec3{0.0f, 0.72f, 0.0f} + camera.forward * 0.18f;
        return;
    }

    const Vec3 desired = player.pos - camera.forward * 3.0f + Vec3{0.0f, 1.1f, 0.0f};
    const float t = clampf(dt * 8.0f, 0.0f, 1.0f);
    camera.pos.x += (desired.x - camera.pos.x) * t;
    camera.pos.y += (desired.y - camera.pos.y) * t;
    camera.pos.z += (desired.z - camera.pos.z) * t;
    camera.pos.y = std::max(camera.pos.y, GROUND_Y + 0.8f);
}

void Game::tryJump() {
    PlayerState& player = state_.player;
    if (player.grounded || player.coyoteTimer > 0.0f) {
        startGroundJump();
    } else if (player.airJumpsRemaining > 0 && player.battery >= BATTERY_DOUBLE_JUMP_COST) {
        startAirJump();
    }
}

void Game::startGroundJump() {
    PlayerState& player = state_.player;
    if (player.battery < BATTERY_JUMP_COST) return;
    player.battery -= BATTERY_JUMP_COST;
    player.jumpVel = JUMP_SPEED;
    player.grounded = false;
    player.coyoteTimer = 0.0f;
    player.jumpBufferTimer = 0.0f;
    player.airJumpsRemaining = 1;
}

void Game::startAirJump() {
    PlayerState& player = state_.player;
    player.battery -= BATTERY_DOUBLE_JUMP_COST;
    player.jumpVel = AIR_JUMP_SPEED;
    player.jumpBufferTimer = 0.0f;
    player.airJumpsRemaining -= 1;
}

void Game::updatePlayer(float dt) {
    InputState& input = state_.input;
    PlayerState& player = state_.player;

    if (player.jumpBufferTimer > 0.0f) player.jumpBufferTimer = std::max(0.0f, player.jumpBufferTimer - dt);
    if (player.grounded) player.coyoteTimer = COYOTE_TIME;
    else player.coyoteTimer = std::max(0.0f, player.coyoteTimer - dt);

    float forwardAxis = (input.forward ? 1.0f : 0.0f) - (input.back ? 1.0f : 0.0f);
    float strafeAxis = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f);
    forwardAxis = clampf(forwardAxis + input.touchMoveZ, -1.0f, 1.0f);
    strafeAxis = clampf(strafeAxis + input.touchMoveX, -1.0f, 1.0f);

    const bool moving = std::abs(forwardAxis) > 0.001f || std::abs(strafeAxis) > 0.001f;
    const bool running = (input.sprint || input.touchSprint) && moving && !state_.vacuum.active && player.battery > 5.0f;

    const float power = batteryPower(player);
    const float airControl = player.grounded ? 1.0f : AIR_ACCEL_MULT;
    const float airSpeed = player.grounded ? 1.0f : AIR_MAX_SPEED_MULT;
    const float accel = (running ? RUN_ACCEL : WALK_ACCEL) * power * airControl;
    const float maxSpeed = (running ? RUN_MAX_SPEED : WALK_MAX_SPEED) * power * airSpeed;
    const float vacuumSlow = 1.0f - state_.vacuum.pose * (1.0f - VACUUM_MOVE_MULT);

    Vec3 move{0.0f, 0.0f, 0.0f};
    const Vec3 forward = cameraForwardFlat();
    const Vec3 right = cameraRightFlat();
    move += forward * forwardAxis;
    move += right * strafeAxis;
    if (lengthSq(move) > 0.0001f) {
        move = normalized(move);
        player.vel += move * (accel * vacuumSlow * dt);
        player.targetYaw = std::atan2(-move.x, -move.z);
    }

    limitHorizontal(player.vel, maxSpeed * vacuumSlow);

    if (!player.grounded) {
        player.jumpVel -= GRAVITY * dt;
        player.pos.y += player.jumpVel * dt;
        if (player.pos.y <= GROUND_Y) {
            player.pos.y = GROUND_Y;
            player.jumpVel = 0.0f;
            player.grounded = true;
            player.coyoteTimer = COYOTE_TIME;
            player.airJumpsRemaining = 1;
            if (horizontalLength(player.vel) > 1.2f) player.vel *= LANDING_MOMENTUM_BOOST;
        }
    }

    if (player.grounded && player.jumpBufferTimer > 0.0f) startGroundJump();

    player.pos += player.vel * dt;
    clampRoom(player.pos);

    const float friction = player.grounded ? GROUND_FRICTION : AIR_FRICTION;
    player.vel *= std::pow(friction, dt * 60.0f);
    player.yaw = approachAngle(player.yaw, player.targetYaw, dt * 10.0f);

    float drain = 0.0f;
    if (moving) drain += BATTERY_WALK_DRAIN;
    if (running) drain += BATTERY_SPRINT_DRAIN;
    if (!player.grounded) drain += BATTERY_AIR_DRAIN;
    if (state_.vacuum.active) drain += BATTERY_VACUUM_DRAIN;
    const float regen = drain > 0.0f ? BATTERY_ACTIVE_REGEN : BATTERY_IDLE_REGEN;
    player.battery = clampf(player.battery + (regen - drain) * dt, 0.0f, BATTERY_MAX);
}

void Game::triggerMelee() {
    GameState& s = state_;
    PlayerState& player = s.player;
    if (s.meleeCooldown > 0.0f || player.battery < BATTERY_MELEE_COST) return;
    player.battery -= BATTERY_MELEE_COST;
    s.meleeCooldown = MELEE_COOLDOWN;
    s.meleePose = 1.0f;

    const Vec3 forward = cameraForwardFlat();
    for (auto& target : s.targets) {
        if (!target.alive) continue;
        const Vec3 toTarget = target.pos - player.pos;
        const float forwardDist = toTarget.x * forward.x + toTarget.z * forward.z;
        const float sideSq = (toTarget.x * toTarget.x + toTarget.z * toTarget.z) - forwardDist * forwardDist;
        if (forwardDist > 0.0f && forwardDist < MELEE_RANGE && sideSq < MELEE_RADIUS * MELEE_RADIUS) {
            target.armor -= MELEE_DAMAGE;
            target.vel += forward * 4.2f;
            target.vel.y = 1.2f;
            if (target.armor <= 0.0f) target.slurpable = true;
        }
    }
}

void Game::shootStoredSoul() {
    GameState& s = state_;
    if (s.player.souls <= 0) return;
    for (auto& bullet : s.bullets) {
        if (bullet.alive) continue;
        bullet.alive = true;
        bullet.life = BULLET_LIFE;
        bullet.pos = s.player.pos + Vec3{0.0f, 0.75f, 0.0f} + s.camera.forward * 0.75f;
        bullet.vel = s.camera.forward * BULLET_SPEED;
        s.player.souls -= 1;
        s.vacuum.active = false;
        return;
    }
}

void Game::updateTargets(float dt) {
    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& target = state_.targets[i];
        if (!target.alive) continue;

        target.phase += dt;
        if (!target.slurpable) {
            target.vel.x += std::sin(target.phase * 0.7f + static_cast<float>(i)) * 0.55f * dt;
            target.vel.z += std::cos(target.phase * 0.55f + static_cast<float>(i)) * 0.55f * dt;
        }

        target.vel.y -= GRAVITY * 0.35f * dt;
        target.pos += target.vel * dt;
        if (target.pos.y <= GROUND_Y) {
            target.pos.y = GROUND_Y;
            target.vel.y = 0.0f;
        }
        target.vel *= std::max(0.0f, 1.0f - 4.5f * dt);
        clampRoom(target.pos);
    }
}

void Game::updateVacuum(float dt) {
    GameState& s = state_;
    s.vacuum.pose += ((s.vacuum.active ? 1.0f : 0.0f) - s.vacuum.pose) * clampf(dt * 10.0f, 0.0f, 1.0f);
    if (s.vacuum.active) s.vacuum.power = clampf(s.vacuum.power + VACUUM_CHARGE_SPEED * dt, 0.0f, 1.0f);
    else s.vacuum.power = clampf(s.vacuum.power - VACUUM_DECAY_SPEED * dt, 0.0f, 1.0f);

    s.vacuum.target = -1;
    if (!s.vacuum.active || s.vacuum.power < 0.32f || s.player.souls >= MAX_STORED_SOULS) return;

    const Vec3 pullPoint = s.player.pos + Vec3{0.0f, 0.72f, 0.0f} + s.camera.forward * 0.35f;
    float bestScore = 99999.0f;
    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& target = s.targets[i];
        if (!target.alive || !target.slurpable) continue;
        Vec3 toPull = pullPoint - target.pos;
        const float d = length(toPull);
        if (d > VACUUM_RANGE) continue;
        const Vec3 dirToTarget = normalized(target.pos - pullPoint);
        const float facing = dirToTarget.x * s.camera.forward.x + dirToTarget.y * s.camera.forward.y + dirToTarget.z * s.camera.forward.z;
        if (facing < -0.25f) continue;
        const float score = d - (d < VACUUM_LATCH_RADIUS ? 3.5f : 0.0f);
        if (score < bestScore) {
            bestScore = score;
            s.vacuum.target = i;
        }
    }

    if (s.vacuum.target < 0) return;
    TargetState& target = s.targets[s.vacuum.target];
    Vec3 toPull = pullPoint - target.pos;
    const float d = std::max(length(toPull), 0.001f);
    const Vec3 dir = toPull * (1.0f / d);
    const float close = 1.0f - clampf(d / VACUUM_RANGE, 0.0f, 1.0f);
    const float speed = VACUUM_PULL * s.vacuum.power * (0.45f + close * 1.4f);
    target.pos += dir * std::min(d, speed * dt);
    target.vel = {0.0f, 0.0f, 0.0f};

    if (d < VACUUM_LATCH_RADIUS) {
        target.capture += dt / VACUUM_CAPTURE_TIME;
        if (target.capture >= 1.0f) {
            target.alive = false;
            target.capture = 0.0f;
            s.player.souls = std::min(MAX_STORED_SOULS, s.player.souls + 1);
            s.player.battery = std::min(BATTERY_MAX, s.player.battery + 3.0f);
        }
    } else {
        target.capture = std::max(0.0f, target.capture - dt * 1.5f);
    }
}

void Game::updateBullets(float dt) {
    for (auto& bullet : state_.bullets) {
        if (!bullet.alive) continue;
        bullet.life -= dt;
        bullet.pos += bullet.vel * dt;
        if (bullet.life <= 0.0f || std::abs(bullet.pos.x) > ROOM_WIDTH || std::abs(bullet.pos.z) > ROOM_DEPTH) {
            bullet.alive = false;
            continue;
        }

        // The web game completes rooms by shooting stored souls into five
        // wall-mounted goals. Goals get first claim on a projectile so a target
        // standing in front of the wall cannot steal a valid slot hit.
        for (auto& capture : state_.captures) {
            if (capture.filled) continue;
            const Vec3 delta = bullet.pos - capture.pos;
            if (std::abs(delta.x) < 0.9f &&
                std::abs(delta.y) < 0.9f &&
                std::abs(delta.z) < 0.75f) {
                capture.filled = true;
                state_.player.battery = std::min(
                    BATTERY_MAX,
                    state_.player.battery + BATTERY_CAPTURE_GAIN
                );
                bullet.alive = false;
                break;
            }
        }
        if (!bullet.alive) continue;

        for (auto& target : state_.targets) {
            if (!target.alive) continue;
            if (distXZ(bullet.pos, target.pos) < 0.85f && std::abs(bullet.pos.y - target.pos.y) < 1.6f) {
                target.slurpable = true;
                target.armor = 0.0f;
                target.vel += normalized(target.pos - state_.player.pos) * 3.5f;
                bullet.alive = false;
                break;
            }
        }
    }
}

void Game::updateCaptures(float dt) {
    (void)dt;
    int filled = 0;
    for (const auto& capture : state_.captures) {
        if (capture.filled) filled += 1;
    }
    if (!state_.roomClear && filled >= CAPTURE_COUNT) {
        state_.roomClear = true;
        state_.roomIndex += 1;
        resetRoom();
    }
}

void Game::clampRoom(Vec3& pos) {
    constexpr float pad = 0.8f;
    pos.x = clampf(pos.x, -ROOM_WIDTH * 0.5f + pad, ROOM_WIDTH * 0.5f - pad);
    pos.z = clampf(pos.z, -ROOM_DEPTH * 0.5f + pad, ROOM_DEPTH * 0.5f - pad);
}
