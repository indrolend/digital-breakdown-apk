#include <cassert>
#include <cmath>
#include <cstdio>

#include "gameplay/EnemyLocomotion.hpp"

int main() {
    using namespace gameplay;
    EnemyLocomotionState locomotion{};
    EnemyLocomotionInput input{};
    input.bodyPosition = {2.0f, 0.0f, 3.0f};
    input.bodyYaw = 0.0f;
    input.individuality = 0.25f;

    const auto flatSupport = [](const Vec3& candidate) {
        return EnemyFootSupport{{candidate.x, 0.0f, candidate.z}, {0.0f, 1.0f, 0.0f}, true};
    };
    initializeEnemyLocomotion(locomotion, input, flatSupport);
    assert(locomotion.initialized);
    assert(locomotion.left.planted && locomotion.right.planted);
    const Vec3 originalLeft = locomotion.left.plantPosition;
    const Vec3 originalRight = locomotion.right.plantPosition;

    // Root motion alone is not allowed to regenerate planted feet.
    input.bodyPosition = {4.0f, 0.0f, -1.0f};
    const auto output = enemyLocomotionOutput(locomotion);
    assert(std::abs(output.leftFootPosition.x - originalLeft.x) < 0.00001f);
    assert(std::abs(output.leftFootPosition.z - originalLeft.z) < 0.00001f);
    assert(std::abs(output.rightFootPosition.x - originalRight.x) < 0.00001f);
    assert(std::abs(output.rightFootPosition.z - originalRight.z) < 0.00001f);
    assert(std::abs(output.leftLoad + output.rightLoad - 1.0f) < 0.00001f);

    const auto centeredSupport = enemySupportRegion(
        originalLeft, 0.5f, 1.0f, originalRight, 0.5f, 1.0f,
        (originalLeft + originalRight) * 0.5f);
    assert(centeredSupport.state == EnemySupportState::Double);
    assert(centeredSupport.distance < 0.00001f);
    const auto leftOnlySupport = enemySupportRegion(
        originalLeft, 1.0f, 1.0f, originalRight, 0.0f, 0.0f, originalLeft);
    assert(leftOnlySupport.state == EnemySupportState::Left);

    EnemyLocomotionState walker{};
    input.bodyPosition = {0.0f, 0.0f, 0.0f};
    input.bodyVelocity = {};
    input.desiredTravelDirection = {0.0f, 0.0f, -1.0f};
    input.desiredSpeed = 2.0f;
    input.desiredYaw = 0.0f;
    input.dt = 1.0f / 60.0f;
    initializeEnemyLocomotion(walker, input, flatSupport);
    const Vec3 stancePlant = walker.right.plantPosition;
    bool sawSwing = false;
    bool sawNewPlant = false;
    for (int frame = 0; frame < 90; ++frame) {
        const auto step = updateEnemyLocomotion(walker, input, flatSupport);
        if (walker.swingFoot == 0 && !walker.left.planted) {
            sawSwing = true;
            assert(std::abs(step.rightFootPosition.x - stancePlant.x) < 0.00001f);
            assert(std::abs(step.rightFootPosition.z - stancePlant.z) < 0.00001f);
        }
        if (sawSwing && walker.swingFoot < 0 && walker.left.planted
            && walker.left.plantPosition.z < -0.10f) {
            sawNewPlant = true;
            break;
        }
    }
    assert(sawSwing);
    assert(sawNewPlant);
    assert(walker.physicalGaitPhase > 3.0f);

    // A route commitment supplied by collision handling survives the direct
    // navigation request long enough for the feet to execute the turn.
    EnemyLocomotionState routed{};
    input.bodyPosition = {};
    input.bodyVelocity = {};
    input.desiredTravelDirection = {0.0f, 0.0f, -1.0f};
    input.desiredSpeed = 1.0f;
    initializeEnemyLocomotion(routed, input, flatSupport);
    routed.committedTravelDirection = {1.0f, 0.0f, 0.0f};
    routed.directionalCommitmentTimer = 0.30f;
    const auto routedOutput = updateEnemyLocomotion(routed, input, flatSupport);
    assert(routed.committedTravelDirection.x > 0.99f);
    assert(routedOutput.supportedDesiredVelocity.x > 0.1f);
    assert(std::abs(routedOutput.supportedDesiredVelocity.z) < 0.01f);

    std::puts("ENEMY_LOCOMOTION_OK planted_feet=WORLD_SPACE step=UNLOAD_SWING_CONTACT_LOAD obstacle=INTENTION");
    return 0;
}
