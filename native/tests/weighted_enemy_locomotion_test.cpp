#include <cassert>
#include <cmath>
#include "gameplay/PhysicalEnemyBody.hpp"
#include "gameplay/EnemyMotor.hpp"

int main() {
    constexpr float dt = 1.0f / 60.0f;

    gameplay::PhysicalEnemyBodyState body{};
    gameplay::PhysicalEnemyBodyInput in{};
    in.desiredVelocity = {0.0f, 0.0f, -3.0f};
    in.leftFootContact = 1.0f;
    in.rightFootContact = 0.0f;
    in.dt = dt;

    auto first = gameplay::updatePhysicalEnemyBody(body, in, 0.0f);
    // A planted leg begins moving the mass, but cannot teleport it to intent.
    assert(first.velocity.z < 0.0f && first.velocity.z > -0.25f);

    float yaw = 0.0f;
    float previousPhase = body.gaitPhase;
    float maxCadence = 0.0f;
    Vec3 velocity = first.velocity;
    for (int frame = 0; frame < 180; ++frame) {
        in.actualVelocity = velocity;
        in.leftFootContact = frame % 120 < 60 ? 1.0f : 0.0f;
        in.rightFootContact = frame % 120 < 60 ? 0.0f : 1.0f;
        const auto out = gameplay::updatePhysicalEnemyBody(body, in, yaw);
        velocity = out.velocity;
        yaw = out.yaw;
        float delta = body.gaitPhase - previousPhase;
        if (delta < 0.0f) delta += 6.28318530718f;
        maxCadence = std::max(maxCadence, delta / dt);
        previousPhase = body.gaitPhase;
    }
    assert(maxCadence < 6.21f);
    assert(horizontalLength(velocity) > 2.0f);

    // A reversal must retain momentum on its first frame.
    in.actualVelocity = {0.0f, 0.0f, -3.0f};
    in.desiredVelocity = {0.0f, 0.0f, 3.0f};
    in.leftFootContact = 1.0f;
    in.rightFootContact = 0.0f;
    const auto reverse = gameplay::updatePhysicalEnemyBody(body, in, yaw);
    assert(reverse.velocity.z < 0.0f);

    // Animal AI pace remains bounded even under deep-room pressure.
    gameplay::EnemyMotorMemory memory{};
    gameplay::EnemyMotorInput motor{};
    motor.toPlayer = {0.0f,0.0f,-1.0f};
    motor.playerDistance = 3.0f;
    motor.roomPressure = 2.0f;
    motor.traction = 1.0f;
    float maxScale = 0.0f;
    for (int i=0;i<600;++i) {
        const auto out = gameplay::updateEnemyMotor(motor,memory,dt,0.37f);
        maxScale = std::max(maxScale,out.speedScale);
    }
    assert(maxScale <= 1.1601f);
    return 0;
}
