#include "Game.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr float ROOM_WIDTH = 30.0f;
constexpr float ROOM_DEPTH = 42.0f;
constexpr float ROOM_WALL_HEIGHT = 7.2f;
constexpr float GROUND_Y = 0.08f;
constexpr float ROOM_EXIT_Z = -ROOM_DEPTH * 0.5f + 1.15f;
constexpr float ROOM_START_Z = ROOM_DEPTH * 0.5f - 5.5f;
constexpr float ROOM_GRID_Z = ROOM_EXIT_Z + 0.42f;
constexpr float ROOM_MIN_SPAWN_Z = -ROOM_DEPTH * 0.5f + 9.0f;
constexpr float ROOM_MAX_SPAWN_Z = ROOM_DEPTH * 0.5f - 7.0f;

constexpr float WALK_ACCEL = 16.0f;
constexpr float RUN_ACCEL = 42.0f;
constexpr float WALK_MAX_SPEED = 18.0f;
constexpr float RUN_MAX_SPEED = 42.0f;
constexpr float GROUND_FRICTION = 0.88f;
constexpr float AIR_FRICTION = 0.985f;
constexpr float AIR_ACCEL_MULT = 0.62f;
constexpr float AIR_MAX_SPEED_MULT = 1.08f;
constexpr float WALL_SLIDE_RETENTION = 0.94f;
constexpr float GRAVITY = 14.0f;
constexpr float JUMP_SPEED = 4.5f;
constexpr float AIR_JUMP_SPEED = 4.25f;
constexpr float COYOTE_TIME = 0.12f;
constexpr float JUMP_BUFFER = 0.12f;
constexpr float LANDING_MOMENTUM_BOOST = 1.04f;
constexpr float WALL_CLIMB_SPEED = 3.15f;
constexpr float WALL_CLIMB_GRIP = 0.64f;
constexpr float WALL_CLIMB_MAX_HEIGHT = 1.25f;
constexpr float WALL_CLIMB_PUSH_DOT = -0.18f;
constexpr float CEILING_CLEARANCE = 0.42f;
constexpr float PLAYER_CEILING_BODY_CLEARANCE = 0.42f;
constexpr float PLAYER_COLLISION_RADIUS = 0.34f;
constexpr float PLAYER_SUPPORT_RADIUS = 0.34f;
constexpr float WALL_CLIMB_RADIUS = 0.34f;
constexpr float CAMERA_COLLISION_RADIUS = 0.42f;
constexpr float CAMERA_COLLISION_BACKOFF = 0.16f;


constexpr float VACUUM_MOVE_MULT = 0.35f;
constexpr float VACUUM_CHARGE_SPEED = 3.5f;
constexpr float VACUUM_DECAY_SPEED = 6.0f;
constexpr float SOUL_ATTRACTION_RANGE = 15.5f;
constexpr float SOUL_LATCH_DISTANCE = 0.48f;
constexpr float SOUL_SEAL_DISTANCE = 0.14f;
constexpr float SOUL_CAPTURE_COMMIT_PHASE = 0.92f;
constexpr float SOUL_CAPTURE_DECAY = 0.75f;
constexpr float SOUL_MORPH_DURATION = 0.72f;
constexpr float SOUL_ARMOR_NORMAL = 2.0f;
constexpr float SOUL_ARMOR_BRUTE = 4.0f;
constexpr float HUMAN_SCALE_BRUTE = 1.7f;
constexpr float HUMAN_WALK_PHASE_PER_METER = 7.5f;
constexpr float HUMAN_WALK_SPEED = 0.72f;
constexpr float HUMAN_WALK_TARGET_RADIUS = 0.55f;
constexpr float HUMAN_WALK_RANGE = 5.5f;
constexpr float HUMAN_ATTACK_NOTICE_RANGE = 5.6f;
constexpr float HUMAN_ATTACK_START_RANGE = 1.55f;
constexpr float HUMAN_ATTACK_HIT_RANGE = 1.85f;
constexpr float HUMAN_ATTACK_DURATION = 0.48f;
constexpr float HUMAN_ATTACK_ACTIVE_TIME = 0.26f;
constexpr float HUMAN_ATTACK_COOLDOWN = 1.15f;
constexpr float HUMAN_ATTACK_KNOCKBACK = 3.0f;
constexpr float HUMAN_ATTACK_BATTERY_COST = 26.0f;
constexpr float TARGET_HITFLASH_DECAY_PER_FRAME = 0.045f;
constexpr float VACUUM_DAMAGE = 0.28f;

constexpr float MELEE_COMBO_WINDOW = 0.720f;
struct MeleeCombo { int variant; float range, damage, hitRadius, visual, dash, dashSpeed, cooldown, recoilDistance, recoilSpeed, lunge; };
constexpr MeleeCombo MELEE_COMBOS[] = {
    {0,2.35f,0.82f,0.78f,0.20f,0.13f,12.5f,0.22f,0.08f,1.25f,0.15f},
    {1,2.85f,1.08f,0.90f,0.25f,0.18f,14.0f,0.27f,0.12f,1.75f,0.22f},
    {2,3.18f,1.48f,1.02f,0.31f,0.23f,15.2f,0.38f,0.15f,2.10f,0.29f},
    {3,3.00f,1.22f,0.96f,0.29f,0.20f,13.8f,0.34f,0.12f,1.80f,0.25f}
};
constexpr float MELEE_VARIANT_SIDE[] = {1,-1,1,-1};
constexpr float MELEE_VARIANT_ROLL[] = {-0.72f,0.72f,-0.42f,0.42f};
constexpr float MELEE_VARIANT_YAW[] = {0.62f,-0.62f,0.42f,-0.42f};
constexpr float MELEE_VARIANT_PITCH[] = {-0.32f,-0.32f,0.42f,0.42f};
constexpr float MELEE_VARIANT_LIFT[] = {0.012f,0.012f,-0.006f,-0.006f};
constexpr float BULLET_SPEED = 25.0f;
constexpr float BULLET_GRAVITY = 11.5f;
constexpr float BULLET_LIFE = 3.25f;
constexpr int MAX_STORED_SOULS = 30;
constexpr int ACTIVE_HUMAN_TARGET = 5;

constexpr float PHONE_GAIT_WALK_PITCH = 0.34f;
constexpr float PHONE_GAIT_RUN_PITCH = 0.88f;
constexpr float PHONE_GAIT_WALK_ROLL = 0.24f;
constexpr float PHONE_GAIT_RUN_ROLL = 0.66f;
constexpr float PHONE_GAIT_WALK_YAW = 0.18f;
constexpr float PHONE_GAIT_RUN_YAW = 0.52f;
constexpr float PHONE_GAIT_FORWARD_OFFSET = 0.052f;
constexpr float PHONE_GAIT_SIDE_OFFSET = 0.062f;
constexpr float PHONE_GAIT_LIFT = 0.065f;
constexpr float PHONE_GAIT_OBLIQUE_SWEEP = 0.58f;
constexpr float PHONE_GAIT_CONE_TWIST = 0.34f;
constexpr float PHONE_GAIT_OLOID_MEANDER = 0.045f;
constexpr float PHONE_GAIT_CYLINDER_RADIUS = 0.36f;

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

float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float dotXZ(const Vec3& a, const Vec3& b) { return a.x * b.x + a.z * b.z; }
float distXZ(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}
float batteryPower(const PlayerState& p) { return clampf(p.battery / 18.0f, 0.35f, 1.0f); }
float smooth01(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
void syncTargetReactionVisual(TargetState& target) {
    target.visualReaction = makeHumanReactionVisual(
        target.visualWalkPhase,
        target.locomotionAmount,
        target.hitFlash,
        target.hitDirectionLocal,
        target.vacuumPullAmount,
        target.captureCollapseAmount,
        target.soulMorph,
        target.visibility > 0.5f,
        target.attackTimer,
        target.attackVariant
    );
}

void syncSoulVisual(TargetState& target, float time) {
    target.soulVisual = makeSoulVisualState(
        static_cast<int>(target.soulState), target.vacuumPullAmount, target.ingestProgress,
        target.hitFlash, time, target.phase, target.alive && target.slurpable);
}
}

void Game::reset() {
    state_ = GameState{};
    resetRoom();
}

float Game::seededRoomValue(int offset) const {
    const float x = std::sin(static_cast<float>(state_.roomSeed + offset) * 12.9898f) * 43758.5453f;
    return x - std::floor(x);
}

int Game::getRoomTileIndex(float z) const {
    return static_cast<int>(std::floor((z + ROOM_DEPTH * 0.5f) / ROOM_DEPTH));
}

float Game::getRoomTileOriginZ(int tileIndex) const {
    return static_cast<float>(tileIndex) * ROOM_DEPTH;
}

float Game::wrapZ(float z) const {
    return z - getRoomTileOriginZ(getRoomTileIndex(z));
}

void Game::buildRoomColliders() {
    state_.debug.colliderCount = std::min(ROOM_COLLIDER_COUNT, 8 + std::min(state_.roomIndex, 7));
    for (auto& c : state_.roomColliders) c = RoomCollider{};
    for (int i = 0; i < state_.debug.colliderCount; ++i) {
        float px = (seededRoomValue(20 + i) - 0.5f) * (ROOM_WIDTH - 8.0f);
        float pz = ROOM_MIN_SPAWN_Z + seededRoomValue(60 + i) * (ROOM_MAX_SPAWN_Z - ROOM_MIN_SPAWN_Z);
        const bool keepStartClear = std::abs(px) < 4.5f && pz > ROOM_START_Z - 4.5f;
        const bool keepGoalClear = std::abs(px) < 5.5f && std::abs(pz - ROOM_GRID_Z) < 4.5f;
        if (keepStartClear) px += px < 0.0f ? -5.0f : 5.0f;
        if (keepGoalClear) pz += 5.0f;
        const float w = 1.0f + seededRoomValue(120 + i) * 1.7f;
        const float d = 1.0f + seededRoomValue(150 + i) * 1.7f;
        const float h = 0.55f + seededRoomValue(90 + i) * 1.45f;
        RoomCollider& c = state_.roomColliders[i];
        c.minX = px - w * 0.5f; c.maxX = px + w * 0.5f;
        c.minZ = pz - d * 0.5f; c.maxZ = pz + d * 0.5f;
        c.bottomY = 0.0f; c.topY = h;
        c.width = w; c.depth = d; c.height = h; c.center = {px, h * 0.5f, pz};
    }
}

void Game::resetRoom() {
    const int roomIndex = state_.roomIndex;
    const int roomSeed = state_.roomSeed;
    state_.roomClear = false;
    state_.player = PlayerState{};
    state_.player.pos = {0.0f, GROUND_Y, ROOM_START_Z};
    state_.player.grounded = true;
    state_.player.jumpVel = 0.0f;
    state_.player.airJumpsRemaining = 1;
    state_.player.coyoteTimer = 0.0f;
    state_.player.jumpBufferTimer = 0.0f;
    state_.camera = CameraState{};
    state_.vacuum = VacuumState{};
    state_.phonePose = PhonePoseState{};
    state_.meleeVisual = MeleeVisualState{};
    state_.meleeComboWindow = 0.0f;
    state_.topology = RoomTopologyState{};
    state_.roomIndex = roomIndex;
    state_.roomSeed = roomSeed;
    buildRoomColliders();

    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& target = state_.targets[i];
        target = TargetState{};
        target.pos = {-8.0f + static_cast<float>(i % 5) * 4.0f, GROUND_Y, -12.0f + static_cast<float>(i / 5) * 4.5f};
        target.alive = i < ACTIVE_HUMAN_TARGET;
        target.brute = seededRoomValue(420 + i) < 0.18f;
        target.armor = target.brute ? SOUL_ARMOR_BRUTE : SOUL_ARMOR_NORMAL;
        target.health = 1.0f;
        target.scale = target.brute ? HUMAN_SCALE_BRUTE : 1.0f;
        target.phase = static_cast<float>(i) * 0.77f;
        target.visualWalkPhase = target.phase;
        target.visualYaw = seededRoomValue(440 + i) * DB_PI * 2.0f;
        target.attackCooldown = seededRoomValue(460 + i) * 0.5f;
        target.attackVariant = static_cast<int>(seededRoomValue(480 + i) * 4.0f) % 4;
        chooseHumanWalkTarget(i);
        syncTargetReactionVisual(target);
        if (!target.alive) target.respawnTimer = 1.45f + seededRoomValue(300 + i) * 0.9f;
    }

    const Vec3 captureSpots[CAPTURE_COUNT] = {
        {-1.64f, 3.05f, ROOM_GRID_Z}, {-0.82f, 3.05f, ROOM_GRID_Z}, {0.0f, 3.05f, ROOM_GRID_Z},
        {0.82f, 3.05f, ROOM_GRID_Z}, {1.64f, 3.05f, ROOM_GRID_Z}
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

void Game::clearInputState() {
    InputState& input = state_.input;
    input.forward = false;
    input.back = false;
    input.left = false;
    input.right = false;
    input.sprint = false;
    input.jumpHeld = false;
    input.primaryHeld = false;
    input.touchPrimaryHeld = false;
    input.touchSprint = false;
    input.touchMoveX = 0.0f;
    input.touchMoveZ = 0.0f;
    input.lookDeltaX = 0.0f;
    input.lookDeltaY = 0.0f;
    input.touching = false;
}

void Game::setTouch(int action, float x, float y, int pointerCount) {
    (void)pointerCount;
    InputState& input = state_.input;
    if (action == TOUCH_DOWN) {
        input.touching = true; input.primaryHeld = true;
        input.touchX = input.lastTouchX = x; input.touchY = input.lastTouchY = y;
    } else if (action == TOUCH_MOVE) {
        input.lookDeltaX += x - input.lastTouchX; input.lookDeltaY += y - input.lastTouchY;
        input.touchX = input.lastTouchX = x; input.touchY = input.lastTouchY = y;
    } else if (action == TOUCH_UP || action == TOUCH_CANCEL) {
        input.touching = false; input.primaryHeld = false;
    }
}

void Game::setTouchControls(float moveX, float moveZ, float lookDeltaX, float lookDeltaY,
    bool vacuumHeld, bool sprintHeld, bool jumpPressed, bool meleePressed,
    bool shootPressed, bool cameraTogglePressed) {
    InputState& input = state_.input;
    input.touchMoveX = clampf(moveX, -1.0f, 1.0f);
    input.touchMoveZ = clampf(moveZ, -1.0f, 1.0f);
    input.lookDeltaX += lookDeltaX; input.lookDeltaY += lookDeltaY;
    input.touchPrimaryHeld = vacuumHeld; input.touchSprint = sprintHeld;
    if (jumpPressed) input.jumpPressed = true;
    if (meleePressed) input.meleePressed = true;
    if (shootPressed) input.shootPressed = true;
    if (cameraTogglePressed) input.cameraTogglePressed = true;
}

void Game::update(float dt) {
    dt = clampf(dt, 0.0f, 0.033f);
    state_.time += dt; state_.frame += 1;
    updateInputActions(dt);
    updatePlayer(dt);
    updateCamera(dt);
    updateTargets(dt);
    updateVacuum(dt);
    float contact = 0.0f;
    for (auto& target : state_.targets) {
        syncSoulVisual(target, state_.time);
        if (target.soulState == SoulState::Latched || target.soulState == SoulState::Ingesting) {
            contact = std::max(contact, target.ingestProgress > 0.0f ? target.ingestProgress : 0.25f);
        }
    }
    state_.phoneVisual = makePhoneVisualState(state_.vacuum.pose, state_.vacuum.power, contact, state_.time, state_.camera.firstPerson);
    updateBullets(dt);
    updateCaptures(dt);
}

void Game::updateInputActions(float dt) {
    InputState& input = state_.input;
    if (input.cameraTogglePressed) state_.camera.firstPerson = !state_.camera.firstPerson;
    state_.camera.yaw -= input.lookDeltaX * 0.003f;
    state_.camera.pitch = clampf(state_.camera.pitch - input.lookDeltaY * 0.003f, -DB_PI * 0.48f, DB_PI * 0.48f);
    input.lookDeltaX = input.lookDeltaY = 0.0f;
    if (input.jumpPressed) { state_.player.jumpBufferTimer = JUMP_BUFFER; tryJump(); }
    if (input.meleePressed) triggerMelee();
    if (input.shootPressed) shootStoredSoul();
    input.cameraTogglePressed = input.jumpPressed = input.meleePressed = input.shootPressed = false;
    state_.meleeCooldown = std::max(0.0f, state_.meleeCooldown - dt);
    state_.meleeComboWindow = std::max(0.0f, state_.meleeComboWindow - dt);
    state_.meleeVisual.visualTimer = std::max(0.0f, state_.meleeVisual.visualTimer - dt);
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

float Game::getPlayerCeilingLimit() const {
    return ROOM_WALL_HEIGHT - GROUND_Y - CEILING_CLEARANCE - PLAYER_CEILING_BODY_CLEARANCE;
}

float Game::getPlayerSupportY(float x, float z) const {
    float supportY = GROUND_Y;
    const float radius = PLAYER_SUPPORT_RADIUS;
    const float localZ = wrapZ(z);
    for (int i = 0; i < state_.debug.colliderCount; ++i) {
        const RoomCollider& c = state_.roomColliders[i];
        if (x > c.minX - radius && x < c.maxX + radius && localZ > c.minZ - radius && localZ < c.maxZ + radius)
            supportY = std::max(supportY, c.topY + GROUND_Y);
    }
    return std::min(supportY, getPlayerCeilingLimit());
}

void Game::resolvePlayerObstacleCollisions() {
    PlayerState& player = state_.player;
    const float radius = PLAYER_COLLISION_RADIUS;
    const float tileOriginZ = getRoomTileOriginZ(getRoomTileIndex(player.pos.z));
    float localPlayerZ = player.pos.z - tileOriginZ;
    for (int i = 0; i < state_.debug.colliderCount; ++i) {
        const RoomCollider& c = state_.roomColliders[i];
        const bool onTop = player.pos.y >= c.topY + GROUND_Y - 0.08f;
        if (onTop) continue;
        if (player.pos.y < c.bottomY - 0.4f || player.pos.y > c.topY + GROUND_Y + 0.4f) continue;
        if (player.pos.x > c.minX - radius && player.pos.x < c.maxX + radius && localPlayerZ > c.minZ - radius && localPlayerZ < c.maxZ + radius) {
            const float pushes[4] = {
                std::abs(player.pos.x - (c.minX - radius)), std::abs((c.maxX + radius) - player.pos.x),
                std::abs(localPlayerZ - (c.minZ - radius)), std::abs((c.maxZ + radius) - localPlayerZ)
            };
            int axis = 0; for (int p = 1; p < 4; ++p) if (pushes[p] < pushes[axis]) axis = p;
            if (axis == 0) { player.pos.x = c.minX - radius; if (player.vel.x > 0) player.vel.x = 0; }
            else if (axis == 1) { player.pos.x = c.maxX + radius; if (player.vel.x < 0) player.vel.x = 0; }
            else if (axis == 2) { localPlayerZ = c.minZ - radius; player.pos.z = tileOriginZ + localPlayerZ; if (player.vel.z > 0) player.vel.z = 0; }
            else { localPlayerZ = c.maxZ + radius; player.pos.z = tileOriginZ + localPlayerZ; if (player.vel.z < 0) player.vel.z = 0; }
        }
    }
}

void Game::applyWallClimb(float dt) {
    PlayerState& p = state_.player;
    InputState& input = state_.input;
    if (p.grounded || !input.jumpHeld) return;
    const float forwardAxis = (input.forward ? 1.0f : 0.0f) - (input.back ? 1.0f : 0.0f) + input.touchMoveZ;
    const float strafeAxis = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f) + input.touchMoveX;
    if (std::abs(forwardAxis) + std::abs(strafeAxis) <= 0.05f) return;
    Vec3 intent = cameraForwardFlat() * forwardAxis + cameraRightFlat() * strafeAxis;
    intent = normalized(intent);
    Vec3 normal{}; float topY = 0.0f; bool contact = false;
    const float radius = WALL_CLIMB_RADIUS, climbGap = 0.08f, localZ = wrapZ(p.pos.z);
    const float minX = -ROOM_WIDTH * 0.5f + 1.1f, maxX = ROOM_WIDTH * 0.5f - 1.1f;
    const float minZ = -ROOM_DEPTH * 0.5f + 0.8f, maxZ = ROOM_DEPTH * 0.5f - 0.72f;
    if (p.pos.x <= minX + climbGap) { normal = {1,0,0}; topY = getPlayerCeilingLimit(); contact = true; }
    else if (p.pos.x >= maxX - climbGap) { normal = {-1,0,0}; topY = getPlayerCeilingLimit(); contact = true; }
    else if (localZ <= minZ + climbGap) { normal = {0,0,1}; topY = getPlayerCeilingLimit(); contact = true; }
    else if (localZ >= maxZ - climbGap) { normal = {0,0,-1}; topY = getPlayerCeilingLimit(); contact = true; }
    for (int i = 0; !contact && i < state_.debug.colliderCount; ++i) {
        const RoomCollider& c = state_.roomColliders[i];
        const bool inZ = localZ > c.minZ - radius && localZ < c.maxZ + radius;
        const bool inX = p.pos.x > c.minX - radius && p.pos.x < c.maxX + radius;
        if (inZ && std::abs(p.pos.x - (c.minX - radius)) < climbGap + 0.05f) { normal={-1,0,0}; topY=c.topY; contact=true; }
        else if (inZ && std::abs(p.pos.x - (c.maxX + radius)) < climbGap + 0.05f) { normal={1,0,0}; topY=c.topY; contact=true; }
        else if (inX && std::abs(localZ - (c.minZ - radius)) < climbGap + 0.05f) { normal={0,0,-1}; topY=c.topY; contact=true; }
        else if (inX && std::abs(localZ - (c.maxZ + radius)) < climbGap + 0.05f) { normal={0,0,1}; topY=c.topY; contact=true; }
    }
    if (!contact || dotXZ(intent, normal) >= WALL_CLIMB_PUSH_DOT) return;
    const float climbLimit = std::min(getPlayerCeilingLimit(), topY + GROUND_Y + WALL_CLIMB_MAX_HEIGHT);
    if (p.pos.y >= climbLimit) { p.pos.y = climbLimit; if (p.jumpVel > 0) p.jumpVel = 0; return; }
    p.jumpVel = std::max(p.jumpVel, WALL_CLIMB_SPEED * batteryPower(p));
    p.vel.x *= WALL_CLIMB_GRIP; p.vel.z *= WALL_CLIMB_GRIP;
    p.battery = std::max(0.0f, p.battery - 2.0f * dt);
}

bool Game::isInsideDoorAperture(const Vec3& position, float pad) const {
    return std::abs(position.x) <= 2.1f + pad && position.y >= GROUND_Y - 0.12f && position.y <= 3.72f + 0.22f;
}

void Game::updateRoomTopology(float previousZ, float currentZ) {
    const int previousTile = getRoomTileIndex(previousZ);
    const int currentTile = getRoomTileIndex(currentZ);
    state_.topology.previousTileIndex = previousTile;
    state_.topology.currentTileIndex = currentTile;
    state_.topology.advancing = false;
    if (previousTile == currentTile) return;
    Vec3 local = state_.player.pos; local.z = wrapZ(local.z);
    if (!isInsideDoorAperture(local, 0.04f)) return;
    if (state_.roomClear && currentTile < previousTile) {
        state_.roomIndex += 1; state_.roomSeed += 97; state_.topology.advancing = true; buildRoomColliders();
    }
}

void Game::updatePlayer(float dt) {
    PlayerState& p = state_.player;
    InputState& input = state_.input;
    const float previousZ = p.pos.z;
    if (p.jumpBufferTimer > 0) p.jumpBufferTimer = std::max(0.0f, p.jumpBufferTimer - dt);
    if (p.grounded) p.coyoteTimer = COYOTE_TIME; else p.coyoteTimer = std::max(0.0f, p.coyoteTimer - dt);
    const bool running = input.sprint || input.touchSprint;
    const float power = batteryPower(p);
    const float airControl = p.grounded ? 1.0f : AIR_ACCEL_MULT;
    const float airSpeed = p.grounded ? 1.0f : AIR_MAX_SPEED_MULT;
    const float accel = (running ? RUN_ACCEL : WALK_ACCEL) * power * airControl;
    const float maxSpeed = (running ? RUN_MAX_SPEED : WALK_MAX_SPEED) * power * airSpeed;
    updateMeleeDash(dt);
    const float vacuumSlow = 1.0f - state_.vacuum.pose * (1.0f - VACUUM_MOVE_MULT);
    float forwardAxis = (input.forward ? 1.0f : 0.0f) - (input.back ? 1.0f : 0.0f) + input.touchMoveZ;
    float strafeAxis = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f) + input.touchMoveX;
    Vec3 move = cameraForwardFlat() * forwardAxis + cameraRightFlat() * strafeAxis;
    if (lengthSq(move) > 0.0f) { move = normalized(move); p.vel += move * (accel * vacuumSlow * dt); }
    limitHorizontal(p.vel, maxSpeed * vacuumSlow);
    if (!p.grounded) {
        p.jumpVel -= GRAVITY * dt;
        applyWallClimb(dt);
        p.pos.y += p.jumpVel * dt;
        if (p.pos.y > getPlayerCeilingLimit()) { p.pos.y = getPlayerCeilingLimit(); if (p.jumpVel > 0) p.jumpVel = 0; }
        const float support = getPlayerSupportY(p.pos.x, p.pos.z);
        if (p.pos.y <= support) {
            p.pos.y = support; p.jumpVel = 0; p.grounded = true; p.coyoteTimer = COYOTE_TIME; p.airJumpsRemaining = 1;
            state_.phonePose.doubleJumpTimer = 0.0f;
            if (horizontalLength(p.vel) > 1.2f) p.vel *= LANDING_MOMENTUM_BOOST;
        }
    }
    if (p.grounded && p.jumpBufferTimer > 0) startGroundJump();
    p.pos += p.vel * dt;
    resolvePlayerObstacleCollisions();
    updateRoomTopology(previousZ, p.pos.z);
    if (p.pos.y > getPlayerCeilingLimit()) { p.pos.y = getPlayerCeilingLimit(); if (p.jumpVel > 0) p.jumpVel = 0; }
    const float supportAfter = getPlayerSupportY(p.pos.x, p.pos.z);
    if (p.grounded) {
        if (p.pos.y > supportAfter + 0.12f) p.grounded = false; else p.pos.y = supportAfter;
    } else if (p.jumpVel <= 0 && p.pos.y <= supportAfter) {
        p.pos.y = supportAfter; p.jumpVel = 0; p.grounded = true; p.coyoteTimer = COYOTE_TIME; p.airJumpsRemaining = 1;
        state_.phonePose.doubleJumpTimer = 0.0f;
    }
    const float minX = -ROOM_WIDTH * 0.5f + 1.1f, maxX = ROOM_WIDTH * 0.5f - 1.1f;
    if (p.pos.x < minX) { p.pos.x = minX; if (p.vel.x < 0) p.vel.x = 0; p.vel.z *= WALL_SLIDE_RETENTION; }
    else if (p.pos.x > maxX) { p.pos.x = maxX; if (p.vel.x > 0) p.vel.x = 0; p.vel.z *= WALL_SLIDE_RETENTION; }
    const float friction = p.grounded ? GROUND_FRICTION : AIR_FRICTION;
    p.vel *= std::pow(friction, dt * 60.0f);
    p.targetYaw = state_.camera.yaw;
    p.yaw = state_.camera.yaw;
    updatePhoneGait(dt, running);
    updatePhoneActionPose(dt, running, forwardAxis, strafeAxis);
    state_.debug.supportY = supportAfter;
    state_.debug.localZ = wrapZ(p.pos.z);
    state_.debug.horizontalSpeed = horizontalLength(p.vel);
    state_.debug.cameraYaw = state_.camera.yaw;
    state_.debug.cameraPitch = state_.camera.pitch;
    state_.debug.cameraMode = state_.camera.firstPerson ? 1 : 0;
    state_.debug.phoneYaw = state_.phonePose.yaw;
    state_.debug.phonePitch = state_.phonePose.pitch;
    state_.debug.phoneRoll = state_.phonePose.roll;
    state_.debug.phoneLift = state_.phonePose.lift;
    state_.debug.phoneForward = state_.phonePose.forward;
    state_.debug.phoneSide = state_.phonePose.side;
}

void Game::updatePhoneActionPose(float dt, bool running, float forwardAxis, float strafeAxis) {
    PhonePoseState& pose = state_.phonePose;
    pose.doubleJumpVacuumPause = std::max(0.0f, pose.doubleJumpVacuumPause - dt);
    pose.doubleJumpTimer = std::max(0.0f, pose.doubleJumpTimer - dt);
    const bool jumpFlip = pose.doubleJumpTimer > 0.0f;
    const bool vacuumFacing = state_.vacuum.active && pose.doubleJumpVacuumPause <= 0.0f;
    const float targetTurn = vacuumFacing ? 1.0f : 0.0f;
    pose.screenForwardTurn += (targetTurn - pose.screenForwardTurn) * std::min(1.0f, dt * 4.5f);
    const float easedTurn = pose.screenForwardTurn*pose.screenForwardTurn*(3.0f-2.0f*pose.screenForwardTurn);

    const float inputMag = std::max(1.0f, std::sqrt(forwardAxis*forwardAxis + strafeAxis*strafeAxis));
    const float lean = running ? 0.5f : 0.35f;
    Quat base = quatAxisAngle({0,1,0}, state_.camera.yaw);
    base = base * quatAxisAngle({1,0,0}, -(forwardAxis/inputMag)*lean);
    base = base * quatAxisAngle({0,0,1}, -(strafeAxis/inputMag)*lean*0.82f);

    // THREE.Object3D.lookAt points the phone's local +Z axis along the camera ray.
    const Vec3 cameraRay = state_.camera.forward;
    const float aimYaw = std::atan2(cameraRay.x, cameraRay.z);
    const float aimPitch = -std::asin(clampf(cameraRay.y, -1.0f, 1.0f));
    Quat aim = quatAxisAngle({0,1,0}, aimYaw) * quatAxisAngle({1,0,0}, aimPitch);
    Quat q = quatSlerp(base, aim, easedTurn);
    const float ritualFlip = std::sin(easedTurn * DB_PI) * 0.75f;
    const float wobble = std::sin(state_.time * 18.0f) * 0.035f * state_.vacuum.pose;

    if (jumpFlip) {
        const float phase = 1.0f - clampf(pose.doubleJumpTimer / 0.30f, 0.0f, 1.0f);
        const float ease = phase*phase*(3.0f-2.0f*phase);
        pose.doubleJumpFlip = ease * DB_PI * 2.0f;
        q = quatAxisAngle({0,1,0}, pose.doubleJumpFlipYaw) * quatAxisAngle({1,0,0}, -pose.doubleJumpFlip);
        pose.actionState = 5;
    } else {
        pose.doubleJumpFlip = 0.0f;
        q = q * quatAxisAngle({1,0,0}, -ritualFlip + pose.pitch);
        q = q * quatAxisAngle({0,1,0}, pose.yaw);
        q = q * quatAxisAngle({0,0,1}, wobble + pose.roll);
        pose.actionState = vacuumFacing ? 2 : (pose.energy > 0.01f ? 1 : 0);
        const MeleeVisualState& melee = state_.meleeVisual;
        if (melee.visualTimer > 0.0f) {
            const float attackT = 1.0f - clampf(melee.visualTimer / std::max(0.001f, melee.visualDuration), 0.0f, 1.0f);
            const float snap = std::sin(attackT * DB_PI);
            const float recover = std::sin(std::min(1.0f, attackT * 1.45f) * DB_PI);
            const int variant = std::max(0, std::min(3, melee.variant));
            const float hitWeight = melee.visualHit ? 1.18f : 0.82f;
            pose.forward += melee.lunge * snap;
            pose.side += MELEE_VARIANT_SIDE[variant] * 0.035f * snap;
            pose.lift += MELEE_VARIANT_LIFT[variant] * snap;
            q = q * quatAxisAngle({1,0,0}, MELEE_VARIANT_PITCH[variant] * snap * hitWeight);
            q = q * quatAxisAngle({0,0,1}, MELEE_VARIANT_ROLL[variant] * recover * hitWeight);
            q = q * quatAxisAngle({0,1,0}, MELEE_VARIANT_YAW[variant] * snap * hitWeight);
            pose.actionState = 4;
        }
    }
    pose.orientation = quatNormalized(q);
}

void Game::updatePhoneGait(float dt, bool running) {
    PhonePoseState& g = state_.phonePose;
    g.pitch = g.roll = g.yaw = g.lift = g.forward = g.side = g.energy = 0.0f;
    const float speed = horizontalLength(state_.player.vel);
    const float speedMix = clampf(speed / RUN_MAX_SPEED, 0.0f, 1.0f);
    const float sprintMix = running ? clampf(speed / (RUN_MAX_SPEED * 0.72f), 0.0f, 1.0f) : 0.0f;
    const float runMix = clampf((speed - WALK_MAX_SPEED * 0.72f) / (RUN_MAX_SPEED - WALK_MAX_SPEED * 0.72f), 0.0f, 1.0f);
    const float targetEnergy = state_.player.grounded && speed > 0.08f ? clampf(speedMix * 0.72f + sprintMix * 0.28f, 0.0f, 1.0f) : 0.0f;
    const float response = targetEnergy > g.rollEnergy ? 10.0f : 5.5f;
    g.rollEnergy += (targetEnergy - g.rollEnergy) * std::min(1.0f, dt * response);
    if (state_.player.grounded && speed > 0.025f) {
        const float tighten = lerpf(1.0f, 0.78f, sprintMix);
        g.phase += speed * dt / (PHONE_GAIT_CYLINDER_RADIUS * tighten);
    }
    const float step = std::sin(g.phase), stepAbs = std::abs(step), planted = step >= 0 ? 1.0f : -1.0f;
    const float quadrature = std::cos(g.phase);
    const float contactEase = std::pow(stepAbs, 0.20f);
    const float transferEase = std::pow(1.0f - std::abs(quadrature), 0.34f);
    const float coneSweep = std::sin(g.phase * 2.0f);
    const float coneCatch = std::pow(std::max(0.0f, coneSweep), 0.42f);
    const float mirroredCone = planted * (contactEase * 0.72f + transferEase * 0.28f);
    const float oloidLoop = std::sin(g.phase + DB_PI * 0.5f);
    const float rotationPower = clampf(runMix * 0.72f + sprintMix * 0.28f, 0.0f, 1.0f);
    const float exaggeration = lerpf(1.15f, 1.55f, rotationPower);
    const float pitchAmp = lerpf(PHONE_GAIT_WALK_PITCH, PHONE_GAIT_RUN_PITCH, rotationPower) * exaggeration;
    const float rollAmp = lerpf(PHONE_GAIT_WALK_ROLL, PHONE_GAIT_RUN_ROLL, rotationPower) * exaggeration;
    const float yawAmp = lerpf(PHONE_GAIT_WALK_YAW, PHONE_GAIT_RUN_YAW, rotationPower) * exaggeration;
    const float suppression = 1.0f - std::max(state_.vacuum.pose, state_.meleePose) * 0.65f;
    const float energy = g.rollEnergy * suppression;
    const float forwardRoll = coneCatch * 0.72f + contactEase * 0.28f;
    const float oblique = planted * (transferEase * 0.62f + stepAbs * 0.38f) * lerpf(0.7f, 1.18f, rotationPower);
    g.energy = energy;
    g.pitch = -forwardRoll * pitchAmp * energy;
    g.roll = (-mirroredCone * rollAmp + quadrature * PHONE_GAIT_CONE_TWIST * transferEase) * energy;
    g.yaw = (mirroredCone * yawAmp + oblique * PHONE_GAIT_OBLIQUE_SWEEP) * energy;
    g.lift = (contactEase * 0.58f + transferEase * 0.42f) * PHONE_GAIT_LIFT * energy;
    g.forward = forwardRoll * PHONE_GAIT_FORWARD_OFFSET * energy;
    g.side = (mirroredCone * PHONE_GAIT_SIDE_OFFSET + oloidLoop * PHONE_GAIT_OLOID_MEANDER * transferEase) * energy;
}

float Game::getSegmentAabbHitT(const Vec3& from, const Vec3& to, const RoomCollider& box, float pad) const {
    float tMin = 0.0f;
    float tMax = 1.0f;
    const float origins[3] = {from.x, from.y, from.z};
    const float deltas[3] = {to.x - from.x, to.y - from.y, to.z - from.z};
    const float mins[3] = {box.minX - pad, box.bottomY - pad, box.minZ - pad};
    const float maxs[3] = {box.maxX + pad, box.topY + pad, box.maxZ + pad};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(deltas[axis]) < 0.00001f) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return -1.0f;
            continue;
        }
        const float inv = 1.0f / deltas[axis];
        float a = (mins[axis] - origins[axis]) * inv;
        float b = (maxs[axis] - origins[axis]) * inv;
        if (a > b) std::swap(a, b);
        tMin = std::max(tMin, a);
        tMax = std::min(tMax, b);
        if (tMin > tMax) return -1.0f;
    }
    return clampf(tMin, 0.0f, 1.0f);
}

void Game::constrainThirdPersonCamera(Vec3& desired, const Vec3& lookBase) const {
    Vec3 start = lookBase;
    start.y += 0.58f;
    const Vec3 end = desired;
    const float localStartZ = wrapZ(start.z);
    const float localEndZ = localStartZ + (end.z - start.z);
    const Vec3 localStart{start.x, start.y, localStartZ};
    const Vec3 localEnd{end.x, end.y, localEndZ};

    float nearestT = 1.0f;
    for (int i = 0; i < state_.debug.colliderCount; ++i) {
        const float t = getSegmentAabbHitT(localStart, localEnd, state_.roomColliders[i], CAMERA_COLLISION_RADIUS);
        if (t >= 0.0f && t < nearestT) nearestT = t;
    }

    const float doorX0 = -2.1f;
    const float doorX1 = 2.1f;
    const float wallPad = 0.3f;
    const auto makeBounds = [](float minX, float maxX, float minZ, float maxZ, float bottomY, float topY) {
        RoomCollider c{};
        c.minX = minX; c.maxX = maxX; c.minZ = minZ; c.maxZ = maxZ; c.bottomY = bottomY; c.topY = topY;
        return c;
    };
    const RoomCollider roomBounds[] = {
        makeBounds(-ROOM_WIDTH * 0.5f - 0.3f, -ROOM_WIDTH * 0.5f + 0.5f, -ROOM_DEPTH * 0.5f, ROOM_DEPTH * 0.5f, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds( ROOM_WIDTH * 0.5f - 0.5f,  ROOM_WIDTH * 0.5f + 0.3f, -ROOM_DEPTH * 0.5f, ROOM_DEPTH * 0.5f, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds(-ROOM_WIDTH * 0.5f, doorX0, -ROOM_DEPTH * 0.5f - wallPad, -ROOM_DEPTH * 0.5f + 0.5f, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds(doorX1, ROOM_WIDTH * 0.5f, -ROOM_DEPTH * 0.5f - wallPad, -ROOM_DEPTH * 0.5f + 0.5f, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds(doorX0, doorX1, -ROOM_DEPTH * 0.5f - wallPad, -ROOM_DEPTH * 0.5f + 0.5f, 3.72f, ROOM_WALL_HEIGHT),
        makeBounds(-ROOM_WIDTH * 0.5f, doorX0, ROOM_DEPTH * 0.5f - 0.5f, ROOM_DEPTH * 0.5f + wallPad, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds(doorX1, ROOM_WIDTH * 0.5f, ROOM_DEPTH * 0.5f - 0.5f, ROOM_DEPTH * 0.5f + wallPad, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds(doorX0, doorX1, ROOM_DEPTH * 0.5f - 0.5f, ROOM_DEPTH * 0.5f + wallPad, 3.72f, ROOM_WALL_HEIGHT)
    };
    for (const RoomCollider& c : roomBounds) {
        const float t = getSegmentAabbHitT(localStart, localEnd, c, CAMERA_COLLISION_RADIUS * 0.8f);
        if (t >= 0.0f && t < nearestT) nearestT = t;
    }

    if (nearestT < 1.0f) {
        const Vec3 segment = end - start;
        const float dist = length(segment);
        const float safeT = dist > 0.001f ? std::max(0.0f, nearestT - CAMERA_COLLISION_BACKOFF / dist) : 0.0f;
        desired = start + segment * safeT;
    }

    desired.x = clampf(desired.x, -ROOM_WIDTH * 0.5f + CAMERA_COLLISION_RADIUS, ROOM_WIDTH * 0.5f - CAMERA_COLLISION_RADIUS);
    const float desiredTileOriginZ = getRoomTileOriginZ(getRoomTileIndex(desired.z));
    const float desiredLocalZ = wrapZ(desired.z);
    const bool nearDoorSeam = std::abs(std::abs(desiredLocalZ) - ROOM_DEPTH * 0.5f) < CAMERA_COLLISION_RADIUS * 2.8f;
    Vec3 desiredInLocal = desired; desiredInLocal.z = desiredLocalZ;
    const bool desiredInDoorAperture = isInsideDoorAperture(desiredInLocal, CAMERA_COLLISION_RADIUS * 0.8f);
    if (!nearDoorSeam || !desiredInDoorAperture) {
        desired.z = desiredTileOriginZ + clampf(desiredLocalZ, -ROOM_DEPTH * 0.5f + CAMERA_COLLISION_RADIUS, ROOM_DEPTH * 0.5f - CAMERA_COLLISION_RADIUS);
    }
    desired.y = clampf(desired.y, GROUND_Y + 0.65f, ROOM_WALL_HEIGHT - 0.45f);
}

void Game::updateCamera(float dt) {
    (void)dt;
    CameraState& camera = state_.camera;
    const PlayerState& player = state_.player;
    const float cp = std::cos(camera.pitch);
    camera.forward = normalized({-std::sin(camera.yaw) * cp, std::sin(camera.pitch), -std::cos(camera.yaw) * cp});
    if (camera.firstPerson) {
        camera.pos = player.pos + Vec3{0, 0.72f, 0} + camera.forward * 0.18f;
        camera.lookTarget = camera.pos + camera.forward * 10.0f;
        return;
    }
    Vec3 desired = player.pos - camera.forward * 3.0f + Vec3{0, 1.1f, 0};
    if (desired.y < GROUND_Y + 0.8f) desired.y = GROUND_Y + 0.8f;
    constrainThirdPersonCamera(desired, player.pos);
    camera.pos = desired;
    camera.lookTarget = player.pos + camera.forward * 10.0f;
    camera.lookTarget.y += 0.45f;
}

void Game::tryJump() {
    if (state_.player.grounded || state_.player.coyoteTimer > 0) startGroundJump();
    else if (state_.player.airJumpsRemaining > 0) startAirJump();
}
void Game::startGroundJump() {
    PlayerState& p = state_.player;
    p.jumpVel = JUMP_SPEED; p.grounded = false; p.coyoteTimer = 0; p.jumpBufferTimer = 0; p.airJumpsRemaining = 1;
}
void Game::startAirJump() {
    PlayerState& p = state_.player;
    p.jumpVel = AIR_JUMP_SPEED; p.jumpBufferTimer = 0; p.airJumpsRemaining -= 1;
    PhonePoseState& pose = state_.phonePose;
    pose.doubleJumpTimer = std::max(pose.doubleJumpTimer, 0.30f);
    pose.doubleJumpVacuumPause = std::max(pose.doubleJumpVacuumPause, 0.16f);
    const InputState& input = state_.input;
    const float forwardAxis = (input.forward ? 1.0f : 0.0f) - (input.back ? 1.0f : 0.0f) + input.touchMoveZ;
    const float strafeAxis = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f) + input.touchMoveX;
    Vec3 direction = cameraForwardFlat()*forwardAxis + cameraRightFlat()*strafeAxis;
    pose.doubleJumpFlipYaw = lengthSq(direction) > 0.000001f
        ? std::atan2(-normalized(direction).x, -normalized(direction).z)
        : state_.camera.yaw;
}

void Game::triggerMelee() {
    if (state_.meleeCooldown > 0) return;
    const int comboIndex = state_.meleeComboWindow > 0.0f ? (state_.meleeVisual.comboIndex + 1) % 4 : 0;
    const MeleeCombo& combo = MELEE_COMBOS[comboIndex];
    state_.meleeComboWindow = MELEE_COMBO_WINDOW;
    state_.meleeCooldown = combo.cooldown; state_.meleePose = 1.0f;
    MeleeVisualState& visual = state_.meleeVisual;
    visual.comboIndex=comboIndex; visual.variant=combo.variant; visual.range=combo.range; visual.damage=combo.damage;
    visual.hitRadius=combo.hitRadius; visual.visualDuration=combo.visual; visual.visualTimer=combo.visual;
    visual.dashTimer=combo.dash; visual.dashSpeed=combo.dashSpeed; visual.travel=0.0f; visual.lunge=combo.lunge;
    visual.recoilDistance=combo.recoilDistance; visual.recoilSpeed=combo.recoilSpeed; visual.visualHit=false;
    visual.direction=cameraForwardFlat(); visual.origin=state_.player.pos+visual.direction*0.22f+Vec3{0,0.42f,0};
    visual.impact=visual.origin+visual.direction*(combo.range*0.72f);
    int hitCount=0;
    for (auto& t : state_.targets) if (t.alive) {
        const Vec3 delta{t.pos.x-state_.player.pos.x,0,t.pos.z-state_.player.pos.z};
        const float forwardDist=dotXZ(delta,visual.direction);
        if(forwardDist < -0.35f || forwardDist > combo.range) continue;
        const Vec3 sideDelta=delta-visual.direction*forwardDist;
        const float hitRadius=combo.hitRadius+(t.brute?0.28f:0.0f);
        if(lengthSq(sideDelta)>hitRadius*hitRadius) continue;
        const Vec3 away = normalized(Vec3{t.pos.x - state_.player.pos.x, 0.0f, t.pos.z - state_.player.pos.z});
        const Vec3 right{std::cos(t.visualYaw), 0.0f, -std::sin(t.visualYaw)};
        t.armor -= combo.damage*(1.0f+std::min(0.75f,hitCount*0.12f));
        t.hitFlash = 1.0f;
        t.hitDirectionLocal = clampf(away.x * right.x + away.z * right.z, -1.0f, 1.0f);
        if (t.armor <= 0) { t.armor = 0.0f; t.slurpable = true; t.soulState = SoulState::Free; t.soulMorph = 0.0f; }
        visual.visualHit=true; visual.impact=t.pos+Vec3{0,0.62f,0}; ++hitCount;
    }
    if(visual.visualHit){state_.player.pos-=visual.direction*visual.recoilDistance; state_.player.vel-=visual.direction*visual.recoilSpeed; visual.dashTimer=hitCount>1?visual.dashTimer*0.35f:0.0f;}
}

void Game::updateMeleeDash(float dt) {
    MeleeVisualState& visual=state_.meleeVisual;
    if(visual.dashTimer<=0.0f) return;
    const float step=std::min(visual.dashSpeed*dt,std::max(0.0f,visual.range-visual.travel));
    visual.dashTimer=std::max(0.0f,visual.dashTimer-dt); visual.travel+=step;
    state_.player.pos+=visual.direction*step; state_.player.vel.x*=0.55f; state_.player.vel.z*=0.55f;
    visual.origin=state_.player.pos+visual.direction*0.22f+Vec3{0,0.42f,0};
    if(!visual.visualHit) visual.impact=visual.origin+visual.direction*(visual.range*0.72f);
}
void Game::shootStoredSoul() {
    if (state_.player.souls <= 0) return;
    for (auto& b : state_.bullets) if (!b.alive) {
        b.alive = true; b.life = BULLET_LIFE; b.pos = state_.player.pos + Vec3{0,0.5f,0}; b.vel = state_.camera.forward * BULLET_SPEED; state_.player.souls--; break;
    }
}
void Game::releaseSoul(int index) { if (index >= 0 && index < TARGET_COUNT) state_.targets[index].soulState = SoulState::Recoiling; }
void Game::captureSoul(int index) {
    if (index < 0 || index >= TARGET_COUNT) return;
    TargetState& t = state_.targets[index];
    t.alive = false;
    t.visibility = 0.0f;
    t.soulCubeAmount = 0.0f;
    syncTargetReactionVisual(t);
    t.respawnTimer = 2.0f;
    state_.player.souls++;
}
void Game::respawnTarget(int index) {
    TargetState& t = state_.targets[index]; t = TargetState{}; t.alive = true;
    t.brute = seededRoomValue(520 + index) < 0.18f;
    t.armor = t.brute ? SOUL_ARMOR_BRUTE : SOUL_ARMOR_NORMAL;
    t.scale = t.brute ? HUMAN_SCALE_BRUTE : 1.0f;
    t.health = 1.0f;
    t.pos = {(seededRoomValue(500 + index) - 0.5f) * 20.0f, GROUND_Y, ROOM_MIN_SPAWN_Z + seededRoomValue(600 + index) * (ROOM_MAX_SPAWN_Z - ROOM_MIN_SPAWN_Z)};
    t.visualYaw = seededRoomValue(540 + index) * DB_PI * 2.0f;
    t.visualWalkPhase = seededRoomValue(560 + index) * DB_PI * 2.0f;
    t.attackCooldown=seededRoomValue(580+index)*0.5f;
    t.attackVariant=static_cast<int>(seededRoomValue(590+index)*4.0f)%4;
    chooseHumanWalkTarget(index);
    syncTargetReactionVisual(t);
}

bool Game::isHumanPointBlocked(float x,float z,float radius) const {
    const float localZ=wrapZ(z);
    if(x < -ROOM_WIDTH*0.5f+radius || x > ROOM_WIDTH*0.5f-radius) return true;
    for(int i=0;i<state_.debug.colliderCount;++i){const RoomCollider& c=state_.roomColliders[i];
        if(x>c.minX-radius && x<c.maxX+radius && localZ>c.minZ-radius && localZ<c.maxZ+radius) return true;}
    return false;
}

void Game::chooseHumanWalkTarget(int index) {
    TargetState& t=state_.targets[index];
    const float tileOrigin=getRoomTileOriginZ(getRoomTileIndex(t.pos.z));
    for(int attempt=0;attempt<10;++attempt){
        const int seed=700+index*97+t.walkTargetSequence*19+attempt*2;
        const float angle=seededRoomValue(seed)*DB_PI*2.0f;
        const float radius=1.8f+seededRoomValue(seed+1)*HUMAN_WALK_RANGE;
        const float x=clampf(t.pos.x+std::cos(angle)*radius,-ROOM_WIDTH*0.5f+1.1f,ROOM_WIDTH*0.5f-1.1f);
        const float localZ=clampf(wrapZ(t.pos.z)+std::sin(angle)*radius,ROOM_MIN_SPAWN_Z,ROOM_MAX_SPAWN_Z);
        if(!isHumanPointBlocked(x,tileOrigin+localZ,0.5f)){t.walkTarget={x,GROUND_Y,tileOrigin+localZ}; ++t.walkTargetSequence; return;}
    }
    t.walkTarget=t.pos; ++t.walkTargetSequence;
}

void Game::updateTargets(float dt) {
    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& t = state_.targets[i];
        if (!t.alive) { if ((t.respawnTimer -= dt) <= 0 && i < ACTIVE_HUMAN_TARGET) respawnTarget(i); continue; }
        t.hitFlash = std::max(0.0f, t.hitFlash - TARGET_HITFLASH_DECAY_PER_FRAME);
        t.visibility = 1.0f;
        t.vacuumPullAmount = 0.0f;
        t.captureCollapseAmount = clampf(t.ingestProgress, 0.0f, 1.0f);
        if (t.slurpable) {
            t.soulMorph = std::min(1.0f, t.soulMorph + dt / SOUL_MORPH_DURATION);
            t.locomotionAmount = 0.0f;
        } else {
            t.soulMorph = 0.0f;
            const float currentTileOrigin=getRoomTileOriginZ(state_.topology.currentTileIndex);
            const float targetTileOrigin=getRoomTileOriginZ(getRoomTileIndex(t.pos.z));
            if(std::abs(currentTileOrigin-targetTileOrigin)>0.001f){const float shift=currentTileOrigin-targetTileOrigin; t.pos.z+=shift; t.walkTarget.z+=shift;}
            t.pos.y=GROUND_Y; t.attackCooldown=std::max(0.0f,t.attackCooldown-dt);
            Vec3 toPlayer{state_.player.pos.x-t.pos.x,0,state_.player.pos.z-t.pos.z};
            float playerDist=horizontalLength(toPlayer);
            if(playerDist>0.001f && playerDist<HUMAN_ATTACK_NOTICE_RANGE) t.visualYaw=std::atan2(-toPlayer.x/playerDist,-toPlayer.z/playerDist);
            if(t.attackTimer>0.0f){
                t.attackTimer=std::max(0.0f,t.attackTimer-dt); t.locomotionAmount=0.0f;
                const float progress=1.0f-clampf(t.attackTimer/HUMAN_ATTACK_DURATION,0.0f,1.0f);
                if(!t.attackHit && progress>=HUMAN_ATTACK_ACTIVE_TIME/HUMAN_ATTACK_DURATION && playerDist<=HUMAN_ATTACK_HIT_RANGE){
                    const Vec3 away=playerDist>0.001f?toPlayer*(-1.0f/playerDist):Vec3{0,0,1};
                    state_.player.vel+=away*HUMAN_ATTACK_KNOCKBACK;
                    state_.player.battery=std::max(0.0f,state_.player.battery-HUMAN_ATTACK_BATTERY_COST); t.attackHit=true;
                }
            } else if(playerDist<HUMAN_ATTACK_START_RANGE && t.attackCooldown<=0.0f){
                t.attackTimer=HUMAN_ATTACK_DURATION; t.attackCooldown=HUMAN_ATTACK_COOLDOWN;
                t.attackVariant=(t.attackVariant+1)%4; t.attackHit=false; t.locomotionAmount=0.0f;
            } else {
                Vec3 destination=(playerDist<HUMAN_ATTACK_NOTICE_RANGE && playerDist>HUMAN_ATTACK_START_RANGE*0.88f)?state_.player.pos:t.walkTarget;
                Vec3 delta{destination.x-t.pos.x,0,destination.z-t.pos.z}; float dist=horizontalLength(delta);
                if(dist<HUMAN_WALK_TARGET_RADIUS && playerDist>=HUMAN_ATTACK_NOTICE_RANGE){chooseHumanWalkTarget(i); delta=t.walkTarget-t.pos; delta.y=0; dist=horizontalLength(delta);}
                if(dist>0.001f){
                    const Vec3 dir=delta*(1.0f/dist); const float aggro=playerDist<HUMAN_ATTACK_NOTICE_RANGE?1.28f:1.0f;
                    const float variation=0.82f+0.18f*std::sin(static_cast<float>(i)*12.9898f);
                    const float speed=HUMAN_WALK_SPEED*aggro*(t.brute?0.56f:1.0f)*variation;
                    const float step=std::min(dist,speed*dt); const Vec3 next=t.pos+dir*step;
                    if(isHumanPointBlocked(next.x,next.z,0.42f)) chooseHumanWalkTarget(i);
                    else {t.pos=next; t.visualYaw=std::atan2(-dir.x,-dir.z); t.visualWalkPhase+=step*HUMAN_WALK_PHASE_PER_METER;}
                    t.locomotionAmount=step>0.00001f?1.0f:0.0f;
                } else t.locomotionAmount=0.0f;
            }
        }
        t.soulCubeAmount = t.slurpable ? smooth01(t.soulMorph) : 0.0f;
        syncTargetReactionVisual(t);
    }
}

void Game::updateVacuum(float dt) {
    VacuumState& v = state_.vacuum;
    v.power = clampf(v.power + (v.active ? VACUUM_CHARGE_SPEED : -VACUUM_DECAY_SPEED) * dt, 0, 1);
    v.pose += ((v.active ? 1.0f : 0.0f) - v.pose) * std::min(1.0f, dt * 8.0f);
    v.target = -1;
    if (!v.active) {
        for (auto& target : state_.targets) {
            if (target.soulState == SoulState::Latched || target.soulState == SoulState::Ingesting || target.soulState == SoulState::Attracted) {
                target.soulState = SoulState::Free;
            }
            target.ingestProgress = std::max(0.0f, target.ingestProgress - SOUL_CAPTURE_DECAY * dt);
            target.captureCollapseAmount = clampf(target.ingestProgress, 0.0f, 1.0f);
            syncTargetReactionVisual(target);
        }
        return;
    }
    float best = SOUL_ATTRACTION_RANGE;
    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& t = state_.targets[i]; if (!t.alive || !t.slurpable) continue;
        const float d = distXZ(t.pos, state_.player.pos); if (d < best) { best = d; v.target = i; }
    }
    if (v.target < 0) return;
    TargetState& t = state_.targets[v.target];
    Vec3 screen = state_.player.pos + state_.camera.forward * 0.2f + Vec3{0,0.5f,0};
    Vec3 delta = screen - t.pos; float d = length(delta);
    const float soulMass = t.brute ? 1.45f : 1.0f;
    const float proximity = 1.0f - clampf(d / SOUL_ATTRACTION_RANGE, 0.0f, 1.0f);
    const float closeEase = smooth01(proximity);
    t.vacuumPullAmount = clampf(std::max(v.power, t.ingestProgress), 0.0f, 1.0f);
    if (d > 0.001f) {
        const float speed = v.power * (3.2f + closeEase * 5.8f) / soulMass;
        t.pos += normalized(delta) * std::min(d, speed * dt);
        t.visualYaw = std::atan2(-delta.x, -delta.z);
    }
    if (d <= SOUL_LATCH_DISTANCE) {
        t.soulState = d <= SOUL_SEAL_DISTANCE ? SoulState::Ingesting : SoulState::Latched;
        const float phase = clampf(t.ingestProgress, 0.0f, 1.0f);
        const float sealEase = smooth01(clampf((phase - 0.08f) / (0.32f - 0.08f), 0.0f, 1.0f));
        const float pressureEase = smooth01(clampf((phase - 0.32f) / (0.78f - 0.32f), 0.0f, 1.0f));
        const float popEase = smooth01(clampf((phase - 0.78f) / (1.0f - 0.78f), 0.0f, 1.0f));
        const float phaseRate = v.power * (0.38f + sealEase * 0.55f + pressureEase * 0.85f + popEase * 2.25f) / soulMass;
        t.ingestProgress = clampf(t.ingestProgress + dt * phaseRate, 0.0f, 1.0f);
        t.health -= VACUUM_DAMAGE * v.power * dt * 0.10f;
    } else {
        t.ingestProgress = std::max(0.0f, t.ingestProgress - SOUL_CAPTURE_DECAY * dt);
        t.soulState = SoulState::Attracted;
    }
    t.captureCollapseAmount = clampf(t.ingestProgress, 0.0f, 1.0f);
    syncTargetReactionVisual(t);
    if (t.ingestProgress >= SOUL_CAPTURE_COMMIT_PHASE || t.health <= 0.0f) captureSoul(v.target);
}

void Game::updateBullets(float dt) {
    for (auto& b : state_.bullets) if (b.alive) {
        b.life -= dt; b.vel.y -= BULLET_GRAVITY * dt; b.pos += b.vel * dt;
        if (b.life <= 0 || b.pos.y < -2) b.alive = false;
    }
}
void Game::updateCaptures(float dt) {
    (void)dt;
    int filled = 0; for (const auto& c : state_.captures) if (c.filled) filled++;
    state_.roomClear = filled >= CAPTURE_COUNT;
}
void Game::clampRoom(Vec3& pos) {
    pos.x = clampf(pos.x, -ROOM_WIDTH * 0.5f + 1.1f, ROOM_WIDTH * 0.5f - 1.1f);
}
