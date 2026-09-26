#pragma once

#include <algorithm>
#include <cmath>

#include "Math.hpp"

namespace gameplay {

enum class EnemyIntentionMode : unsigned char { RelentlessZombie, ExistingMotor };

struct RelentlessZombieInput {
    Vec3 toData{};
    float dataDistance = 0.0f;
    bool dataValid = false;
};

struct RelentlessZombieOutput {
    Vec3 steering{};
    float speedScale = 0.0f;
    float attackCommitment = 0.0f;
    float brace = 0.62f;
    bool pursuing = false;
};

inline RelentlessZombieOutput relentlessZombieMotor(const RelentlessZombieInput& input) {
    RelentlessZombieOutput output{};
    if (!input.dataValid || !std::isfinite(input.dataDistance)) return output;
    const Vec3 horizontal{input.toData.x, 0.0f, input.toData.z};
    const float distance=std::sqrt(horizontal.x*horizontal.x+horizontal.z*horizontal.z);
    if (distance>0.0001f)
        output.steering=horizontal*(1.0f/distance);
    output.speedScale=1.0f;
    output.attackCommitment=1.0f;
    output.pursuing=true;
    return output;
}

struct ZombieV1Telemetry {
    bool active = false;
    bool relentless = false;
    Vec3 dataPosition{};
    Vec3 requestedSteering{};
    float requestedSpeed = 0.0f;
    Vec3 actualVelocity{};
    Vec3 leftFoot{};
    Vec3 rightFoot{};
    Vec3 swingTarget{};
    Vec3 projectedCenterOfMass{};
    Vec3 supportCenter{};
    float dataDistance = 0.0f;
    float progress = 0.0f;
    float leftLoad = 0.0f;
    float rightLoad = 0.0f;
    float supportError = 0.0f;
    float disruption = 0.0f;
    float traction = 0.0f;
    bool fallen = false;
    bool recovering = false;
};

} // namespace gameplay
