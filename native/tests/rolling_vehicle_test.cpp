#include <cmath>
#include <cstdio>
#include "RollingVehicle.hpp"

namespace {
using namespace rolling_vehicle;

std::array<Probe, WheelCount> flat(float height = 0.0f) {
    std::array<Probe, WheelCount> probes{};
    for (auto& probe : probes) probe = {true, height, {0.0f, 1.0f, 0.0f}};
    return probes;
}

bool finite(const State& state) {
    return std::isfinite(state.pos.x) && std::isfinite(state.pos.y) && std::isfinite(state.pos.z) &&
        std::isfinite(state.vel.x) && std::isfinite(state.vel.y) && std::isfinite(state.vel.z) &&
        std::isfinite(state.yaw) && std::isfinite(state.yawRate);
}

float forwardSpeed(const State& state) { return dot3(state.vel, forward(state.yaw)); }
}

int main() {
    constexpr float dt = 1.0f / 60.0f;
    const auto support = flat();

    State idle{}; idle.active = true; idle.pos.y = CartConfig.rideHeight;
    for (int tick = 0; tick < 600; ++tick) update(idle, {}, support, dt);
    const bool idleStable = finite(idle) && horizontalLength(idle.pos) < 0.0001f && std::abs(idle.yaw) < 0.0001f;

    State accelerating{}; accelerating.active = true; accelerating.pos.y = CartConfig.rideHeight;
    bool monotonic = true; float previousSpeed = 0.0f;
    for (int tick = 0; tick < 240; ++tick) {
        update(accelerating, {1.0f, 0.0f, 0.0f}, support, dt);
        const float speed = forwardSpeed(accelerating);
        monotonic = monotonic && speed + 0.0001f >= previousSpeed;
        previousSpeed = speed;
    }
    const bool accelerates = monotonic && previousSpeed > 4.0f && previousSpeed <= CartConfig.maxForwardSpeed + 0.001f;

    State coasting = accelerating; float coastPrevious = forwardSpeed(coasting); bool coastStable = true;
    for (int tick = 0; tick < 180; ++tick) {
        update(coasting, {}, support, dt);
        const float speed = forwardSpeed(coasting);
        coastStable = coastStable && speed >= -0.0001f && speed <= coastPrevious + 0.0001f;
        coastPrevious = speed;
    }

    State braking = accelerating; const float beforeBrake = forwardSpeed(braking);
    for (int tick = 0; tick < 30; ++tick) update(braking, {-1.0f, 0.0f, 0.0f}, support, dt);
    const bool brakeBeforeReverse = forwardSpeed(braking) < beforeBrake && forwardSpeed(braking) >= -0.05f;
    for (int tick = 0; tick < 120; ++tick) update(braking, {-1.0f, 0.0f, 0.0f}, support, dt);
    const bool reverses = forwardSpeed(braking) < -0.5f;

    State left{}; left.active = true; left.vel = forward(0.0f) * 2.0f; left.pos.y = CartConfig.rideHeight;
    State rightTurn = left;
    for (int tick = 0; tick < 60; ++tick) {
        update(left, {0.4f, -1.0f, 0.0f}, support, dt);
        update(rightTurn, {0.4f, 1.0f, 0.0f}, support, dt);
    }
    const bool steerSymmetric = left.yawRate < 0.0f && rightTurn.yawRate > 0.0f && std::abs(left.yawRate + rightTurn.yawRate) < 0.02f;
    const bool speedSensitive = maxSteer(7.0f) < maxSteer(2.0f);

    State lateral{}; lateral.active = true; lateral.vel = right(0.0f) * 3.0f; lateral.pos.y = CartConfig.rideHeight;
    const float lateralBefore = horizontalLength(lateral.vel);
    for (int tick = 0; tick < 60; ++tick) update(lateral, {}, support, dt);
    const bool lateralDamped = horizontalLength(lateral.vel) < lateralBefore * 0.1f;

    auto outlier = flat(1.0f); outlier[3].height = 1000.0f;
    float reducedHeight = 0.0f; Vec3 reducedNormal{}; int contacts = 0;
    const bool outlierRejected = reduceSupport(outlier, reducedHeight, reducedNormal, contacts) && contacts == 4 && std::abs(reducedHeight - 1.0f) < 0.001f;
    auto partial = flat(); partial[2].supported = false; partial[3].supported = false;
    State partialState{}; partialState.active = true;
    for (int tick = 0; tick < 120; ++tick) update(partialState, {0.5f, 0.2f, 0.0f}, partial, dt);
    const bool partialStable = finite(partialState) && partialState.contactCount == 2;

    State wheels{}; wheels.active = true; wheels.pos.y = CartConfig.rideHeight;
    for (int tick = 0; tick < 60; ++tick) update(wheels, {1.0f, 0.5f, 0.0f}, support, dt);
    const bool presentationDerived = std::abs(wheels.wheelSpin[0]) > 0.1f && std::isfinite(wheels.casterYaw[0]);

    const bool ok = idleStable && accelerates && coastStable && brakeBeforeReverse && reverses && steerSymmetric &&
        speedSensitive && lateralDamped && outlierRejected && partialStable && presentationDerived;
    if (!ok) {
        std::fprintf(stderr, "ROLLING_VEHICLE_FAILED idle=%d accel=%d coast=%d brake=%d reverse=%d symmetry=%d speed_steer=%d lateral=%d outlier=%d partial=%d wheels=%d\n",
            idleStable, accelerates, coastStable, brakeBeforeReverse, reverses, steerSymmetric, speedSensitive,
            lateralDamped, outlierRejected, partialStable, presentationDerived);
        return 1;
    }
    std::printf("ROLLING_VEHICLE_OK idle acceleration coast brake_reverse steering lateral_support caster_spin\n");
    return 0;
}
