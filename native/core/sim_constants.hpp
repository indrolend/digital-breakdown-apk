#pragma once

namespace db {

struct SimConstants {
    float roomWidth = 30.0f;
    float roomDepth = 42.0f;
    float wallHeight = 7.2f;

    int activeTargets = 5;
    int maxStoredSouls = 5;
    int capturePoints = 3;

    float walkSpeed = 7.0f;
    float sprintSpeed = 11.5f;
    float accel = 34.0f;
    float airAccel = 13.0f;
    float friction = 13.0f;
    float gravity = 24.0f;
    float jumpSpeed = 9.2f;

    float batteryMax = 100.0f;
    float batteryIdleRegen = 22.0f;
    float batteryActiveRegen = 3.0f;
    float batteryWalkDrain = 0.45f;
    float batterySprintDrain = 3.0f;
    float batteryAirDrain = 0.9f;
    float batteryVacuumDrain = 1.35f;
    float batteryJumpCost = 4.5f;
    float batteryCaptureGain = 6.0f;

    float vacuumRange = 6.0f;
    float vacuumLatchRadius = 1.0f;
    float vacuumPull = 14.0f;
    float vacuumCaptureTime = 0.55f;
    float vacuumMoveMult = 0.35f;

    float captureRadius = 1.75f;

    float playerGroundY = 0.55f;
    float targetDamping = 5.5f;
    float targetWanderForce = 0.6f;
};

constexpr float FIXED_DT = 1.0f / 30.0f;
constexpr int MAX_TARGETS = 5;
constexpr int MAX_CAPTURE_POINTS = 5;

} // namespace db
