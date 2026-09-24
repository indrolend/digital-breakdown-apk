#include <cassert>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "gameplay/EnemyMotor.hpp"
#include "gameplay/SoulMotion.hpp"
#include "gameplay/TargetRoles.hpp"
#include "VisualIdentity.hpp"

namespace {

TargetState makeHuman() {
    TargetState target{};
    target.alive = true;
    target.slurpable = false;
    target.soulState = SoulState::Free;
    return target;
}

TargetState makeSoul(SoulState state = SoulState::Free) {
    TargetState target{};
    target.alive = true;
    target.slurpable = true;
    target.soulState = state;
    return target;
}

void testRolePredicates() {
    TargetState human = makeHuman();
    assert(gameplay::isActiveHuman(human));
    assert(gameplay::isCombatTarget(human));
    assert(!gameplay::isLooseSoul(human));
    assert(!gameplay::isVacuumTarget(human));

    TargetState soul = makeSoul();
    assert(!gameplay::isActiveHuman(soul));
    assert(!gameplay::isCombatTarget(soul));
    assert(gameplay::isLooseSoul(soul));
    assert(gameplay::isVacuumTarget(soul));
    assert(gameplay::isFreeVacuumOffer(soul));

    soul.captureQueued = true;
    assert(!gameplay::isLooseSoul(soul));
    assert(!gameplay::isVacuumTarget(soul));

    TargetState recoiling = makeSoul(SoulState::Recoiling);
    assert(gameplay::isLooseSoul(recoiling));
    assert(!gameplay::isVacuumTarget(recoiling));

    TargetState revolving = makeSoul(SoulState::Revolving);
    assert(gameplay::isLooseSoul(revolving));
    assert(!gameplay::isVacuumTarget(revolving));
}

void testFreeSoulMotion() {
    TargetState soul = makeSoul();
    soul.pos = {0.0f, 1.0f, 0.0f};
    soul.vel = {2.0f, 0.0f, -1.0f};
    gameplay::updateLooseSoulMotion(soul, 0.1f);
    assert(soul.pos.x > 0.0f);
    assert(soul.pos.y < 1.0f);
    assert(soul.pos.z < 0.0f);
    assert(soul.vel.y < 0.0f);
    assert(std::abs(soul.vel.x) < 2.0f);
    assert(std::abs(soul.vel.z) < 1.0f);
}

void testRecoilTimeout() {
    TargetState soul = makeSoul(SoulState::Recoiling);
    soul.recoilTime = 0.05f;
    soul.networkOwnerPlayerId = 2;
    soul.pos = {0.0f, 0.5f, 0.0f};
    gameplay::updateLooseSoulMotion(soul, 0.1f);
    assert(soul.soulState == SoulState::Free);
    assert(soul.networkOwnerPlayerId == -1);
    assert(soul.recoilTime == 0.0f);
}

void testGroundClampAndStop() {
    TargetState soul = makeSoul();
    soul.pos = {0.0f, 0.081f, 0.0f};
    soul.vel = {0.01f, -1.0f, -0.01f};
    gameplay::updateLooseSoulMotion(soul, 0.1f);
    assert(std::abs(soul.pos.y - 0.08f) < 0.0001f);
    assert(soul.vel.y == 0.0f);
    assert(soul.vel.x == 0.0f);
    assert(soul.vel.z == 0.0f);
}

void testVacuumOwnedStatesDoNotMove() {
    for (const SoulState state : {SoulState::Attracted, SoulState::Latched, SoulState::Ingesting, SoulState::Revolving}) {
        TargetState soul = makeSoul(state);
        soul.pos = {1.0f, 2.0f, 3.0f};
        soul.vel = {4.0f, 5.0f, 6.0f};
        const Vec3 originalPos = soul.pos;
        const Vec3 originalVel = soul.vel;
        gameplay::updateLooseSoulMotion(soul, 0.25f);
        assert(soul.pos.x == originalPos.x && soul.pos.y == originalPos.y && soul.pos.z == originalPos.z);
        assert(soul.vel.x == originalVel.x && soul.vel.y == originalVel.y && soul.vel.z == originalVel.z);
    }
}

void testIngestingSoulShellContractsContinuously() {
    const SoulVisualState start = makeSoulVisualState(3, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, true);
    const SoulVisualState middle = makeSoulVisualState(3, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, true);
    const SoulVisualState late = makeSoulVisualState(3, 1.0f, 0.8f, 0.0f, 1.0f, 0.0f, true);
    assert(std::abs(start.morphScale - 1.0f) < 0.0001f);
    assert(std::abs(middle.morphScale - 0.5f) < 0.0001f);
    assert(late.morphScale < middle.morphScale);
    assert(late.morphScale > 0.0f);
    const SoulVisualState freeSoul = makeSoulVisualState(0, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, true);
    const SoulVisualState attractedSoul = makeSoulVisualState(1, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, true);
    const SoulVisualState latchedSoul = makeSoulVisualState(2, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, true);
    assert(freeSoul.shellOpacity > attractedSoul.shellOpacity);
    assert(attractedSoul.shellOpacity > latchedSoul.shellOpacity);
    assert(late.shellOpacity < start.shellOpacity);
}

gameplay::EnemyMotorOutput settleMotor(gameplay::EnemyMotorInput input, float individuality) {
    gameplay::EnemyMotorMemory memory{};
    gameplay::EnemyMotorOutput output{};
    for (int step = 0; step < 120; ++step)
        output = gameplay::updateEnemyMotor(input, memory, 1.0f / 60.0f, individuality);
    return output;
}

void testEnemyMotorIsDeterministicBoundedAndStateful() {
    gameplay::EnemyMotorInput input{};
    input.toPlayer = {0.0f, 0.0f, -1.0f};
    input.playerDistance = 7.0f;
    input.playerVelocity = {0.45f, 0.0f, -0.20f};
    input.toNearestAlly = {1.0f, 0.0f, 0.0f};
    input.nearestAllyDistance = 1.3f;
    input.roomPressure = 0.65f;
    input.vacuumPressure = 0.0f;
    input.bodySpeed = 2.4f;

    gameplay::EnemyMotorMemory a{}, b{};
    gameplay::EnemyMotorOutput first{};
    for (int step = 0; step < 90; ++step) {
        first = gameplay::updateEnemyMotor(input, a, 1.0f / 60.0f, 0.22f);
        const auto same = gameplay::updateEnemyMotor(input, b, 1.0f / 60.0f, 0.22f);
        assert(std::abs(first.steering.x - same.steering.x) < 0.000001f);
        assert(std::abs(first.steering.z - same.steering.z) < 0.000001f);
        assert(std::abs(first.speedScale - same.speedScale) < 0.000001f);
        assert(std::abs(first.attackCommitment - same.attackCommitment) < 0.000001f);
        assert(std::abs(first.brace - same.brace) < 0.000001f);
    }
    const float steeringMagnitude = std::sqrt(first.steering.x * first.steering.x + first.steering.z * first.steering.z);
    assert(std::abs(steeringMagnitude - 1.0f) < 0.0001f);
    assert(first.speedScale >= 0.52f && first.speedScale <= 1.50f);
    assert(first.attackCommitment >= 0.0f && first.attackCommitment <= 1.0f);
    assert(first.brace >= 0.0f && first.brace <= 1.0f);
    assert(std::abs(first.steering.x) > 0.02f);
}

void testEnemyMotorRespondsToContinuousPressure() {
    gameplay::EnemyMotorInput base{};
    base.toPlayer = {0.6f, 0.0f, -0.8f};
    base.playerDistance = 4.5f;
    base.playerVelocity = {-0.3f, 0.0f, 0.4f};
    base.toNearestAlly = {-0.8f, 0.0f, -0.6f};
    base.nearestAllyDistance = 0.9f;
    base.roomPressure = 0.8f;
    base.bodySpeed = 3.0f;

    auto calmInput = base;
    calmInput.vacuumPressure = 0.0f;
    const auto calm = settleMotor(calmInput, -0.35f);
    auto vacuumInput = base;
    vacuumInput.vacuumPressure = 1.0f;
    const auto vacuum = settleMotor(vacuumInput, -0.35f);
    const auto other = settleMotor(vacuumInput, 0.65f);

    assert(vacuum.brace > calm.brace);
    const float vacuumHeadingDelta = std::sqrt(
        std::pow(vacuum.steering.x - calm.steering.x, 2.0f) +
        std::pow(vacuum.steering.z - calm.steering.z, 2.0f));
    const float individualityDelta = std::sqrt(
        std::pow(other.steering.x - vacuum.steering.x, 2.0f) +
        std::pow(other.steering.z - vacuum.steering.z, 2.0f));
    assert(vacuumHeadingDelta > 0.005f);
    assert(individualityDelta > 0.005f);

    // Endless pressure increases persistence/aggression without unbounded output.
    auto deepInput = base;
    deepInput.roomPressure = 1.8f;
    const auto deep = settleMotor(deepInput, -0.35f);
    assert(deep.attackCommitment > calm.attackCommitment);
    assert(deep.speedScale >= 0.52f && deep.speedScale <= 1.50f);

    std::printf(
        "ENEMY_MOTOR_PROBE social_lateral=%.6f vacuum_heading_delta=%.6f "
        "vacuum_brace_delta=%.6f individuality_heading_delta=%.6f "
        "attack_commitment=%.6f speed_scale=%.6f\n",
        std::abs(calm.steering.x), vacuumHeadingDelta,
        vacuum.brace - calm.brace, individualityDelta,
        vacuum.attackCommitment, vacuum.speedScale);
}

void testEnemyMotorLearnsConsequencesAndRemainsSafe() {
    gameplay::EnemyMotorInput experience{};
    experience.toPlayer = {0.6f, 0.0f, -0.8f};
    experience.playerDistance = 3.5f;
    experience.toNearestAlly = {-1.0f, 0.0f, 0.0f};
    experience.nearestAllyDistance = 0.7f;
    experience.bodySpeed = 3.0f;

    gameplay::EnemyMotorMemory effective{}, disrupted{}, repeat{};
    gameplay::EnemyMotorOutput effectiveOutput{}, disruptedOutput{};
    for (int frame = 0; frame < 360; ++frame) {
        experience.pursuitProgress = 0.55f;
        experience.traction = 1.0f;
        experience.physicalDisruption = 0.0f;
        experience.vacuumPressure = 0.0f;
        effectiveOutput = gameplay::updateEnemyMotor(experience, effective, 1.0f / 60.0f, 0.0f);
        gameplay::updateEnemyMotor(experience, repeat, 1.0f / 60.0f, 0.0f);

        experience.pursuitProgress = -0.15f;
        experience.traction = 0.0f;
        experience.physicalDisruption = 1.0f;
        experience.vacuumPressure = 1.0f;
        disruptedOutput = gameplay::updateEnemyMotor(experience, disrupted, 1.0f / 60.0f, 0.0f);
    }
    assert(effective.tendency == repeat.tendency);
    assert(effective.tendency[1] > disrupted.tendency[1]);
    assert(disrupted.tendency[2] > effective.tendency[2]);
    assert(disruptedOutput.brace > effectiveOutput.brace);
    assert(disruptedOutput.speedScale < effectiveOutput.speedScale);

    // The same neutral situation now produces different output because the
    // creatures arrived with different histories, not selected behavior states.
    experience.pursuitProgress = 0.0f;
    experience.traction = 1.0f;
    experience.physicalDisruption = 0.0f;
    experience.vacuumPressure = 0.0f;
    const auto afterEffective = gameplay::updateEnemyMotor(experience, effective, 1.0f / 60.0f, 0.0f);
    const auto afterDisruption = gameplay::updateEnemyMotor(experience, disrupted, 1.0f / 60.0f, 0.0f);
    assert(std::abs(afterEffective.speedScale - afterDisruption.speedScale) > 0.05f);
    assert(std::abs(afterEffective.brace - afterDisruption.brace) > 0.05f);

    // Old habits decay gradually rather than being erased by an ordinary tick.
    const float retained = disrupted.tendency[2];
    for (int frame = 0; frame < 60; ++frame)
        gameplay::updateEnemyMotor(experience, disrupted, 1.0f / 60.0f, 0.0f);
    assert(disrupted.tendency[2] > 0.0f && disrupted.tendency[2] < retained);

    gameplay::EnemyMotorInput hostile = experience;
    hostile.toPlayer = {NAN, 0.0f, INFINITY};
    hostile.playerVelocity = {NAN, INFINITY, -INFINITY};
    hostile.playerDistance = INFINITY;
    hostile.bodySpeed = NAN;
    hostile.physicalDisruption = INFINITY;
    disrupted.hidden[4] = NAN;
    disrupted.tendency[3] = INFINITY;
    disrupted.disruptionExposure = NAN;
    for (int frame = 0; frame < 10000; ++frame) {
        const auto output = gameplay::updateEnemyMotor(hostile, disrupted, 1.0f / 60.0f, NAN);
        assert(std::isfinite(output.steering.x) && std::isfinite(output.steering.z));
        assert(std::isfinite(output.speedScale) && output.speedScale >= 0.58f && output.speedScale <= 1.42f);
        assert(std::isfinite(output.attackCommitment) && output.attackCommitment >= 0.0f && output.attackCommitment <= 1.0f);
        assert(std::isfinite(output.brace) && output.brace >= 0.0f && output.brace <= 1.0f);
        for (float value : disrupted.hidden) assert(std::isfinite(value) && std::abs(value) <= 1.0f);
        for (float value : disrupted.tendency) assert(std::isfinite(value) && std::abs(value) <= 1.0f);
        assert(std::isfinite(disrupted.disruptionExposure));
        assert(disrupted.disruptionExposure >= 0.0f && disrupted.disruptionExposure <= 1.0f);
    }
}

void testEnemyMotorCrowdCostIsTiny() {
    std::array<gameplay::EnemyMotorMemory, 32> crowd{};
    gameplay::EnemyMotorInput input{};
    input.toPlayer = {0.0f, 0.0f, -1.0f};
    input.toNearestAlly = {1.0f, 0.0f, 0.0f};
    input.playerDistance = 5.0f;
    input.nearestAllyDistance = 1.0f;
    input.traction = 1.0f;
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < 3600; ++frame)
        for (int index = 0; index < 32; ++index) {
            input.pursuitProgress = static_cast<float>((index % 3) - 1) * 0.25f;
            gameplay::updateEnemyMotor(input, crowd[index], 1.0f / 60.0f, 0.0f);
        }
    const double milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    const double microsecondsPerEnemyFrame = milliseconds * 1000.0 / (3600.0 * 32.0);
    assert(std::isfinite(crowd[31].tendency[0]));
    assert(microsecondsPerEnemyFrame < 50.0);
    std::printf("ENEMY_MOTOR_BENCHMARK crowd=32 simulated_seconds=60 total_ms=%.3f us_per_enemy_frame=%.5f\n",
        milliseconds, microsecondsPerEnemyFrame);
}

} // namespace

int main() {
    testRolePredicates();
    testFreeSoulMotion();
    testRecoilTimeout();
    testGroundClampAndStop();
    testVacuumOwnedStatesDoNotMove();
    testIngestingSoulShellContractsContinuously();
    testEnemyMotorIsDeterministicBoundedAndStateful();
    testEnemyMotorRespondsToContinuousPressure();
    testEnemyMotorLearnsConsequencesAndRemainsSafe();
    testEnemyMotorCrowdCostIsTiny();
    return 0;
}
