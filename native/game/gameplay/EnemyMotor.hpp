#pragma once

#include <array>
#include <algorithm>
#include <cmath>

#include "Math.hpp"

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
    float pursuitProgress = 0.0f;   // signed velocity toward the player, normalized
    float traction = 1.0f;          // supported-foot authority
    float physicalDisruption = 0.0f;// tilt, falling, or loss of support
    float pursuitCertainty = 1.0f;   // confidence that the spatial belief is actually the player
};

struct EnemyMotorMemory {
    std::array<float, 8> hidden{};
    // Slowly changing motor biases: turn, pace, brace, spacing, pressure, retreat.
    // They are consequences accumulated by one creature, never shared state.
    std::array<float, 6> tendency{};
    float disruptionExposure = 0.0f;
    // Animal state is deliberately tiny and local. It gives each body a
    // changing internal condition without granting perfect knowledge.
    float arousal = 0.0f;
    float caution = 0.0f;
    float fixation = 0.0f;
    float animalPhase = 0.0f;
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

inline float finiteMotorValue(float value, float fallback = 0.0f) {
    return std::isfinite(value) ? value : fallback;
}

inline EnemyMotorOutput updateEnemyMotor(
    const EnemyMotorInput& input,
    EnemyMotorMemory& memory,
    float dt,
    float individuality)
{
    for (float& value : memory.hidden)
        value = motorClamp(finiteMotorValue(value));
    for (float& value : memory.tendency)
        value = motorClamp(finiteMotorValue(value));
    memory.disruptionExposure = std::max(0.0f, std::min(1.0f,
        finiteMotorValue(memory.disruptionExposure)));
    memory.arousal = std::max(0.0f, std::min(1.0f, finiteMotorValue(memory.arousal)));
    memory.caution = std::max(0.0f, std::min(1.0f, finiteMotorValue(memory.caution)));
    memory.fixation = std::max(0.0f, std::min(1.0f, finiteMotorValue(memory.fixation)));
    memory.animalPhase = finiteMotorValue(memory.animalPhase);

    const float distance = std::min(1.0f, std::max(0.0f, finiteMotorValue(input.playerDistance) / 24.0f));
    const float allyDistance = std::min(1.0f, std::max(0.0f, finiteMotorValue(input.nearestAllyDistance, 10.0f) / 10.0f));
    const float vacuum = std::min(1.0f, std::max(0.0f, finiteMotorValue(input.vacuumPressure)));
    // Pressure may exceed one in deep endless rooms. Motor outputs remain
    // bounded; extra pressure changes decision quality/persistence rather than
    // becoming unbounded movement power.
    const float pressure = std::min(2.0f, std::max(0.0f, finiteMotorValue(input.roomPressure)));
    const float pressure01 = std::min(1.0f, pressure);
    const float deepPressure = std::max(0.0f, pressure - 1.0f);
    const float speed = std::min(1.0f, std::max(0.0f, finiteMotorValue(input.bodySpeed) / 6.0f));
    const float progress = motorClamp(finiteMotorValue(input.pursuitProgress));
    const float traction = std::min(1.0f, std::max(0.0f, finiteMotorValue(input.traction, 1.0f)));
    const float disruption = std::min(1.0f, std::max(0.0f, finiteMotorValue(input.physicalDisruption)));
    const float certainty = std::min(1.0f, std::max(0.0f, finiteMotorValue(input.pursuitCertainty, 1.0f)));
    const float sideToPlayer = motorClamp(finiteMotorValue(input.toPlayer.z));
    const float crossPlayer = motorClamp(finiteMotorValue(input.toPlayer.x));
    const float sideToAlly = motorClamp(finiteMotorValue(input.toNearestAlly.z));
    const float crossAlly = motorClamp(finiteMotorValue(input.toNearestAlly.x));
    const float playerVelocityX = motorClamp(finiteMotorValue(input.playerVelocity.x));
    const float playerVelocityZ = motorClamp(finiteMotorValue(input.playerVelocity.z));
    const float rawPersonality = motorClamp(finiteMotorValue(individuality));
    const float personality = rawPersonality * 0.12f;
    // Stable temperament dimensions from one scalar identity. These are not
    // difficulty classes: every creature remains an individual at every room.
    const float boldness = 0.5f + 0.5f * std::sin(rawPersonality * 5.73f + 0.71f);
    const float curiosity = 0.5f + 0.5f * std::sin(rawPersonality * 9.17f + 2.03f);
    const float sociability = 0.5f + 0.5f * std::sin(rawPersonality * 13.31f - 0.44f);
    const float patience = 0.5f + 0.5f * std::sin(rawPersonality * 7.11f + 4.21f);

    // Internal condition. Close contact and room pressure raise arousal;
    // disruption teaches caution; successful pursuit creates fixation. The
    // state relaxes slowly, producing stalking, hesitation and re-engagement.
    const float animalDt = std::min(0.05f, std::max(0.0f, finiteMotorValue(dt)));
    const float proximity = 1.0f - distance;
    const float trustedProximity = proximity * (0.22f + certainty * 0.78f);
    const float arousalTarget = std::min(1.0f, trustedProximity * (0.38f + boldness * 0.42f)
        + pressure01 * 0.30f + deepPressure * 0.18f + vacuum * 0.20f);
    const float cautionTarget = std::min(1.0f, disruption * 0.72f + vacuum * (1.0f - traction) * 0.55f
        + (1.0f - boldness) * 0.18f);
    const float fixationTarget = std::min(1.0f, std::max(0.0f, progress) * 0.58f * certainty
        + trustedProximity * (0.18f + patience * 0.22f) + pressure01 * 0.20f + deepPressure * 0.12f);
    memory.arousal += (arousalTarget - memory.arousal) * animalDt * (1.7f + pressure01 * 0.8f);
    memory.caution += (cautionTarget - memory.caution) * animalDt * 1.25f;
    memory.fixation += (fixationTarget - memory.fixation) * animalDt * (0.85f + patience * 0.75f);
    memory.arousal = std::max(0.0f, std::min(1.0f, memory.arousal));
    memory.caution = std::max(0.0f, std::min(1.0f, memory.caution));
    memory.fixation = std::max(0.0f, std::min(1.0f, memory.fixation));
    memory.animalPhase = std::fmod(memory.animalPhase + animalDt *
        (0.42f + curiosity * 0.63f + memory.arousal * 0.74f + pressure01 * 0.31f), DB_PI * 2.0f);

    // Local plasticity. Recent motor activity supplies a compact eligibility
    // trace; useful pressure is reinforced, while disruption pushes against
    // the activity that preceded it. All habits continuously decay, so old
    // competence and dysfunction can both regress.
    const float learningDt = std::min(0.05f, std::max(0.0f, finiteMotorValue(dt)));
    const float closeAlly = 1.0f - allyDistance;
    const float useful = std::max(0.0f, progress) * (1.0f - disruption);
    const float adverse = std::max(disruption, vacuum * (1.0f - traction));
    const float adverseLesson = adverse * (1.0f - memory.disruptionExposure);
    memory.disruptionExposure += (adverse * (1.0f - memory.disruptionExposure) * 2.5f
                                  - (1.0f - adverse) * memory.disruptionExposure * 0.18f) * learningDt;
    memory.disruptionExposure = std::max(0.0f, std::min(1.0f,
        finiteMotorValue(memory.disruptionExposure)));
    const std::array<float, 6> lesson{
        -adverseLesson * memory.hidden[0] + useful * memory.hidden[7] * 0.35f,
        useful - adverseLesson * (0.65f + speed * 0.35f),
        adverseLesson - useful * 0.18f,
        closeAlly * (adverseLesson - useful * 0.22f),
        useful - adverseLesson * 0.55f,
        adverseLesson * (0.55f + (1.0f - distance) * 0.45f) - useful * 0.30f
    };
    for (int i = 0; i < 6; ++i) {
        memory.tendency[i] += (lesson[i] * 0.72f - memory.tendency[i] * 0.055f) * learningDt;
        memory.tendency[i] = motorClamp(finiteMotorValue(memory.tendency[i]));
    }

    // Sparse recurrent core. These are continuous pressures, not named tactics.
    // Persistence lets identical local geometry produce different trajectories
    // depending on the preceding encounter state.
    std::array<float, 8> next{};
    next[0] = std::tanh( 1.35f * crossPlayer - 0.55f * crossAlly + 0.52f * memory.hidden[0] + 0.22f * memory.hidden[3] + 0.24f * personality);
    next[1] = std::tanh( 1.20f * sideToPlayer  - 0.48f * sideToAlly  + 0.56f * memory.hidden[1] - 0.18f * memory.hidden[2]);
    next[2] = std::tanh((1.0f-distance) * 1.10f + pressure01 * 0.72f + deepPressure * 0.22f + memory.arousal * 0.34f + memory.hidden[2] * 0.61f - vacuum * 0.42f);
    next[3] = std::tanh((1.0f-allyDistance) * 0.95f + vacuum * 0.80f + memory.hidden[3] * 0.58f - memory.hidden[4] * 0.21f);
    next[4] = std::tanh(playerVelocityX * 0.46f + playerVelocityZ * 0.38f + memory.hidden[4] * 0.64f + personality * 0.28f);
    next[5] = std::tanh(speed * 0.54f + pressure01 * 0.48f + deepPressure * 0.18f + memory.fixation * 0.24f + memory.hidden[5] * 0.68f - vacuum * 0.26f);
    next[6] = std::tanh((1.0f-distance) * 0.74f + vacuum * 0.57f - (1.0f-allyDistance) * 0.31f + memory.hidden[6] * 0.63f);
    next[7] = std::tanh(crossPlayer * sideToAlly * 0.52f - sideToPlayer * crossAlly * 0.52f + memory.hidden[7] * 0.66f + personality * 0.19f);

    const float blend = std::min(1.0f, learningDt * 18.0f);
    for (int i = 0; i < 8; ++i)
        memory.hidden[i] += (next[i] - memory.hidden[i]) * blend;

    // Convert the latent state into the only signals this prototype is allowed
    // to own. Existing gameplay remains responsible for applying them safely.
    const float tangentX = sideToPlayer;
    const float tangentZ = -crossPlayer;
    const float forward = motorClamp(0.72f + memory.hidden[2] * 0.34f - memory.hidden[3] * 0.18f
                                     + memory.tendency[4] * 0.20f - memory.tendency[5] * 0.27f);
    // A living lateral rhythm produces circling and probing. Caution can make
    // the same animal peel away; fixation and deep pressure gradually suppress
    // indecision without making every creature converge on one tactic.
    const float animalProbe = std::sin(memory.animalPhase + rawPersonality * 2.9f)
        * (0.10f + curiosity * 0.22f) * (1.0f - memory.fixation * 0.48f);
    const float socialOrbit = std::sin(memory.animalPhase * 0.71f + rawPersonality * 5.1f)
        * sociability * closeAlly * 0.18f;
    const float lateral = motorClamp(memory.hidden[0] * 0.52f + memory.hidden[7] * 0.46f
                                     + personality * 0.12f + memory.tendency[0] * 0.38f
                                     + memory.tendency[3] * closeAlly * 0.24f
                                     + animalProbe + socialOrbit);
    Vec3 steering{
        crossPlayer * forward + tangentX * lateral,
        0.0f,
        sideToPlayer * forward + tangentZ * lateral
    };
    const float magnitude = std::sqrt(steering.x * steering.x + steering.z * steering.z);
    if (magnitude > 0.0001f) {
        steering.x /= magnitude;
        steering.z /= magnitude;
    } else {
        steering = {crossPlayer, 0.0f, sideToPlayer};
    }

    EnemyMotorOutput output;
    output.steering = steering;
    const float hesitation = memory.caution * (1.0f - boldness * 0.55f)
        * (0.55f + 0.45f * std::sin(memory.animalPhase * 0.53f + 1.4f));
    // Pace is deliberately conservative. Aggression should improve the
    // creature's commitment and interception, not turn its walk cycle into a
    // weightless high-frequency shuffle. Arousal can still produce a distinct
    // pursuit gait, but ordinary stalking remains slow and readable.
    output.speedScale = std::max(0.36f, std::min(1.16f,
        0.72f + memory.hidden[5] * 0.12f + memory.hidden[2] * 0.10f - memory.hidden[3] * 0.09f
        + memory.tendency[1] * 0.14f - memory.tendency[5] * 0.14f
        + memory.arousal * 0.09f + memory.fixation * 0.08f + deepPressure * 0.025f - hesitation * 0.18f
        - disruption * 0.26f - (1.0f-certainty) * 0.16f));
    output.attackCommitment = std::max(0.0f, std::min(1.0f,
        0.34f + memory.hidden[6] * 0.28f + memory.hidden[2] * 0.18f
        + memory.arousal * (0.16f + boldness * 0.14f) + memory.fixation * 0.20f
        + pressure01 * 0.10f + deepPressure * 0.08f - memory.caution * 0.18f - vacuum * 0.06f
        - disruption * 0.52f - (1.0f-certainty) * 0.34f));
    output.brace = std::max(0.0f, std::min(1.0f,
        0.5f + memory.hidden[3] * 0.39f + vacuum * 0.28f - memory.hidden[5] * 0.12f
        + memory.tendency[2] * 0.32f + disruption * 0.46f + (1.0f-certainty) * 0.10f));
    return output;
}

} // namespace gameplay
