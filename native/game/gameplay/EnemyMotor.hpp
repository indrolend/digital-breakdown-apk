#pragma once

#include <array>
#include <algorithm>
#include <cmath>

#include "Game.hpp"

namespace gameplay {

// Experimental solo motor boundary. It owns no combat, collision, navigation,
// networking, animation, or target lifecycle state. The caller supplies local
// perception and receives only bounded movement/commitment signals.
struct EnemyMotorInput {
    Vec3 toPlayer{};          // normalized XZ direction
    float playerDistance = 0.0f;
    Vec3 playerVelocity{};    // world XZ, normalized by caller scale
    Vec3 toNearestAlly{};     // normalized XZ direction
    float nearestAllyDistance = 1.0f;
    float vacuumPressure = 0.0f;
    float roomPressure = 0.0f;
    float bodySpeed = 0.0f;
};

struct EnemyMotorMemory {
    std::array<float, 8> hidden{};
};

struct EnemyMotorOutput {
    Vec3 steering{};
    float speedScale = 1.0f;
    float attackCommitment = 0.0f;
    float brace = 0.0f;
};

inline float motorClamp(float value) {
    return std::max(-1.0f, std::min(1.0f, value));
}

inline EnemyMotorOutput updateEnemyMotor(
    const EnemyMotorInput& input,
    EnemyMotorMemory& memory,
    float dt,
    float individuality)
{
    const float distance = std::min(1.0f, std::max(0.0f, input.playerDistance / 24.0f));
    const float allyDistance = std::min(1.0f, std::max(0.0f, input.nearestAllyDistance / 10.0f));
    const float vacuum = std::min(1.0f, std::max(0.0f, input.vacuumPressure));
    const float pressure = std::min(1.0f, std::max(0.0f, input.roomPressure));
    const float speed = std::min(1.0f, std::max(0.0f, input.bodySpeed / 6.0f));
    const float sideToPlayer = input.toPlayer.z;
    const float crossPlayer = input.toPlayer.x;
    const float sideToAlly = input.toNearestAlly.z;
    const float crossAlly = input.toNearestAlly.x;
    const float personality = motorClamp(individuality);

    // Sparse recurrent core. These are continuous pressures, not named tactics.
    // Persistence lets identical local geometry produce different trajectories
    // depending on the preceding encounter state.
    std::array<float, 8> next{};
    next[0] = std::tanh( 1.35f * crossPlayer - 0.55f * crossAlly + 0.52f * memory.hidden[0] + 0.22f * memory.hidden[3] + 0.24f * personality);
    next[1] = std::tanh( 1.20f * sideToPlayer  - 0.48f * sideToAlly  + 0.56f * memory.hidden[1] - 0.18f * memory.hidden[2]);
    next[2] = std::tanh((1.0f-distance) * 1.10f + pressure * 0.72f + memory.hidden[2] * 0.61f - vacuum * 0.42f);
    next[3] = std::tanh((1.0f-allyDistance) * 0.95f + vacuum * 0.80f + memory.hidden[3] * 0.58f - memory.hidden[4] * 0.21f);
    next[4] = std::tanh(input.playerVelocity.x * 0.46f + input.playerVelocity.z * 0.38f + memory.hidden[4] * 0.64f + personality * 0.28f);
    next[5] = std::tanh(speed * 0.54f + pressure * 0.48f + memory.hidden[5] * 0.68f - vacuum * 0.26f);
    next[6] = std::tanh((1.0f-distance) * 0.74f + vacuum * 0.57f - (1.0f-allyDistance) * 0.31f + memory.hidden[6] * 0.63f);
    next[7] = std::tanh(crossPlayer * sideToAlly * 0.52f - sideToPlayer * crossAlly * 0.52f + memory.hidden[7] * 0.66f + personality * 0.19f);

    const float blend = std::min(1.0f, std::max(0.0f, dt * 18.0f));
    for (int i = 0; i < 8; ++i)
        memory.hidden[i] += (next[i] - memory.hidden[i]) * blend;

    // Convert the latent state into the only signals this prototype is allowed
    // to own. Existing gameplay remains responsible for applying them safely.
    const float tangentX = input.toPlayer.z;
    const float tangentZ = -input.toPlayer.x;
    const float forward = motorClamp(0.72f + memory.hidden[2] * 0.34f - memory.hidden[3] * 0.18f);
    const float lateral = motorClamp(memory.hidden[0] * 0.52f + memory.hidden[7] * 0.46f + personality * 0.12f);
    Vec3 steering{
        input.toPlayer.x * forward + tangentX * lateral,
        0.0f,
        input.toPlayer.z * forward + tangentZ * lateral
    };
    const float magnitude = std::sqrt(steering.x * steering.x + steering.z * steering.z);
    if (magnitude > 0.0001f) {
        steering.x /= magnitude;
        steering.z /= magnitude;
    } else {
        steering = input.toPlayer;
    }

    EnemyMotorOutput output;
    output.steering = steering;
    output.speedScale = std::max(0.58f, std::min(1.42f,
        1.0f + memory.hidden[5] * 0.19f + memory.hidden[2] * 0.17f - memory.hidden[3] * 0.11f));
    output.attackCommitment = std::max(0.0f, std::min(1.0f,
        0.5f + memory.hidden[6] * 0.34f + memory.hidden[2] * 0.24f - vacuum * 0.08f));
    output.brace = std::max(0.0f, std::min(1.0f,
        0.5f + memory.hidden[3] * 0.39f + vacuum * 0.28f - memory.hidden[5] * 0.12f));
    return output;
}

} // namespace gameplay
