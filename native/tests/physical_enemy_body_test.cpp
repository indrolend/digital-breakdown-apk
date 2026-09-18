#include <cassert>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "gameplay/PhysicalEnemyBody.hpp"

int main() {
    constexpr float dt = 1.0f / 60.0f;
    gameplay::PhysicalEnemyBodyState body{};
    gameplay::PhysicalEnemyBodyInput input{};
    input.desiredVelocity = {0.0f, 0.0f, -1.8f};
    input.desiredYaw = 0.0f;
    input.grounded = true;
    input.dt = dt;

    Vec3 velocity{};
    float yaw = 1.2f;
    for (int frame = 0; frame < 180; ++frame) {
        input.actualVelocity = velocity;
        const auto output = gameplay::updatePhysicalEnemyBody(body, input, yaw);
        velocity = output.velocity;
        yaw = output.yaw;
    }

    assert(body.initialized);
    assert(velocity.z < -1.0f);
    assert(std::abs(yaw) < 0.25f);
    assert(body.gaitPhase > 1.0f);

    // An excessive physical attitude removes locomotor authority. The planner
    // cannot keep translating an enemy whose body has fallen.
    body.bodyPitch = 0.9f;
    input.actualVelocity = {1.2f, 0.0f, 0.0f};
    const auto fallen = gameplay::updatePhysicalEnemyBody(body, input, yaw);
    assert(body.fallen);
    assert(fallen.locomotion == 0.0f);
    assert(fallen.velocity.x < input.actualVelocity.x);

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
