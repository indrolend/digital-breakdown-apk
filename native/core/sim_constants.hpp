#pragma once

namespace db {

struct SimConstants {
    float roomWidth = 30.0f;
    float roomDepth = 42.0f;
    float wallHeight = 7.2f;

    int activeTargets = 5;
    int maxStoredSouls = 5;
    int capturePoints = 3;

    // PC browser movement contract, simplified for the native core.
    float walkAccel = 16.0f;
    float runAccel = 42.0f;
    float walkMaxSpeed = 18.0f;
    float runMaxSpeed = 42.0f;
    float groundFriction = 0.88f;
    float airFriction = 0.985f;
    float airAccelMultiplier = 0.62f;
    float airMaxSpeedMultiplier = 1.08f;
    float gravity = 14.0f;
    float jumpSpeed = 4.5f;
    float wallSlideRetention = 0.94f;

    float batteryMax = 100.0f;
    float batteryIdleRegen = 22.0f;
    float batteryActiveRegen = 3.0f;
    float batteryWalkDrain = 0.45f;
    float batterySprintDrain = 3.0f;
    float batteryAirDrain = 0.9f;
    float batteryVacuumDrain = 1.35f;
    float batteryJumpCost = 3.0f;
    float batteryCaptureGain = 18.0f;
    float batterySoulEfficiency = 0.16f;

    float vacuumRange = 6.0f;
    float vacuumLatchRadius = 1.0f;
    float vacuumPull = 14.0f;
    float vacuumCaptureTime = 0.55f;
    float vacuumMoveMult = 0.35f;

    float captureRadius = 1.75f;

    float groundY = 0.0f;
    float playerGroundY = 0.55f;
    float targetDamping = 5.5f;
    float targetWanderForce = 0.6f;

    // PC browser third-person camera constants.
    float cameraDistance = 3.0f;
    float cameraHeight = 1.1f;
    float cameraLookLift = 0.45f;
    float cameraMinGroundOffset = 0.8f;
    float cursorSensitivity = 0.003f;
    float cursorMaxPitch = 1.50796449f; // PI * 0.48
};

constexpr float FIXED_DT = 1.0f / 30.0f;
constexpr int MAX_TARGETS = 5;
constexpr int MAX_CAPTURE_POINTS = 5;

} // namespace db
