#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "gameplay/EnemyLocomotion.hpp"
#include "gameplay/PhysicalEnemyBody.hpp"

namespace {
constexpr float Dt = 1.0f / 60.0f;

struct ProbeResult {
    bool sawStep = false;
    bool sawCorrectiveStep = false;
    bool sawFall = false;
    bool sawSupportedRise = false;
    Vec3 displacement{};
    float yaw = 0.0f;
};

ProbeResult runScenario(const char* name, float desiredSpeed, float desiredYaw,
                        float initialVelocityZ, int frames, std::ofstream* telemetry) {
    using namespace gameplay;
    EnemyLocomotionState locomotion{};
    PhysicalEnemyBodyState body{};
    Vec3 position{};
    Vec3 velocity{0.0f, 0.0f, initialVelocityZ};
    float yaw = 0.0f;
    const auto flatSupport = [](const Vec3& candidate) {
        return EnemyFootSupport{{candidate.x, 0.0f, candidate.z}, {0.0f, 1.0f, 0.0f}, true};
    };
    ProbeResult result{};
    for (int frame = 0; frame < frames; ++frame) {
        EnemyLocomotionInput locomotionInput{};
        locomotionInput.bodyPosition = position;
        locomotionInput.bodyVelocity = velocity;
        locomotionInput.bodyYaw = yaw;
        locomotionInput.bodyPitch = body.bodyPitch;
        locomotionInput.bodyRoll = body.bodyRoll;
        locomotionInput.desiredTravelDirection = desiredSpeed > 0.0f
            ? Vec3{-std::sin(desiredYaw), 0.0f, -std::cos(desiredYaw)} : Vec3{};
        locomotionInput.desiredSpeed = desiredSpeed;
        locomotionInput.desiredYaw = desiredYaw;
        locomotionInput.centerOfMassHeight = 0.90f;
        locomotionInput.traction = 1.0f;
        locomotionInput.dt = Dt;
        locomotionInput.grounded = true;
        locomotionInput.fallen = body.fallen;
        const auto feet = updateEnemyLocomotion(locomotion, locomotionInput, flatSupport);
        result.sawStep = result.sawStep || locomotion.swingFoot >= 0;
        result.sawCorrectiveStep = result.sawCorrectiveStep || feet.correctiveStepActive;
        result.sawSupportedRise = result.sawSupportedRise || feet.supportRecoveryReady;

        PhysicalEnemyBodyInput bodyInput{};
        bodyInput.desiredVelocity = feet.supportedDesiredVelocity;
        bodyInput.actualVelocity = velocity;
        bodyInput.bodyPosition = position;
        bodyInput.desiredYaw = feet.desiredYaw;
        bodyInput.dt = Dt;
        bodyInput.grounded = true;
        bodyInput.leftFootContact = feet.leftContact;
        bodyInput.rightFootContact = feet.rightContact;
        bodyInput.leftFootLoad = feet.leftLoad;
        bodyInput.rightFootLoad = feet.rightLoad;
        bodyInput.leftFootPosition = feet.leftFootPosition;
        bodyInput.rightFootPosition = feet.rightFootPosition;
        bodyInput.supportNormal = feet.supportNormal;
        bodyInput.recoveryUrgency = feet.recoveryUrgency;
        bodyInput.correctiveStepActive = feet.correctiveStepActive;
        bodyInput.turnStepActive = feet.turnStepActive;
        bodyInput.supportRecoveryReady = feet.supportRecoveryReady;
        const auto physical = updatePhysicalEnemyBody(body, bodyInput, yaw);
        velocity = physical.velocity;
        yaw = physical.yaw;
        position += velocity * Dt;
        result.sawFall = result.sawFall || body.fallen;

        if (telemetry) {
            const float totalLoad = feet.leftLoad + feet.rightLoad;
            const Vec3 supportCenter = totalLoad > 0.001f
                ? (feet.leftFootPosition * feet.leftLoad
                    + feet.rightFootPosition * feet.rightLoad) * (1.0f / totalLoad)
                : position;
            const Vec3 projectedCom = projectedEnemyCenterOfMass(
                position, yaw, body.bodyPitch, body.bodyRoll, 0.90f);
            const Vec3 predictedCom = projectedCom + Vec3{velocity.x, 0.0f, velocity.z} * 0.24f;
            const float balanceError = horizontalLength(projectedCom - supportCenter);
            *telemetry << name << ',' << frame * Dt << ','
                << position.x << ',' << position.y << ',' << position.z << ','
                << velocity.x << ',' << velocity.y << ',' << velocity.z << ','
                << projectedCom.x << ',' << projectedCom.z << ','
                << predictedCom.x << ',' << predictedCom.z << ','
                << feet.leftFootPosition.x << ',' << feet.leftFootPosition.y << ',' << feet.leftFootPosition.z << ','
                << static_cast<int>(locomotion.left.phase) << ',' << feet.leftLoad << ','
                << feet.rightFootPosition.x << ',' << feet.rightFootPosition.y << ',' << feet.rightFootPosition.z << ','
                << static_cast<int>(locomotion.right.phase) << ',' << feet.rightLoad << ','
                << supportCenter.x << ',' << supportCenter.z << ',' << balanceError << ','
                << feet.recoveryUrgency << ',' << (body.fallen ? 1 : 0) << ','
                << locomotionInput.desiredTravelDirection.x << ','
                << locomotionInput.desiredTravelDirection.z << ','
                << horizontalLength(velocity) * Dt << '\n';
        }
    }
    result.displacement = position;
    result.yaw = yaw;
    return result;
}
}

int main(int argc, char** argv) {
    std::ofstream telemetry;
    if (argc == 3 && std::string(argv[1]) == "--telemetry") {
        telemetry.open(argv[2], std::ios::trunc);
        if (!telemetry) return 2;
        telemetry << "scenario,time,body_x,body_y,body_z,velocity_x,velocity_y,velocity_z,"
            "projected_com_x,projected_com_z,predicted_com_x,predicted_com_z,"
            "left_x,left_y,left_z,left_phase,left_load,right_x,right_y,right_z,right_phase,right_load,"
            "support_x,support_z,balance_error,recovery_urgency,fallen,desired_x,desired_z,actual_displacement\n";
    }
    std::ofstream* output = telemetry ? &telemetry : nullptr;
    const ProbeResult walk = runScenario("walk", 2.2f, 0.0f, 0.0f, 240, output);
    const ProbeResult turn = runScenario("turn", 1.2f, 1.57079632679f, 0.0f, 240, output);
    const ProbeResult moderate = runScenario("moderate_shove", 0.0f, 0.0f, 2.1f, 240, output);
    const ProbeResult strong = runScenario("strong_shove", 0.0f, 0.0f, 5.8f, 240, output);

    assert(walk.sawStep && walk.displacement.z < -0.25f);
    assert(turn.sawStep && std::abs(turn.yaw) > 0.20f);
    assert(moderate.sawCorrectiveStep && !moderate.sawFall);
    assert(strong.sawFall);
    assert(strong.sawSupportedRise);
    std::puts("ENEMY_LOCOMOTION_PROBE_OK walk=STEP turn=STEP moderate=CORRECTIVE strong=FALL_SUPPORTED_RISE");
    return 0;
}
