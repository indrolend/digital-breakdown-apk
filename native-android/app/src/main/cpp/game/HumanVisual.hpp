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
    float vacuumLean = 0.0f;
    float collapse = 0.0f;
};

struct HumanReactionVisual {
    float locomotionPhase = 0.0f;
    float locomotionAmount = 0.0f;
    float hitAmount = 0.0f;
    float hitDirectionLocal = 0.0f;
    float vacuumPullAmount = 0.0f;
    float captureCollapseAmount = 0.0f;
    float visibility = 1.0f;
    float soulCubeAmount = 0.0f;
    float attackTimer = 0.0f;
    int attackVariant = 0;
};

inline float smoothStep01(float x) {
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

inline HumanReactionVisual makeHumanReactionVisual(
    float walkPhase,
    float locomotionAmount,
    float hitFlash,
    float hitDirectionLocal,
    float vacuumPullAmount,
    float captureProgress,
    float soulMorphPhase,
    bool humanVisible,
    float attackTimer = 0.0f,
    int attackVariant = 0
) {
    HumanReactionVisual visual;
    visual.locomotionPhase = walkPhase;
    visual.locomotionAmount = clampf(locomotionAmount, 0.0f, 1.0f);
    visual.hitAmount = clampf(hitFlash, 0.0f, 1.0f);
    visual.hitDirectionLocal = clampf(hitDirectionLocal, -1.0f, 1.0f);
    visual.vacuumPullAmount = clampf(vacuumPullAmount, 0.0f, 1.0f);
    visual.captureCollapseAmount = smoothStep01(captureProgress);
    visual.visibility = humanVisible ? 1.0f : 0.0f;
    visual.soulCubeAmount = smoothStep01(soulMorphPhase);
    visual.attackTimer = std::max(0.0f, attackTimer);
    visual.attackVariant = std::max(0, std::min(3, attackVariant));
    return visual;
}

inline HumanVisualPose makeHumanVisualPose(float yaw, float scale, float time, const HumanReactionVisual& reaction, bool aliveHuman) {
    HumanVisualPose pose;
    pose.yaw = yaw + PASS7_HUMAN_VISUAL_SPEC.forwardYawOffset;
    pose.scale = scale;
    pose.soulMorph = reaction.soulCubeAmount;
    pose.morphShrink = aliveHuman ? (1.0f - pose.soulMorph) : 1.0f;
    const float stress = clampf(reaction.hitAmount * 0.18f + pose.soulMorph, 0.0f, 1.0f);
    pose.scale *= (1.0f - stress * 0.25f) * std::max(0.0f, pose.morphShrink);
    pose.scale *= reaction.visibility;

    const float cadence = reaction.locomotionPhase;
    const float stride = std::sin(cadence);
    const float counterStride = std::sin(cadence + DB_PI);
    const float idle = std::sin(time * 3.0f);
    const float active = aliveHuman ? reaction.locomotionAmount : 0.0f;
    pose.collapse = reaction.captureCollapseAmount;
    pose.vacuumLean = reaction.vacuumPullAmount;
    pose.rootBob = (0.012f * idle + 0.028f * std::abs(stride) * active) * pose.scale;
    pose.torsoPitch = -0.04f * active - reaction.hitAmount * 0.16f - reaction.vacuumPullAmount * 0.20f + pose.collapse * 0.42f;
    pose.torsoRoll = stride * 0.055f * active + reaction.hitDirectionLocal * reaction.hitAmount * 0.18f;
    pose.headPitch = 0.035f * idle - reaction.hitAmount * 0.12f + pose.collapse * 0.24f;
    const float armTrail = reaction.vacuumPullAmount * 0.28f + pose.collapse * 0.42f;
    pose.leftArmSwing = counterStride * 0.46f * active - 0.08f + reaction.hitDirectionLocal * reaction.hitAmount * 0.32f - armTrail;
    pose.rightArmSwing = stride * 0.46f * active - 0.08f - reaction.hitDirectionLocal * reaction.hitAmount * 0.32f - armTrail;
    pose.leftLegSwing = stride * 0.36f * active - pose.collapse * 0.24f;
    pose.rightLegSwing = counterStride * 0.36f * active - pose.collapse * 0.24f;
    pose.hitLean = reaction.hitAmount * 0.08f;
    if (aliveHuman && reaction.attackTimer > 0.0f) {
        const float t = 1.0f - clampf(reaction.attackTimer / 0.48f, 0.0f, 1.0f);
        const float windup = std::sin(clampf(t / 0.28f, 0.0f, 1.0f) * DB_PI) * (t < 0.35f ? 1.0f : 0.0f);
        const float strike = std::sin(clampf((t - 0.18f) / 0.38f, 0.0f, 1.0f) * DB_PI);
        const float recover = std::sin(clampf((t - 0.48f) / 0.52f, 0.0f, 1.0f) * DB_PI);
        const float impact = std::max(strike, recover * 0.35f);
        const float side = reaction.attackVariant % 2 == 0 ? 1.0f : -1.0f;
        const float low = reaction.attackVariant >= 2 ? 1.0f : 0.0f;
        pose.torsoPitch += -impact * (0.30f + low * 0.08f) + windup * 0.12f;
        pose.torsoRoll += side * (strike * 0.18f - windup * 0.08f);
        pose.headPitch += -impact * 0.12f;
        const float lead = strike * (1.35f + low * 0.28f) - windup * 0.55f;
        const float rear = -strike * 0.38f + windup * 0.20f;
        if (side > 0) { pose.rightArmSwing -= lead; pose.leftArmSwing += rear; }
        else { pose.leftArmSwing -= lead; pose.rightArmSwing += rear; }
    }
    return pose;
}

inline HumanVisualPose makeHumanVisualPose(float yaw, float scale, float walkPhase, float time, float hitFlash, float soulMorphPhase, bool aliveHuman) {
    const HumanReactionVisual reaction = makeHumanReactionVisual(walkPhase, aliveHuman ? 1.0f : 0.0f, hitFlash, 0.0f, 0.0f, 0.0f, soulMorphPhase, true);
    return makeHumanVisualPose(yaw, scale, time, reaction, aliveHuman);
}
