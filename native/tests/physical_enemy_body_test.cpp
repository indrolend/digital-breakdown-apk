#include <cassert>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "gameplay/PhysicalEnemyBody.hpp"
#include "HumanVisual.hpp"

int main() {
    constexpr float dt = 1.0f / 60.0f;
    gameplay::PhysicalEnemyBodyState body{};

    const HumanReactionVisual calm{};
    const auto embodiedPose=makeEnemyVisualPose(0.4f,1.0f,2.0f,calm,true,true,0.31f,-0.22f,1.7f,0.8f,9.0f);
    assert(std::abs(embodiedPose.animationTime-1.7f)<0.0001f);
    assert(std::abs(embodiedPose.rootPitch-0.31f)<0.0001f);
    assert(std::abs(embodiedPose.rootRoll+0.22f)<0.0001f);
    const auto maturePose=makeEnemyVisualPose(0.4f,1.0f,2.0f,calm,true,false,0.31f,-0.22f,1.7f,0.8f,9.0f);
    assert(std::abs(maturePose.animationTime-9.0f)<0.0001f);
    assert(std::abs(maturePose.rootPitch)<0.0001f&&std::abs(maturePose.rootRoll)<0.0001f);
    gameplay::PhysicalEnemyBodyInput input{};
    input.desiredVelocity = {0.0f, 0.0f, -1.8f};
    input.desiredYaw = 0.0f;
    input.grounded = true;
    input.dt = dt;

    Vec3 velocity{};
    float yaw = 1.2f;
    float gaitTravel = 0.0f;
    float previousGaitPhase = body.gaitPhase;
    for (int frame = 0; frame < 180; ++frame) {
        input.actualVelocity = velocity;
        const auto output = gameplay::updatePhysicalEnemyBody(body, input, yaw);
        velocity = output.velocity;
        yaw = output.yaw;
        float gaitDelta = body.gaitPhase - previousGaitPhase;
        if (gaitDelta < 0.0f) gaitDelta += 2.0f * 3.14159265358979323846f;
        gaitTravel += gaitDelta;
        previousGaitPhase = body.gaitPhase;
    }

    assert(body.initialized);
    assert(velocity.z < -1.0f);
    assert(std::abs(yaw) < 0.25f);
    // Gait phase is cyclic authority, not elapsed time. Verify temporal
    // progression across wraps instead of requiring an arbitrary final phase.
    assert(body.gaitPhase >= 0.0f && body.gaitPhase < 2.0f * 3.14159265358979323846f);
    assert(gaitTravel > 4.0f);

    // A grounded flag is not traction. The environment must report at least
    // one supported foot before the body can accelerate toward motor intent.
    gameplay::PhysicalEnemyBodyState unsupportedBody{};
    gameplay::PhysicalEnemyBodyInput unsupportedInput=input;
    unsupportedInput.actualVelocity={};
    unsupportedInput.leftFootContact=0.0f;
    unsupportedInput.rightFootContact=0.0f;
    const auto unsupported=gameplay::updatePhysicalEnemyBody(unsupportedBody,unsupportedInput,0.0f);
    assert(horizontalLength(unsupported.velocity)<0.0001f);
    unsupportedInput.leftFootContact=1.0f;
    Vec3 supportedVelocity{};
    for(int frame=0;frame<12;++frame){
        unsupportedInput.actualVelocity=supportedVelocity;
        supportedVelocity=gameplay::updatePhysicalEnemyBody(unsupportedBody,unsupportedInput,0.0f).velocity;
    }
    // Support grants acceleration authority over time; it does not promise an
    // arbitrary one-frame impulse.
    assert(horizontalLength(supportedVelocity)>0.05f);

    // World support orientation is physical input: repeated contact on an
    // incline produces a bounded body attitude without changing intent.
    gameplay::PhysicalEnemyBodyState slopeBody{};
    gameplay::PhysicalEnemyBodyInput slopeInput=input;
    slopeInput.actualVelocity={};
    slopeInput.desiredVelocity={};
    slopeInput.supportNormal=normalized(Vec3{0.0f,0.92f,0.38f});
    for(int frame=0;frame<90;++frame)
        gameplay::updatePhysicalEnemyBody(slopeBody,slopeInput,0.0f);
    assert(std::abs(slopeBody.bodyPitch)>0.02f);
    assert(std::abs(slopeBody.bodyPitch)<0.20f);

    // An excessive physical attitude removes locomotor authority. The planner
    // cannot keep translating an enemy whose body has fallen.
    body.bodyPitch = 0.9f;
    input.actualVelocity = {1.2f, 0.0f, 0.0f};
    const auto fallen = gameplay::updatePhysicalEnemyBody(body, input, yaw);
    assert(body.fallen);
    assert(fallen.locomotion == 0.0f);
    assert(fallen.velocity.x < input.actualVelocity.x);

    // A settled fallen body must eventually regain the same posture authority.
    // The fall pose and get-up target may not fight into a permanent tilted
    // equilibrium, which previously left enemies inert indefinitely.
    gameplay::PhysicalEnemyBodyState recoveryBody{};
    recoveryBody.initialized=true;
    recoveryBody.fallen=true;
    recoveryBody.bodyPitch=1.28f;
    recoveryBody.bodyRoll=0.82f;
    gameplay::PhysicalEnemyBodyInput recoveryInput{};
    recoveryInput.dt=dt;
    recoveryInput.grounded=true;
    recoveryInput.leftFootContact=1.0f;
    recoveryInput.rightFootContact=1.0f;
    recoveryInput.leftFootPosition={-0.12f,0.0f,0.0f};
    recoveryInput.rightFootPosition={0.12f,0.0f,0.0f};
    for(int frame=0;frame<360&&recoveryBody.fallen;++frame)
        gameplay::updatePhysicalEnemyBody(recoveryBody,recoveryInput,0.0f);
    assert(!recoveryBody.fallen);
    assert(std::abs(recoveryBody.bodyPitch)<0.20f);
    assert(std::abs(recoveryBody.bodyRoll)<0.20f);

    // Impact changes angular state rather than playing a detached animation.
    const float pitchVelocityBefore = body.pitchVelocity;
    gameplay::applyPhysicalEnemyImpact(body, {0.0f, 0.0f, 3.0f});
    assert(body.pitchVelocity > pitchVelocityBefore);

    // Representative worst-case CPU probe: every pooled enemy updates for one
    // simulated minute. This measures controller cost, not rendering.
    std::array<gameplay::PhysicalEnemyBodyState,32> crowd{};
    std::array<Vec3,32> crowdVelocity{};
    const auto benchmarkStart=std::chrono::steady_clock::now();
    for(int frame=0;frame<3600;++frame){
        for(int index=0;index<32;++index){
            gameplay::PhysicalEnemyBodyInput crowdInput{};
            crowdInput.desiredVelocity={std::sin(index*0.71f)*1.5f,0.0f,-1.2f};
            crowdInput.actualVelocity=crowdVelocity[index];
            crowdInput.desiredYaw=std::sin(frame*0.002f+index)*0.8f;
            crowdInput.individuality=std::sin(index*12.9898f);
            crowdInput.brace=0.5f;
            crowdInput.dt=dt;
            const auto crowdOutput=gameplay::updatePhysicalEnemyBody(crowd[index],crowdInput,0.0f);
            crowdVelocity[index]=crowdOutput.velocity;
        }
    }
    const auto benchmarkEnd=std::chrono::steady_clock::now();
    const double benchmarkMilliseconds=std::chrono::duration<double,std::milli>(benchmarkEnd-benchmarkStart).count();
    const double microsecondsPerEnemyFrame=benchmarkMilliseconds*1000.0/(3600.0*32.0);
    assert(std::isfinite(crowdVelocity[31].x));

    std::puts("PHYSICAL_ENEMY_BODY_TEST_OK");
    std::printf("PHYSICAL_ENEMY_BODY_BENCHMARK crowd=32 simulated_seconds=60 total_ms=%.3f us_per_enemy_frame=%.5f\n",
        benchmarkMilliseconds,microsecondsPerEnemyFrame);
    return 0;
}
