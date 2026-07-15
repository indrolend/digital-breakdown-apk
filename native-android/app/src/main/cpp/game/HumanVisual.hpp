#pragma once

#include <cmath>

#include "Math.hpp"

struct HumanVisualSpec {
    float totalHeight;
    float shoulderWidth;
    float torsoHeight;
    float torsoWidth;
    float torsoDepth;
    float pelvisHeight;
    float pelvisWidth;
    float pelvisDepth;
    float headRadius;
    float neckHeight;
    float upperArmLength;
    float forearmLength;
    float thighLength;
    float shinLength;
    float footLength;
    float footHeight;
    float handSize;
    float rootGroundOffset;
    float centerOfMassHeight;
    float forwardYawOffset;
    float normalScale;
    float bruteScale;
};

constexpr HumanVisualSpec PASS7_HUMAN_VISUAL_SPEC{
    1.16f,  // totalHeight: Pass 7 HUMAN_MODEL_HEIGHT after FBX normalization.
    0.34f,  // shoulderWidth: proportional reconstruction from normalized FBX height.
    0.38f,
    0.27f,
    0.15f,
    0.16f,
    0.24f,
    0.14f,
    0.105f,
    0.89f,
    0.24f,
    0.22f,
    0.33f,
    0.32f,
    0.19f,
    0.065f,
    0.07f,
    0.0f,
    0.56f,
    DB_PI,
    1.0f,
    1.7f
};

struct HumanVisualPose {
    float yaw = 0.0f;
    float scale = 1.0f;
    float morphShrink = 1.0f;
    float rootBob = 0.0f;
    float torsoPitch = 0.0f;
    float torsoRoll = 0.0f;
    float headPitch = 0.0f;
    float leftArmSwing = 0.0f;
    float rightArmSwing = 0.0f;
    float leftLegSwing = 0.0f;
    float rightLegSwing = 0.0f;
    float hitLean = 0.0f;
    float soulMorph = 0.0f;
};

inline float smoothStep01(float x) {
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

inline HumanVisualPose makeHumanVisualPose(float yaw, float scale, float walkPhase, float time, float hitFlash, float soulMorphPhase, bool aliveHuman) {
    HumanVisualPose pose;
    pose.yaw = yaw + PASS7_HUMAN_VISUAL_SPEC.forwardYawOffset;
    pose.scale = scale;
    pose.soulMorph = smoothStep01(soulMorphPhase);
    pose.morphShrink = aliveHuman ? (1.0f - pose.soulMorph) : 1.0f;
    const float stress = clampf(hitFlash * 0.18f + pose.soulMorph, 0.0f, 1.0f);
    pose.scale *= (1.0f - stress * 0.25f) * std::max(0.0f, pose.morphShrink);

    const float cadence = walkPhase;
    const float stride = std::sin(cadence);
    const float counterStride = std::sin(cadence + DB_PI);
    const float idle = std::sin(time * 3.0f);
    const float active = aliveHuman ? 1.0f : 0.0f;
    pose.rootBob = (0.012f * idle + 0.028f * std::abs(stride) * active) * pose.scale;
    pose.torsoPitch = -0.04f * active + hitFlash * -0.16f;
    pose.torsoRoll = stride * 0.055f * active + hitFlash * 0.18f;
    pose.headPitch = 0.035f * idle - hitFlash * 0.12f;
    pose.leftArmSwing = counterStride * 0.48f * active - 0.08f + hitFlash * 0.32f;
    pose.rightArmSwing = stride * 0.48f * active - 0.08f - hitFlash * 0.32f;
    pose.leftLegSwing = stride * 0.38f * active;
    pose.rightLegSwing = counterStride * 0.38f * active;
    pose.hitLean = hitFlash * 0.08f;
    return pose;
}
