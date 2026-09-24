#include <cassert>
#include <cstdio>

#include "gameplay/EnemyLocomotion.hpp"
#include "gameplay/PhysicalEnemyBody.hpp"

int main() {
    using namespace gameplay;
    constexpr float Dt = 1.0f / 60.0f;
    const auto flatSupport = [](const Vec3& candidate) {
        return EnemyFootSupport{{candidate.x, 0.0f, candidate.z}, {0.0f, 1.0f, 0.0f}, true};
    };

    EnemyLocomotionState locomotion{};
    EnemyLocomotionInput input{};
    input.bodyPosition = {0.0f, 0.0f, 0.0f};
    input.bodyVelocity = {0.0f, 0.0f, 2.4f};
    input.bodyYaw = 0.0f;
    input.desiredYaw = 0.0f;
    input.dt = Dt;
    initializeEnemyLocomotion(locomotion, input, flatSupport);

    bool correctiveStep = false;
    float targetZ = 0.0f;
    for (int frame = 0; frame < 20; ++frame) {
        const auto output = updateEnemyLocomotion(locomotion, input, flatSupport);
        if (output.correctiveStepActive) {
            correctiveStep = true;
            const auto& swing = locomotion.swingFoot == 0 ? locomotion.left : locomotion.right;
            targetZ = swing.swingTarget.z;
            break;
        }
    }
    assert(correctiveStep);
    assert(targetZ > 0.10f); // catch the backwards-moving COM with a rear step
    assert(locomotion.crouch > 0.0f);
    assert(targetZ <= enemyCapturePoint(
        input.bodyPosition, input.bodyVelocity, input.centerOfMassHeight).z + 0.20f);

    PhysicalEnemyBodyState catching{};
    catching.initialized = true;
    catching.leftFootPlanted = true;
    catching.rightFootPlanted = true;
    catching.leftFootPlant = {-0.12f, 0.0f, 0.0f};
    catching.rightFootPlant = {0.12f, 0.0f, 0.0f};
    catching.leftPlantWeight = 0.5f;
    catching.rightPlantWeight = 0.5f;
    PhysicalEnemyBodyInput bodyInput{};
    bodyInput.bodyPosition = {0.0f, 0.0f, 0.72f};
    bodyInput.predictedCenterOfMass = {0.0f, 0.0f, 0.34f};
    bodyInput.hasPredictedCenterOfMass = true;
    bodyInput.actualVelocity = {0.0f, 0.0f, 2.4f};
    bodyInput.leftFootPosition = catching.leftFootPlant;
    bodyInput.rightFootPosition = catching.rightFootPlant;
    bodyInput.leftFootLoad = 0.5f;
    bodyInput.rightFootLoad = 0.5f;
    bodyInput.leftFootContact = 1.0f;
    bodyInput.rightFootContact = 1.0f;
    bodyInput.grounded = true;
    bodyInput.dt = Dt;
    bodyInput.recoveryUrgency = 0.8f;
    bodyInput.correctiveStepActive = true;
    updatePhysicalEnemyBody(catching, bodyInput, 0.0f);
    assert(!catching.fallen); // a reachable step gets a bounded landing window

    PhysicalEnemyBodyState failed = catching;
    failed.fallen = false;
    failed.supportFailureTime = 0.0f;
    bodyInput.bodyPosition.z = 1.10f;
    bodyInput.predictedCenterOfMass.z = 1.10f;
    bodyInput.correctiveStepActive = false;
    updatePhysicalEnemyBody(failed, bodyInput, 0.0f);
    assert(failed.fallen); // severe unsupported escape still falls

    EnemyLocomotionState floorRecovery{};
    input.bodyPosition = {};
    input.bodyVelocity = {};
    input.fallen = false;
    initializeEnemyLocomotion(floorRecovery, input, flatSupport);
    input.fallen = true;
    EnemyLocomotionOutput recoveryOutput{};
    bool sawGathering = false;
    for (int frame = 0; frame < 90; ++frame) {
        recoveryOutput = updateEnemyLocomotion(floorRecovery, input, flatSupport);
        if (floorRecovery.recoveryPhase == EnemyRecoveryPhase::GatherFeet) {
            sawGathering = true;
            assert(recoveryOutput.leftContact == 0.0f);
            assert(recoveryOutput.rightContact == 0.0f);
        }
        if (recoveryOutput.supportRecoveryReady) break;
    }
    assert(sawGathering);
    assert(recoveryOutput.supportRecoveryReady);
    assert(recoveryOutput.leftContact > 0.80f && recoveryOutput.rightContact > 0.80f);

    std::puts("ENEMY_LOCOMOTION_RECOVERY_OK predictive_step=REAR fall=FAILED_SUPPORT rise=SUPPORTED");
    return 0;
}
