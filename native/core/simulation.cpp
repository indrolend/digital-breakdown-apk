#include "simulation.hpp"

#include <cmath>

namespace db {
namespace {

constexpr float PI = 3.14159265358979323846f;

float seededPhase(int i) {
    return static_cast<float>(i) * 1.61803398875f;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Vec3 lerp(Vec3 a, Vec3 b, float t) {
    return Vec3{lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t)};
}

float horizLen(Vec3 v) {
    return std::sqrt(v.x * v.x + v.z * v.z);
}

void clampHorizontalSpeed(Vec3& v, float maxSpeed) {
    const float speed = horizLen(v);
    if (speed <= maxSpeed || speed <= 0.0001f) return;
    const float s = maxSpeed / speed;
    v.x *= s;
    v.z *= s;
}

Vec3 cursorForward(float yaw) {
    return Vec3{-std::sin(yaw), 0.0f, -std::cos(yaw)};
}

Vec3 cursorRight(float yaw) {
    return Vec3{std::cos(yaw), 0.0f, -std::sin(yaw)};
}

Vec3 normalizedMoveFromCursor(float moveX, float moveZ, float yaw) {
    Vec3 forward = cursorForward(yaw);
    Vec3 right = cursorRight(yaw);
    Vec3 move = {
        forward.x * moveZ + right.x * moveX,
        0.0f,
        forward.z * moveZ + right.z * moveX
    };
    const float len = horizLen(move);
    if (len <= 0.0001f) return Vec3{};
    return mul(move, 1.0f / len);
}

float batteryPower(const PlayerState& player, const SimConstants& c) {
    const float carriedSoulPenalty = 1.0f / (1.0f + static_cast<float>(player.souls) * c.batterySoulEfficiency);
    const float batteryScale = clamp(player.battery / c.batteryMax, 0.35f, 1.0f);
    return carriedSoulPenalty * batteryScale;
}

void spawnTarget(TargetState& t, int index, float x, float z) {
    t.pos = {x, 0.0f, z};
    t.vel = {0.0f, 0.0f, 0.0f};
    t.kind = TargetKind::Basic;
    t.lifecycle = TargetLifecycle::Alive;
    t.captureProgress = 0.0f;
    t.phase = seededPhase(index);
    t.alive = true;
}

void spawnCapture(CapturePointState& c, float x, float z) {
    c.pos = {x, 0.0f, z};
    c.filled = false;
}

} // namespace

void clampToRoom(Vec3& pos, const SimConstants& c) {
    constexpr float pad = 1.1f;
    const float halfW = c.roomWidth * 0.5f;
    const float halfD = c.roomDepth * 0.5f;
    pos.x = clamp(pos.x, -halfW + pad, halfW - pad);
    pos.z = clamp(pos.z, -halfD + 0.8f, halfD - 0.72f);
}

void resetWorld(WorldState& world, const SimConstants& c) {
    const int nextRoom = world.room.roomIndex <= 0 ? 1 : world.room.roomIndex;
    world = WorldState{};
    world.room.roomIndex = nextRoom;
    world.room.clear = false;
    world.room.requiredCaptures = c.capturePoints;

    world.player.pos = {0.0f, c.playerGroundY, c.roomDepth * 0.5f - 5.5f};
    world.player.vel = {0.0f, 0.0f, 0.0f};
    world.player.yaw = 0.0f;
    world.player.targetYaw = 0.0f;
    world.player.battery = c.batteryMax;
    world.player.souls = 0;
    world.player.grounded = true;
    world.player.alive = true;

    world.camera.yaw = 0.0f;
    world.camera.pitch = 0.0f;
    world.camera.aimDir = cursorForward(world.camera.yaw);
    world.camera.lookAt = add(world.player.pos, Vec3{0.0f, c.cameraLookLift, 0.0f});
    world.camera.pos = add(world.player.pos, Vec3{0.0f, c.cameraHeight, c.cameraDistance});

    static constexpr float targetSpots[MAX_TARGETS][2] = {
        {-8.0f, -12.0f},
        { 8.0f, -13.0f},
        {-9.0f,   0.0f},
        { 9.0f,  -1.0f},
        { 0.0f, -17.0f}
    };

    world.targetCount = clampInt(c.activeTargets, 0, MAX_TARGETS);
    for (int i = 0; i < world.targetCount; ++i) {
        spawnTarget(world.targets[i], i, targetSpots[i][0], targetSpots[i][1]);
    }

    static constexpr float captureSpots[MAX_CAPTURE_POINTS][2] = {
        {-8.0f, -17.0f},
        { 8.0f, -17.0f},
        { 0.0f,  -5.0f},
        {-8.0f,   8.0f},
        { 8.0f,   8.0f}
    };

    world.captureCount = clampInt(c.capturePoints, 0, MAX_CAPTURE_POINTS);
    for (int i = 0; i < world.captureCount; ++i) {
        spawnCapture(world.captures[i], captureSpots[i][0], captureSpots[i][1]);
    }
}

void updatePlayer(WorldState& world, const InputIntent& input, const SimConstants& c, float dt) {
    PlayerState& p = world.player;

    const float moveMagSq = input.moveX * input.moveX + input.moveZ * input.moveZ;
    const bool moving = moveMagSq > 0.0001f;
    const bool vacuuming = input.vacuum && p.battery > 1.0f;
    const bool sprinting = input.sprint && moving && !vacuuming && p.battery > 5.0f;

    const float power = batteryPower(p, c);
    const float airControl = p.grounded ? 1.0f : c.airAccelMultiplier;
    const float airSpeed = p.grounded ? 1.0f : c.airMaxSpeedMultiplier;
    const float accel = (sprinting ? c.runAccel : c.walkAccel) * power * airControl;
    const float maxSpeed = (sprinting ? c.runMaxSpeed : c.walkMaxSpeed) * power * airSpeed;
    const float vacuumSlow = vacuuming ? c.vacuumMoveMult : 1.0f;

    if (moving) {
        const Vec3 move = normalizedMoveFromCursor(input.moveX, input.moveZ, world.camera.yaw);
        p.vel.x += move.x * accel * vacuumSlow * dt;
        p.vel.z += move.z * accel * vacuumSlow * dt;
        p.targetYaw = std::atan2(-move.x, -move.z);
    }

    clampHorizontalSpeed(p.vel, maxSpeed * vacuumSlow);

    if (input.jump && p.grounded && p.battery >= c.batteryJumpCost) {
        p.vel.y = c.jumpSpeed;
        p.grounded = false;
        p.battery -= c.batteryJumpCost;
    }

    if (!p.grounded) {
        p.vel.y -= c.gravity * dt;
    }

    p.pos.x += p.vel.x * dt;
    p.pos.y += p.vel.y * dt;
    p.pos.z += p.vel.z * dt;

    if (p.pos.y <= c.playerGroundY) {
        p.pos.y = c.playerGroundY;
        p.vel.y = 0.0f;
        p.grounded = true;
    }

    const float beforeX = p.pos.x;
    const float beforeZ = p.pos.z;
    clampToRoom(p.pos, c);
    if (p.pos.x != beforeX) {
        p.vel.x = 0.0f;
        p.vel.z *= c.wallSlideRetention;
    }
    if (p.pos.z != beforeZ) {
        p.vel.z = 0.0f;
        p.vel.x *= c.wallSlideRetention;
    }

    const float friction = p.grounded ? c.groundFriction : c.airFriction;
    const float fr = std::pow(friction, dt * 60.0f);
    p.vel.x *= fr;
    p.vel.z *= fr;

    float drain = 0.0f;
    if (moving) drain += sprinting ? c.batterySprintDrain : c.batteryWalkDrain;
    if (!p.grounded) drain += c.batteryAirDrain;
    if (vacuuming) drain += c.batteryVacuumDrain;

    if (drain > 0.0f) p.battery -= drain * dt;
    else p.battery += c.batteryIdleRegen * dt;
    if (drain > 0.0f && !sprinting) p.battery += c.batteryActiveRegen * dt;
    p.battery = clamp(p.battery, 0.0f, c.batteryMax);

    const float yawDiff = shortestAngle(p.yaw, p.targetYaw);
    p.yaw += yawDiff * clamp(dt * 10.0f, 0.0f, 1.0f);
}

void updateTargets(WorldState& world, const InputIntent& input, const SimConstants& c, float dt) {
    PlayerState& p = world.player;
    const Vec3 playerXZ = {p.pos.x, 0.0f, p.pos.z};

    for (int i = 0; i < world.targetCount; ++i) {
        TargetState& t = world.targets[i];
        if (!t.alive) continue;

        Vec3 toPlayer = sub(playerXZ, t.pos);
        const float dist = lengthXZ(toPlayer);
        const bool vacuuming = input.vacuum && p.battery > 1.0f && dist < c.vacuumRange;

        if (vacuuming) {
            if (dist > 0.001f) {
                toPlayer = mul(toPlayer, 1.0f / dist);
            }

            t.vel.x += toPlayer.x * c.vacuumPull * dt;
            t.vel.z += toPlayer.z * c.vacuumPull * dt;
            t.lifecycle = dist < c.vacuumLatchRadius ? TargetLifecycle::Latched : TargetLifecycle::Attracted;

            if (dist < c.vacuumLatchRadius) {
                t.captureProgress += dt / c.vacuumCaptureTime;
            } else {
                t.captureProgress = clamp(t.captureProgress - dt * 1.5f, 0.0f, 1.0f);
            }

            if (t.captureProgress >= 1.0f && p.souls < c.maxStoredSouls) {
                t.alive = false;
                t.lifecycle = TargetLifecycle::Captured;
                p.souls += 1;
                p.battery = clamp(p.battery + c.batteryCaptureGain, 0.0f, c.batteryMax);
                continue;
            }
        } else {
            t.lifecycle = TargetLifecycle::Alive;
            t.captureProgress = clamp(t.captureProgress - dt * 1.5f, 0.0f, 1.0f);

            t.phase += dt;
            t.vel.x += std::sin(t.phase * 0.7f + static_cast<float>(i)) * c.targetWanderForce * dt;
            t.vel.z += std::cos(t.phase * 0.55f + static_cast<float>(i)) * c.targetWanderForce * dt;
        }

        const float damp = dampFactor(c.targetDamping, dt);
        t.vel.x *= damp;
        t.vel.z *= damp;

        t.pos.x += t.vel.x * dt;
        t.pos.z += t.vel.z * dt;
        clampToRoom(t.pos, c);
    }
}

void updateCaptures(WorldState& world, const SimConstants& c, float /*dt*/) {
    int filled = 0;

    for (int i = 0; i < world.captureCount; ++i) {
        CapturePointState& cp = world.captures[i];
        if (cp.filled) {
            filled += 1;
            continue;
        }

        const float dx = world.player.pos.x - cp.pos.x;
        const float dz = world.player.pos.z - cp.pos.z;
        const float distSq = dx * dx + dz * dz;
        const float radiusSq = c.captureRadius * c.captureRadius;

        if (distSq < radiusSq && world.player.souls > 0) {
            cp.filled = true;
            world.player.souls -= 1;
            world.player.battery = clamp(world.player.battery + c.batteryCaptureGain, 0.0f, c.batteryMax);
            filled += 1;
        }
    }

    if (!world.room.clear && filled >= world.captureCount) {
        world.room.clear = true;
        world.room.roomIndex += 1;
    }
}

void updateCamera(WorldState& world, const InputIntent& input, const SimConstants& c, float dt) {
    CameraState& cam = world.camera;

    cam.yaw -= input.lookX * c.cursorSensitivity;
    cam.pitch = clamp(cam.pitch - input.lookY * c.cursorSensitivity, -c.cursorMaxPitch, c.cursorMaxPitch);

    const float cosPitch = std::cos(cam.pitch);
    cam.aimDir = Vec3{
        -std::sin(cam.yaw) * cosPitch,
        std::sin(cam.pitch),
        -std::cos(cam.yaw) * cosPitch
    };

    const Vec3 offset = mul(cam.aimDir, -c.cameraDistance);
    Vec3 desired = add(world.player.pos, offset);
    desired.y += c.cameraHeight;
    const float minCameraY = c.groundY + c.cameraMinGroundOffset;
    if (desired.y < minCameraY) desired.y = minCameraY;

    const float followBlend = clamp(12.0f * dt, 0.0f, 1.0f);
    cam.pos = lerp(cam.pos, desired, followBlend);
    cam.lookAt = add(add(world.player.pos, mul(cam.aimDir, 10.0f)), Vec3{0.0f, c.cameraLookLift, 0.0f});
}

void updateWorld(WorldState& world, const InputIntent& input, const SimConstants& c, float dt) {
    if (dt <= 0.0f) return;
    updateCamera(world, input, c, dt);
    updatePlayer(world, input, c, dt);
    updateTargets(world, input, c, dt);
    updateCaptures(world, c, dt);
}

int countAliveTargets(const WorldState& world) {
    int alive = 0;
    for (int i = 0; i < world.targetCount; ++i) {
        if (world.targets[i].alive) alive += 1;
    }
    return alive;
}

int countFilledCaptures(const WorldState& world) {
    int filled = 0;
    for (int i = 0; i < world.captureCount; ++i) {
        if (world.captures[i].filled) filled += 1;
    }
    return filled;
}

} // namespace db
