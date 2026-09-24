#include <cassert>
#include <cmath>
#include "gameplay/PhysicalEnemyBody.hpp"

int main() {
    using namespace gameplay;
    constexpr float dt = 1.0f / 60.0f;

    // Lean moves the projected COM. With a narrow single-foot support, a large
    // lean must become a fall rather than a visual-only root rotation.
    PhysicalEnemyBodyState leaning{};
    leaning.initialized = true;
    leaning.leftFootPlanted = true;
    leaning.leftPlantWeight = 1.0f;
    leaning.leftFootPlant = {0.0f, 0.0f, 0.0f};
    leaning.bodyPitch = 0.70f;

    PhysicalEnemyBodyInput leanInput{};
    leanInput.dt = dt;
    leanInput.grounded = true;
    leanInput.bodyPosition = {0.0f, 0.0f, 0.0f};
    leanInput.centerOfMassHeight = 1.0f;
    leanInput.leftFootContact = 1.0f;
    leanInput.rightFootContact = 0.0f;
    leanInput.leftFootPosition = {0.0f, 0.0f, 0.0f};
    updatePhysicalEnemyBody(leaning, leanInput, 0.0f);
    assert(leaning.fallen);

    // A planner may request an arbitrary heading, but without planted support
    // the body is not allowed to generate new turning torque.
    PhysicalEnemyBodyState airborne{};
    airborne.initialized = true;
    PhysicalEnemyBodyInput airInput{};
    airInput.dt = dt;
    airInput.grounded = false;
    airInput.desiredYaw = 1.57079632679f;
    float yaw = 0.0f;
    for (int i = 0; i < 30; ++i)
        yaw = updatePhysicalEnemyBody(airborne, airInput, yaw).yaw;
    assert(std::abs(yaw) < 0.0001f);

    // Contact sensing initializes support inside the body authority itself.
    PhysicalEnemyBodyState grounded{};
    PhysicalEnemyBodyInput groundInput{};
    groundInput.dt = dt;
    groundInput.grounded = true;
    groundInput.bodyPosition = {0.0f,0.0f,0.0f};
    groundInput.leftFootContact = 1.0f;
    groundInput.rightFootContact = 1.0f;
    groundInput.leftFootPosition = {-0.12f,0.0f,0.0f};
    groundInput.rightFootPosition = {0.12f,0.0f,0.0f};
    groundInput.desiredVelocity = {0.0f,0.0f,-2.0f};
    const auto moved = updatePhysicalEnemyBody(grounded, groundInput, 0.0f);
    assert(grounded.leftFootPlanted && grounded.rightFootPlanted);
    assert(moved.velocity.z < 0.0f);

    // Single-foot support should become visible body weight transfer through
    // the same COM/support authority. With the left plant carrying the body,
    // roll develops toward that support instead of leaving the shell centered
    // while only horizontal catch acceleration reacts.
    PhysicalEnemyBodyState singleSupport{};
    singleSupport.initialized = true;
    singleSupport.leftFootPlanted = true;
    singleSupport.leftPlantWeight = 1.0f;
    singleSupport.leftFootPlant = {-0.16f,0.0f,0.0f};
    PhysicalEnemyBodyInput singleInput{};
    singleInput.dt = dt;
    singleInput.grounded = true;
    singleInput.bodyPosition = {0.0f,0.0f,0.0f};
    singleInput.leftFootContact = 1.0f;
    singleInput.rightFootContact = 0.0f;
    singleInput.leftFootPosition = {-0.16f,0.0f,0.0f};
    for (int i = 0; i < 12; ++i)
        updatePhysicalEnemyBody(singleSupport, singleInput, 0.0f);
    assert(singleSupport.bodyRoll < -0.002f);
    assert(singleSupport.bodyRoll > -0.14f);

    return 0;
}
