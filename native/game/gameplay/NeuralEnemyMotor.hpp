#pragma once

#include <array>
#include <cmath>

#include "../Math.hpp"

namespace gameplay {

struct NeuralEnemyMotorOutput {
    float leftArm = 0.0f;
    float rightArm = 0.0f;
    float leftLeg = 0.0f;
    float rightLeg = 0.0f;
    float torsoPitch = 0.0f;
    float torsoRoll = 0.0f;
};

inline NeuralEnemyMotorOutput neuralEnemyMotor(
    float phase,
    float locomotion,
    float hit,
    float vacuum,
    float collapse,
    float attack
) {
    // Tiny fixed network: 7 inputs -> 8 tanh units -> 6 motor targets.
    // The weights are deliberately small and deterministic so this remains
    // cheap enough to run for every enemy every frame. A later trainer can
    // replace only these weights while the runtime contract stays unchanged.
    const std::array<float, 7> input{
        std::sin(phase),
        std::cos(phase),
        clampf(locomotion, 0.0f, 1.0f),
        clampf(hit, 0.0f, 1.0f),
        clampf(vacuum, 0.0f, 1.0f),
        clampf(collapse, 0.0f, 1.0f),
        clampf(attack, 0.0f, 1.0f)
    };

    constexpr float hiddenWeights[8][8] = {
        { 1.28f,  0.14f,  0.88f, -0.31f, -0.24f, -0.48f,  0.38f, -0.06f},
        {-1.20f, -0.18f,  0.91f,  0.27f, -0.22f, -0.44f,  0.35f,  0.04f},
        { 0.20f,  1.34f,  0.72f, -0.20f, -0.12f, -0.30f,  0.14f, -0.12f},
        {-0.18f, -1.31f,  0.75f,  0.18f, -0.10f, -0.28f,  0.16f,  0.10f},
        { 0.62f, -0.46f,  0.42f,  0.78f,  0.20f, -0.36f,  0.44f, -0.08f},
        {-0.58f,  0.49f,  0.40f, -0.74f,  0.18f, -0.34f, -0.42f,  0.06f},
        { 0.16f,  0.24f,  0.34f, -0.12f,  0.92f,  0.70f, -0.18f, -0.04f},
        {-0.10f,  0.12f,  0.26f,  0.36f, -0.24f,  0.64f,  1.02f,  0.02f}
    };

    std::array<float, 8> hidden{};
    for (int h = 0; h < 8; ++h) {
        float value = hiddenWeights[h][7];
        for (int j = 0; j < 7; ++j) value += hiddenWeights[h][j] * input[j];
        hidden[h] = std::tanh(value);
    }

    constexpr float outputWeights[6][9] = {
        {-0.58f,  0.62f, -0.22f,  0.18f,  0.42f, -0.20f, -0.18f, -0.74f, -0.06f},
        { 0.61f, -0.57f,  0.20f, -0.24f, -0.18f,  0.44f, -0.16f, -0.72f, -0.06f},
        { 0.62f, -0.63f,  0.34f, -0.30f,  0.08f, -0.04f, -0.12f, -0.18f,  0.00f},
        {-0.64f,  0.61f, -0.31f,  0.35f, -0.04f,  0.10f, -0.10f, -0.20f,  0.00f},
        {-0.10f, -0.08f, -0.22f, -0.18f,  0.12f,  0.10f,  0.46f,  0.52f, -0.02f},
        { 0.18f, -0.20f,  0.12f, -0.14f,  0.44f, -0.46f,  0.08f,  0.20f,  0.00f}
    };

    std::array<float, 6> output{};
    for (int o = 0; o < 6; ++o) {
        float value = outputWeights[o][8];
        for (int h = 0; h < 8; ++h) value += outputWeights[o][h] * hidden[h];
        output[o] = std::tanh(value);
    }

    NeuralEnemyMotorOutput motor;
    motor.leftArm = output[0];
    motor.rightArm = output[1];
    motor.leftLeg = output[2];
    motor.rightLeg = output[3];
    motor.torsoPitch = output[4];
    motor.torsoRoll = output[5];
    return motor;
}

} // namespace gameplay
