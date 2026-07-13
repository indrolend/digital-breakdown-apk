#include "Game.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr float ROOM_WIDTH = 30.0f;
constexpr float ROOM_DEPTH = 42.0f;
constexpr float GROUND_Y = 0.55f;

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
constexpr float BATTERY_DOUBLE_JUMP_COST = 6.0f;
constexpr float BATTERY_MELEE_COST = 4.0f;
constexpr float BATTERY_CAPTURE_GAIN = 18.0f;

constexpr float VACUUM_MOVE_MULT = 0.35f;
constexpr float VACUUM_CHARGE_SPEED = 3.5f;
constexpr float VACUUM_DECAY_SPEED = 6.0f;
constexpr float SOUL_ATTRACTION_RANGE = 15.5f;
constexpr float SOUL_ATTRACTION_CONE_DOT = 0.10f;
constexpr float SOUL_LATCH_DISTANCE = 0.48f;
constexpr float SOUL_SEAL_DISTANCE = 0.14f;
constexpr float SOUL_RECOIL_DURATION = 0.55f;
constexpr float SOUL_CAPTURE_COMMIT_PHASE = 0.92f;
constexpr float SOUL_CAPTURE_DECAY = 0.75f;

constexpr float MELEE_RANGE = 2.85f;
constexpr float MELEE_RADIUS = 0.88f;
constexpr float MELEE_COOLDOWN = 0.34f;
constexpr float MELEE_DAMAGE = 1.0f;
constexpr float BULLET_SPEED = 25.0f;
constexpr float BULLET_GRAVITY = 11.5f;
constexpr float BULLET_LIFE = 3.25f;
constexpr int MAX_STORED_SOULS = 30;
constexpr int ACTIVE_HUMAN_TARGET = 5;

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

float dot3(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float smooth01(float value) {
    const float t = clampf(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float deterministic01(int index, int room, float salt) {
    const float n = std::sin(static_cast<float>(index) * 12.9898f + static_cast<float>(room) * 78.233f + salt) * 43758.5453f;
    return n - std::floor(n);
}
}

void Game::reset() {
    state_ = GameState{};
    resetRoom();
}

void Game::resetRoom() {
    const int roomIndex = state_.roomIndex;
    state_.roomClear = false;
    state_.player = PlayerState{};
    state_.camera = CameraState{};
    state_.vacuum = VacuumState{};
    state_.roomIndex = roomIndex;

    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& target = state_.targets[i];
        target = TargetState{};
        const int column = i % 6;
        const int row = i / 6;
        target.pos = {
            -10.0f + static_cast<float>(column) * 4.0f,
            GROUND_Y,
            -14.0f + static_cast<float>(row) * 5.0f
        };
        target.pos.x += (deterministic01(i, roomIndex, 1.0f) - 0.5f) * 1.5f;
        target.pos.z += (deterministic01(i, roomIndex, 2.0f) - 0.5f) * 1.5f;
        target.alive = i < ACTIVE_HUMAN_TARGET;
        target.slurpable = false;
        target.armor = 2.0f;
        target.health = 1.0f;
        target.scale = 1.0f;
        target.phase = static_cast<float>(i) * 0.77f;
        target.soulState = SoulState::Free;
        if (!target.alive) {
            target.respawnTimer = 1.45f + deterministic01(i, roomIndex, 3.0f) * 0.9f;
        }
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
    if (input.cameraTogglePressed) state_.camera.firstPerson = !state_.camera.firstPerson;
    if (input.jumpPressed) {
        state_.player.jumpBufferTimer = JUMP_BUFFER;
        tryJump();
    }
    if (input.meleePressed) triggerMelee();
    if (input.shootPressed) shootStoredSoul();

    input.cameraTogglePressed = false;
    input.jumpPressed = false;
    input.meleePressed = false;
    input.shootPressed = false;

    state_.meleeCooldown = std::max(0.0f, state_.meleeCooldown - dt);
    state_.meleePose = std::max(0.0f, state_.meleePose - dt * 5.5f);
    state_.vacuum.active = (input.primaryHeld || input.touchPrimaryHeld) && state_.player.battery > 1.0f;
    if (state_.player.souls >= MAX_STORED_SOULS) state_.vacuum.active = false;
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
    camera.pos += (desired - camera.pos) * t;
    camera.pos.y = std::max(camera.pos.y, GROUND_Y + 0.8f);
}

void Game::tryJump() {
    PlayerState& player = state_.player;
    if (player.grounded || player.coyoteTimer > 0.0f) startGroundJump();
    else if (player.airJumpsRemaining > 0 && player.battery >= BATTERY_DOUBLE_JUMP_COST) startAirJump();
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
    const float accel = (running ? RUN_ACCEL : WALK_ACCEL) * power * (player.grounded ? 1.0f : AIR_ACCEL_MULT);
    const float maxSpeed = (running ? RUN_MAX_SPEED : WALK_MAX_SPEED) * power * (player.grounded ? 1.0f : AIR_MAX_SPEED_MULT);
    const float vacuumSlow = 1.0f - state_.vacuum.pose * (1.0f - VACUUM_MOVE_MULT);

    Vec3 move{};
    move += cameraForwardFlat() * forwardAxis;
    move += cameraRightFlat() * strafeAxis;
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
    player.vel *= std::pow(player.grounded ? GROUND_FRICTION : AIR_FRICTION, dt * 60.0f);
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
    if (state_.meleeCooldown > 0.0f || state_.player.battery < BATTERY_MELEE_COST) return;
    state_.player.battery -= BATTERY_MELEE_COST;
    state_.meleeCooldown = MELEE_COOLDOWN;
    state_.meleePose = 1.0f;
    const Vec3 forward = cameraForwardFlat();
    for (auto& target : state_.targets) {
        if (!target.alive || target.soulState == SoulState::Revolving) continue;
        const Vec3 toTarget = target.pos - state_.player.pos;
        const float forwardDist = toTarget.x * forward.x + toTarget.z * forward.z;
        const float sideSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z - forwardDist * forwardDist;
        if (forwardDist > 0.0f && forwardDist < MELEE_RANGE && sideSq < MELEE_RADIUS * MELEE_RADIUS) {
            target.armor -= MELEE_DAMAGE;
            target.vel += forward * 4.2f;
            target.vel.y = 1.2f;
            if (target.armor <= 0.0f) {
                target.armor = 0.0f;
                target.slurpable = true;
                target.soulState = SoulState::Free;
            }
        }
    }
}

void Game::shootStoredSoul() {
    if (state_.player.souls <= 0 || state_.player.battery < 7.0f) return;
    for (auto& bullet : state_.bullets) {
        if (bullet.alive) continue;
        bullet.alive = true;
        bullet.life = BULLET_LIFE;
        bullet.pos = state_.player.pos + Vec3{0.0f, 0.75f, 0.0f} + state_.camera.forward * 0.75f;
        bullet.vel = state_.camera.forward * BULLET_SPEED;
        state_.player.souls -= 1;
        state_.player.battery -= 7.0f;
        state_.vacuum.active = false;
        return;
    }
}

void Game::respawnTarget(int index) {
    TargetState& target = state_.targets[index];
    const float angle = deterministic01(index, state_.roomIndex, state_.time + 4.0f) * DB_PI * 2.0f;
    const float radius = 8.0f + deterministic01(index, state_.roomIndex, state_.time + 5.0f) * 7.0f;
    target = TargetState{};
    target.pos = {std::cos(angle) * radius, GROUND_Y, std::sin(angle) * radius};
    clampRoom(target.pos);
    target.alive = true;
    target.armor = 2.0f;
    target.health = 1.0f;
    target.phase = static_cast<float>(index) * 0.77f;
}

void Game::updateTargets(float dt) {
    int active = 0;
    for (const auto& target : state_.targets) if (target.alive) ++active;

    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& target = state_.targets[i];
        if (!target.alive) {
            if (active < ACTIVE_HUMAN_TARGET) {
                target.respawnTimer -= dt;
                if (target.respawnTimer <= 0.0f) {
                    respawnTarget(i);
                    ++active;
                }
            }
            continue;
        }

        target.phase += dt;
        if (target.soulState == SoulState::Revolving) continue;
        if (target.soulState == SoulState::Recoiling) {
            target.recoilTime -= dt;
            target.vel.y -= 5.5f * dt;
            target.pos += target.vel * dt;
            target.vel.x *= std::max(0.0f, 1.0f - 3.5f * dt);
            target.vel.z *= std::max(0.0f, 1.0f - 3.5f * dt);
            if (target.pos.y <= GROUND_Y) {
                target.pos.y = GROUND_Y;
                target.vel.y = 0.0f;
            }
            clampRoom(target.pos);
            if (target.recoilTime <= 0.0f) target.soulState = SoulState::Free;
            continue;
        }
        if (target.soulState == SoulState::Attracted || target.soulState == SoulState::Latched || target.soulState == SoulState::Ingesting) continue;

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

void Game::releaseSoul(int index) {
    TargetState& target = state_.targets[index];
    if (target.ingestProgress >= SOUL_CAPTURE_COMMIT_PHASE) {
        captureSoul(index);
        return;
    }
    Vec3 recoil = normalized(target.pos - (state_.player.pos + Vec3{0.0f, 0.72f, 0.0f}));
    if (lengthSq(recoil) < 0.001f) recoil = {std::sin(state_.camera.yaw), 0.18f, std::cos(state_.camera.yaw)};
    recoil = normalized(recoil);
    const float strength = 4.5f + target.ingestProgress * 10.5f;
    target.vel = {recoil.x * strength, 1.4f + target.ingestProgress * 2.2f, recoil.z * strength};
    target.recoilTime = SOUL_RECOIL_DURATION;
    target.capture = 0.0f;
    target.ingestProgress = 0.0f;
    target.soulState = SoulState::Recoiling;
}

void Game::captureSoul(int index) {
    TargetState& target = state_.targets[index];
    target.alive = false;
    target.capture = 0.0f;
    target.ingestProgress = 0.0f;
    target.soulState = SoulState::Free;
    target.respawnTimer = 1.45f + deterministic01(index, state_.roomIndex, state_.time) * 0.9f;
    state_.player.souls = std::min(MAX_STORED_SOULS, state_.player.souls + 1);
    state_.player.battery = std::min(BATTERY_MAX, state_.player.battery + BATTERY_CAPTURE_GAIN);
}

void Game::updateVacuum(float dt) {
    VacuumState& vacuum = state_.vacuum;
    vacuum.pose += ((vacuum.active ? 1.0f : 0.0f) - vacuum.pose) * clampf(dt * 10.0f, 0.0f, 1.0f);
    vacuum.power = clampf(vacuum.power + (vacuum.active ? VACUUM_CHARGE_SPEED : -VACUUM_DECAY_SPEED) * dt, 0.0f, 1.0f);
    vacuum.fieldStrength += ((vacuum.active ? 1.0f : 0.0f) - vacuum.fieldStrength) * clampf(dt * 5.0f, 0.0f, 1.0f);

    const bool attractionActive = vacuum.active && vacuum.power > 0.32f && state_.player.souls < MAX_STORED_SOULS;
    const Vec3 pullPoint = state_.camera.firstPerson
        ? state_.camera.pos - state_.camera.forward * 0.85f
        : state_.player.pos + Vec3{0.0f, 0.72f, 0.0f} + state_.camera.forward * 0.35f;

    int offered = -1;
    float offeredScore = 99999.0f;
    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& target = state_.targets[i];
        if (!target.alive || !target.slurpable) continue;
        if (target.soulState == SoulState::Latched || target.soulState == SoulState::Ingesting) {
            offered = i;
            break;
        }
        if (!attractionActive || (target.soulState != SoulState::Free && target.soulState != SoulState::Attracted)) continue;
        const Vec3 fromCamera = target.pos - state_.camera.pos;
        const float distance = length(fromCamera);
        if (distance > SOUL_ATTRACTION_RANGE || distance <= 0.001f) continue;
        const float facing = dot3(normalized(fromCamera), state_.camera.forward);
        if (facing < SOUL_ATTRACTION_CONE_DOT) continue;
        const float score = distance - (distance <= SOUL_LATCH_DISTANCE ? 3.5f : 0.0f);
        if (score < offeredScore) {
            offeredScore = score;
            offered = i;
        }
    }

    vacuum.target = offered;
    vacuum.coneTightness += (((offered >= 0 && attractionActive) ? 1.0f : 0.0f) - vacuum.coneTightness) * clampf(dt * 4.0f, 0.0f, 1.0f);

    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& target = state_.targets[i];
        if (!target.alive || !target.slurpable || target.soulState == SoulState::Recoiling) continue;

        const bool ownsOffer = attractionActive && i == offered;
        if (!ownsOffer) {
            if (target.soulState == SoulState::Latched || target.soulState == SoulState::Ingesting) releaseSoul(i);
            else if (target.soulState == SoulState::Attracted) target.soulState = SoulState::Free;
            if (target.soulState == SoulState::Free) {
                target.ingestProgress = std::max(0.0f, target.ingestProgress - dt * SOUL_CAPTURE_DECAY);
                target.capture = target.ingestProgress;
            }
            continue;
        }

        Vec3 toPull = pullPoint - target.pos;
        const float distance = std::max(length(toPull), 0.001f);
        const Vec3 direction = toPull * (1.0f / distance);

        if (target.soulState == SoulState::Free) target.soulState = SoulState::Attracted;
        if (target.soulState == SoulState::Attracted) {
            const float proximity = 1.0f - clampf(distance / SOUL_ATTRACTION_RANGE, 0.0f, 1.0f);
            const float speed = vacuum.power * (3.2f + smooth01(proximity) * 5.8f + (distance <= SOUL_LATCH_DISTANCE ? 8.5f : 0.0f));
            target.pos += direction * std::min(distance, speed * dt);
            target.vel = {};
            if (distance <= SOUL_LATCH_DISTANCE) {
                target.soulState = SoulState::Latched;
                target.latchPoint = pullPoint;
            }
            continue;
        }

        target.latchPoint = pullPoint;
        Vec3 toSeal = target.latchPoint - target.pos;
        const float sealDistance = length(toSeal);
        const float phase = clampf(target.ingestProgress, 0.0f, 1.0f);
        const float sealEase = smooth01(clampf((phase - 0.08f) / 0.24f, 0.0f, 1.0f));
        const float pressureEase = smooth01(clampf((phase - 0.32f) / 0.46f, 0.0f, 1.0f));
        const float popEase = smooth01(clampf((phase - 0.78f) / 0.22f, 0.0f, 1.0f));
        const float latchSpeed = 7.0f + sealEase * 5.0f + popEase * 14.0f;
        if (sealDistance > 0.001f) target.pos += normalized(toSeal) * std::min(sealDistance, latchSpeed * dt);
        target.vel = {};

        const bool sealed = sealDistance <= SOUL_SEAL_DISTANCE;
        target.soulState = sealed ? SoulState::Ingesting : SoulState::Latched;
        if (sealed) {
            const float phaseRate = vacuum.power * (0.38f + sealEase * 0.55f + pressureEase * 0.85f + popEase * 2.25f);
            target.ingestProgress = clampf(target.ingestProgress + dt * phaseRate, 0.0f, 1.0f);
            target.health -= 0.028f * vacuum.power * dt;
        }
        target.capture = target.ingestProgress;
        if (target.ingestProgress >= SOUL_CAPTURE_COMMIT_PHASE || target.health <= 0.0f) captureSoul(i);
    }
}

void Game::updateBullets(float dt) {
    for (auto& bullet : state_.bullets) {
        if (!bullet.alive) continue;
        bullet.life -= dt;
        bullet.vel.y -= BULLET_GRAVITY * dt;
        const Vec3 previous = bullet.pos;
        bullet.pos += bullet.vel * dt;
        (void)previous;
        if (bullet.life <= 0.0f || std::abs(bullet.pos.x) > ROOM_WIDTH * 1.25f || std::abs(bullet.pos.z) > ROOM_DEPTH * 1.25f || bullet.pos.y < -4.0f) {
            bullet.alive = false;
            continue;
        }
        for (auto& capture : state_.captures) {
            if (capture.filled) continue;
            const Vec3 delta = bullet.pos - capture.pos;
            if (std::abs(delta.x) < 0.9f && std::abs(delta.y) < 0.9f && std::abs(delta.z) < 0.75f) {
                capture.filled = true;
                state_.player.battery = std::min(BATTERY_MAX, state_.player.battery + 6.0f);
                bullet.alive = false;
                break;
            }
        }
        if (!bullet.alive) continue;
        for (auto& target : state_.targets) {
            if (!target.alive) continue;
            if (distXZ(bullet.pos, target.pos) < 0.85f && std::abs(bullet.pos.y - target.pos.y) < 1.6f) {
                target.armor = 0.0f;
                target.slurpable = true;
                target.soulState = SoulState::Free;
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
    for (const auto& capture : state_.captures) if (capture.filled) ++filled;
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
