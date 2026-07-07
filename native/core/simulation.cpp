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
    return Vec3{
        lerp(a.x, b.x, t),
        lerp(a.y, b.y, t),
        lerp(a.z, b.z, t)
    };
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
    constexpr float pad = 0.8f;
    const float halfW = c.roomWidth * 0.5f;
    const float halfD = c.roomDepth * 0.5f;
    pos.x = clamp(pos.x, -halfW + pad, halfW - pad);
    pos.z = clamp(pos.z, -halfD + pad, halfD - pad);
}

void resetWorld(WorldState& world, const SimConstants& c) {
    const int nextRoom = world.room.roomIndex <= 0 ? 1 : world.room.roomIndex;
    world = WorldState{};
    world.room.roomIndex = nextRoom;
    world.room.clear = false;
    world.room.requiredCaptures = c.capturePoints;

    world.player.pos = {0.0f, c.playerGroundY, 12.0f};
    world.player.vel = {0.0f, 0.0f, 0.0f};
    world.player.yaw = PI;
    world.player.targetYaw = PI;
    world.player.battery = c.batteryMax;
    world.player.souls = 0;
    world.player.grounded = true;
    world.player.alive = true;

    world.camera.yaw = PI;
    world.camera.pitch = c.cameraPitch;
    world.camera.targetPitch = c.cameraPitch;
    world.camera.lookAt = {world.player.pos.x, world.player.pos.y + 0.7f, world.player.pos.z};
    world.camera.pos = {
        world.player.pos.x + std::sin(world.camera.yaw) * c.cameraDistance,
        world.player.pos.y + c.cameraHeight + world.camera.pitch * c.cameraPitchScale,
        world.player.pos.z + std::cos(world.camera.yaw) * c.cameraDistance
    };

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

    float mx = input.moveX;
    float mz = input.moveZ;
    const float magSq = mx * mx + mz * mz;
    const bool moving = magSq > 0.0001f;
    const bool vacuuming = input.vacuum && p.battery > 1.0f;
    const bool sprinting = input.sprint && moving && !vacuuming && p.battery > 5.0f;

    const float desiredSpeedBase = sprinting ? c.sprintSpeed : c.walkSpeed;
    const float desiredSpeed = vacuuming ? desiredSpeedBase * c.vacuumMoveMult : desiredSpeedBase;

    if (moving) {
        const float invMag = 1.0f / std::sqrt(magSq);
        mx *= invMag;
        mz *= invMag;

        const Vec3 desired = {mx * desiredSpeed, 0.0f, mz * desiredSpeed};
        const float accel = p.grounded ? c.accel : c.airAccel;
        const float blend = clamp(accel * dt, 0.0f, 1.0f);

        p.vel.x += (desired.x - p.vel.x) * blend;
        p.vel.z += (desired.z - p.vel.z) * blend;
        p.targetYaw = std::atan2(mx, mz);
    } else {
        const float damp = dampFactor(c.friction, dt);
        p.vel.x *= damp;
        p.vel.z *= damp;
    }

    if (input.jump && p.grounded && p.battery >= c.batteryJumpCost) {
        p.vel.y = c.jumpSpeed;
        p.grounded = false;
        p.battery -= c.batteryJumpCost;
    }

    p.vel.y -= c.gravity * dt;

    p.pos.x += p.vel.x * dt;
    p.pos.y += p.vel.y * dt;
    p.pos.z += p.vel.z * dt;

    if (p.pos.y <= c.playerGroundY) {
        p.pos.y = c.playerGroundY;
        p.vel.y = 0.0f;
        p.grounded = true;
    }

    clampToRoom(p.pos, c);

    float drain = 0.0f;
    if (moving) drain += c.batteryWalkDrain;
    if (sprinting) drain += c.batterySprintDrain;
    if (!p.grounded) drain += c.batteryAirDrain;
    if (vacuuming) drain += c.batteryVacuumDrain;

    const float regen = drain > 0.0f ? c.batteryActiveRegen : c.batteryIdleRegen;
    p.battery += (regen - drain) * dt;
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
                p.battery = clamp(p.battery + 3.0f, 0.0f, c.batteryMax);
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

    cam.yaw -= input.lookX * 0.008f;
    cam.targetPitch += input.lookY * 0.005f;
    cam.targetPitch = clamp(cam.targetPitch, c.cameraPitchMin, c.cameraPitchMax);

    const float pitchBlend = clamp(c.cameraPitchRate * dt, 0.0f, 1.0f);
    cam.pitch = lerp(cam.pitch, cam.targetPitch, pitchBlend);

    const Vec3 desired = {
        world.player.pos.x + std::sin(cam.yaw) * c.cameraDistance,
        world.player.pos.y + c.cameraHeight + cam.pitch * c.cameraPitchScale,
        world.player.pos.z + std::cos(cam.yaw) * c.cameraDistance
    };

    const float followBlend = clamp(c.cameraFollowRate * dt, 0.0f, 1.0f);
    cam.pos = lerp(cam.pos, desired, followBlend);
    cam.lookAt = {world.player.pos.x, world.player.pos.y + 0.7f, world.player.pos.z};
}

void updateWorld(WorldState& world, const InputIntent& input, const SimConstants& c, float dt) {
    if (dt <= 0.0f) return;
    updatePlayer(world, input, c, dt);
    updateTargets(world, input, c, dt);
    updateCaptures(world, c, dt);
    updateCamera(world, input, c, dt);
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
