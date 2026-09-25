#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

#include "gameplay/EnemyBehaviorState.hpp"
#include "gameplay/EnemyLocomotion.hpp"
#include "gameplay/PhysicalEnemyBody.hpp"

namespace {
constexpr float Dt = 1.0f / 60.0f;

struct Organism {
    gameplay::EnemyBehaviorState behavior{};
    gameplay::EnemyLocomotionState locomotion{};
    gameplay::PhysicalEnemyBodyState body{};
    Vec3 position{};
    Vec3 velocity{};
    float yaw = 0.0f;
};

struct FrameResult {
    gameplay::EnemyBehaviorOutput behavior{};
    gameplay::EnemyLocomotionOutput feet{};
};

FrameResult advance(Organism& organism, const gameplay::EnemyBehaviorInput& evidence) {
    using namespace gameplay;
    FrameResult frame{};
    frame.behavior = updateEnemyBehavior(organism.behavior, evidence);
    const auto flatSupport = [](const Vec3& candidate) {
        return EnemyFootSupport{{candidate.x, 0.0f, candidate.z}, {0.0f, 1.0f, 0.0f}, true};
    };

    EnemyLocomotionInput locomotionInput{};
    locomotionInput.bodyPosition = organism.position;
    locomotionInput.bodyVelocity = organism.velocity;
    locomotionInput.bodyYaw = organism.yaw;
    locomotionInput.bodyPitch = organism.body.bodyPitch;
    locomotionInput.bodyRoll = organism.body.bodyRoll;
    locomotionInput.desiredTravelDirection = frame.behavior.travelScale > 0.0f
        ? Vec3{0.0f, 0.0f, -1.0f} : Vec3{};
    locomotionInput.desiredSpeed = 2.2f * frame.behavior.travelScale;
    locomotionInput.desiredYaw = 0.0f;
    locomotionInput.centerOfMassHeight = 0.90f;
    locomotionInput.traction = 1.0f;
    locomotionInput.dt = Dt;
    locomotionInput.grounded = true;
    locomotionInput.fallen = organism.body.fallen;
    frame.feet = updateEnemyLocomotion(organism.locomotion, locomotionInput, flatSupport);

    PhysicalEnemyBodyInput bodyInput{};
    bodyInput.desiredVelocity = frame.feet.supportedDesiredVelocity;
    bodyInput.actualVelocity = organism.velocity;
    bodyInput.bodyPosition = organism.position;
    bodyInput.predictedCenterOfMass = frame.feet.predictedCenterOfMass;
    bodyInput.hasPredictedCenterOfMass = true;
    bodyInput.centerOfMassHeight = locomotionInput.centerOfMassHeight;
    bodyInput.desiredYaw = frame.feet.desiredYaw;
    bodyInput.dt = Dt;
    bodyInput.grounded = true;
    bodyInput.leftFootContact = frame.feet.leftContact;
    bodyInput.rightFootContact = frame.feet.rightContact;
    bodyInput.leftFootLoad = frame.feet.leftLoad;
    bodyInput.rightFootLoad = frame.feet.rightLoad;
    bodyInput.leftFootPosition = frame.feet.leftFootPosition;
    bodyInput.rightFootPosition = frame.feet.rightFootPosition;
    bodyInput.supportNormal = frame.feet.supportNormal;
    bodyInput.recoveryUrgency = frame.feet.recoveryUrgency;
    bodyInput.correctiveStepActive = frame.feet.correctiveStepActive;
    bodyInput.turnStepActive = frame.feet.turnStepActive;
    bodyInput.supportRecoveryReady = frame.feet.supportRecoveryReady;
    const auto physical = updatePhysicalEnemyBody(organism.body, bodyInput, organism.yaw);
    organism.velocity = physical.velocity;
    organism.yaw = physical.yaw;
    organism.position += organism.velocity * Dt;
    return frame;
}

bool samePlant(const Vec3& a, const Vec3& b) {
    return horizontalLength(a - b) < 0.0001f;
}
}

int main() {
    using namespace gameplay;
    Organism organism{};
    EnemyBehaviorInput quiet{};
    quiet.dt = Dt;

    float maximumRestUrgency = 0.0f;
    for (int frame = 0; frame < 1800; ++frame) {
        const auto result = advance(organism, quiet);
        maximumRestUrgency = std::max(maximumRestUrgency, result.feet.recoveryUrgency);
        assert(result.behavior.mode == EnemyBehaviorMode::Rest);
        assert(organism.locomotion.swingFoot < 0);
        assert(!organism.body.fallen);
    }
    assert(horizontalLength(organism.position) < 0.0001f);
    assert(horizontalLength(organism.velocity) < 0.0001f);
    assert(maximumRestUrgency < 0.001f);
    assert(std::abs(organism.body.bodyPitch) < 0.001f);
    assert(std::abs(organism.body.bodyRoll) < 0.001f);

    EnemyBehaviorInput contact{};
    contact.confidence = 0.85f;
    contact.uncertainty = 0.05f;
    contact.confirmed = true;
    contact.hasSpatialBelief = true;
    contact.dt = Dt;
    Vec3 previousLeft = organism.locomotion.left.plantPosition;
    Vec3 previousRight = organism.locomotion.right.plantPosition;
    Vec3 supportEpochRoot = organism.position;
    float maximumFixedContactTravel = 0.0f;
    float maximumCruiseUrgency = 0.0f;
    int plantChanges = 0;
    for (int frame = 0; frame < 600; ++frame) {
        const auto result = advance(organism, contact);
        const bool plantChanged = !samePlant(previousLeft, organism.locomotion.left.plantPosition)
            || !samePlant(previousRight, organism.locomotion.right.plantPosition);
        if (plantChanged) {
            ++plantChanges;
            previousLeft = organism.locomotion.left.plantPosition;
            previousRight = organism.locomotion.right.plantPosition;
            supportEpochRoot = organism.position;
        } else if (result.feet.leftContact > 0.80f && result.feet.rightContact > 0.80f) {
            maximumFixedContactTravel = std::max(maximumFixedContactTravel,
                horizontalLength(organism.position - supportEpochRoot));
        }
        maximumCruiseUrgency = std::max(maximumCruiseUrgency, result.feet.recoveryUrgency);
        assert(!organism.body.fallen);
        assert(std::abs(organism.body.bodyPitch) < 0.50f);
        assert(std::abs(organism.body.bodyRoll) < 0.50f);
    }
    assert(organism.position.z < -5.0f);
    assert(plantChanges >= 12);
    assert(maximumFixedContactTravel < 0.55f);
    assert(maximumCruiseUrgency < 0.20f);

    std::printf(
        "ENEMY_BEHAVIOR_LONG_HORIZON_OK rest=SETTLED plants=%d fixed_contact_travel=%.3f cruise_urgency=%.3f\n",
        plantChanges, maximumFixedContactTravel, maximumCruiseUrgency);
    return 0;
}
