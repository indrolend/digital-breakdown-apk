#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "Math.hpp"
#include "gameplay/EnemyMotionFacts.hpp"

constexpr float HUMAN_SWING_ATTACK_DURATION = 0.86f;
constexpr float HUMAN_SWING_COMMIT_PHASE = 0.30f;
constexpr float HUMAN_SWING_END_PHASE = 0.72f;

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

inline float humanShellThinningAmount(float armor, float armorMax, bool slurpable) {
    if(slurpable||armorMax<=0.001f)return 0.0f;
    const float remaining=clampf(armor/armorMax,0.0f,1.0f);
    // Start exposing deterministic data-shaped holes after the first meaningful
    // damage and grow them into an unmistakable, still-readable silhouette.
    return smoothStep01(clampf((0.86f-remaining)/0.86f,0.0f,1.0f))*0.44f;
}

inline float humanShellTriangleDataSample(std::size_t triangleIndex) {
    std::uint32_t hash=static_cast<std::uint32_t>(triangleIndex)*747796405u+2891336453u;
    hash=((hash>>((hash>>28u)+4u))^hash)*277803737u;
    hash=(hash>>22u)^hash;
    return static_cast<float>(hash&0x00ffffffu)/16777216.0f;
}

inline bool humanShellTriangleMissing(std::size_t triangleIndex,float thinningAmount) {
    return humanShellTriangleDataSample(triangleIndex)<thinningAmount;
}

inline Vec3 humanShellCritCenter() {
    return {0.0f,PASS7_HUMAN_VISUAL_SPEC.totalHeight-PASS7_HUMAN_VISUAL_SPEC.headRadius,0.0f};
}

inline float humanShellCritInfluence(const Vec3& triangleCenter) {
    const Vec3 delta=triangleCenter-humanShellCritCenter();
    return smoothStep01(1.0f-clampf(std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z)/0.72f,0.0f,1.0f));
}

inline bool humanShellTriangleMissingTowardCrit(std::size_t triangleIndex,float thinningAmount,const Vec3& triangleCenter) {
    const float threshold=clampf(thinningAmount*(0.56f+humanShellCritInfluence(triangleCenter)*1.20f),0.0f,0.72f);
    return humanShellTriangleDataSample(triangleIndex)<threshold;
}

inline Vec3 humanShellAbsorbTowardCrit(const Vec3& vertex,std::size_t triangleIndex,float thinningAmount) {
    if(thinningAmount<=0.001f)return vertex;
    const float damage=clampf(thinningAmount/0.44f,0.0f,1.0f);
    const float channel=0.30f+(1.0f-humanShellTriangleDataSample(triangleIndex))*0.70f;
    return vertex+(humanShellCritCenter()-vertex)*(smoothStep01(damage)*0.075f*channel);
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

inline HumanVisualPose makeHumanVisualPose(float yaw, float scale, float time, const HumanReactionVisual& reaction, bool aliveHuman, const EnemyMotionFacts& motion = {}) {
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

    // Presentation consumes controller facts; it does not rediscover world geometry.
    // This keeps support/contact policy in the controller while allowing the body to
    // visibly tell the truth about slopes, obstruction, slipping, and climbing.
    if (aliveHuman) {
        const float uphill = clampf(1.0f - motion.supportNormal.y, 0.0f, 1.0f);
        const float horizontalMismatch = horizontalLength(motion.desiredVelocity - motion.actualVelocity);
        const float imbalance = clampf(std::max(motion.balanceError, horizontalMismatch * 0.18f), 0.0f, 1.0f);
        const float brace = clampf(std::max(motion.obstructionAmount, motion.impactAmount) + imbalance * 0.45f, 0.0f, 1.0f);
        const float climb = clampf(motion.climbAmount, 0.0f, 1.0f);
        const float climbCycle = std::sin(time * 8.0f + cadence * 0.35f);

        pose.torsoPitch += uphill * 0.34f + motion.slipAmount * 0.18f - (motion.airborne ? 0.08f : 0.0f);
        pose.torsoRoll += clampf(motion.supportNormal.x, -0.7f, 0.7f) * 0.22f;
        pose.headPitch -= uphill * 0.10f;
        pose.leftArmSwing -= brace * 0.55f;
        pose.rightArmSwing -= brace * 0.55f;
        pose.leftArmSwing += imbalance * 0.30f;
        pose.rightArmSwing -= imbalance * 0.30f;

        if (climb > 0.001f) {
            pose.rootBob += std::abs(climbCycle) * 0.018f * pose.scale * climb;
            pose.torsoPitch -= 0.22f * climb;
            pose.leftArmSwing = pose.leftArmSwing + (-1.10f + climbCycle * 0.42f - pose.leftArmSwing) * climb;
            pose.rightArmSwing = pose.rightArmSwing + (-1.10f - climbCycle * 0.42f - pose.rightArmSwing) * climb;
            pose.leftLegSwing = pose.leftLegSwing + (0.52f - climbCycle * 0.35f - pose.leftLegSwing) * climb;
            pose.rightLegSwing = pose.rightLegSwing + (0.52f + climbCycle * 0.35f - pose.rightLegSwing) * climb;
            pose.torsoRoll += climbCycle * 0.07f * climb;
        }

        // Extreme physical expression: the simulation still owns one compact body,
        // while the visible skeleton behaves like a soft, badly controlled puppet.
        // Impacts and airborne motion can fully overwhelm the ordinary walk cycle.
        const float violent = clampf(std::max(motion.impactAmount, motion.slipAmount * 0.82f) + (motion.airborne ? 0.44f : 0.0f), 0.0f, 1.0f);
        if (violent > 0.001f) {
            const float speed = horizontalLength(motion.actualVelocity);
            const float chaosA = std::sin(time * (15.0f + speed * 0.55f) + motion.actualVelocity.x * 0.71f + motion.actualVelocity.z * 0.37f);
            const float chaosB = std::sin(time * (21.0f + speed * 0.31f) - motion.actualVelocity.x * 0.43f + 1.7f);
            const float vertical = clampf(motion.actualVelocity.y / 9.0f, -1.0f, 1.0f);
            pose.rootBob += std::abs(chaosB) * 0.055f * pose.scale * violent;
            pose.torsoPitch += (-vertical * 0.82f + chaosA * 0.48f) * violent;
            pose.torsoRoll += chaosB * 0.86f * violent;
            pose.headPitch += (chaosA * 0.72f - vertical * 0.38f) * violent;
            pose.leftArmSwing += (chaosA * 1.65f + chaosB * 0.72f) * violent;
            pose.rightArmSwing += (-chaosA * 1.52f + chaosB * 0.91f) * violent;
            pose.leftLegSwing += (-chaosB * 1.18f + vertical * 0.48f) * violent;
            pose.rightLegSwing += (chaosB * 1.24f + vertical * 0.42f) * violent;
            pose.hitLean += violent * 0.16f;
        }
    }

    if (aliveHuman && reaction.attackTimer > 0.0f) {
        const float t = 1.0f - clampf(reaction.attackTimer / HUMAN_SWING_ATTACK_DURATION, 0.0f, 1.0f);
        const float windup = std::sin(clampf(t / HUMAN_SWING_COMMIT_PHASE, 0.0f, 1.0f) * DB_PI * 0.5f) * (t < HUMAN_SWING_COMMIT_PHASE ? 1.0f : 0.0f);
        const float sweepT=clampf((t-HUMAN_SWING_COMMIT_PHASE)/(HUMAN_SWING_END_PHASE-HUMAN_SWING_COMMIT_PHASE),0.0f,1.0f);
        const float strike = std::sin(sweepT * DB_PI);
        const float recover = std::sin(clampf((t - HUMAN_SWING_END_PHASE) / (1.0f-HUMAN_SWING_END_PHASE), 0.0f, 1.0f) * DB_PI);
        const float side = reaction.attackVariant % 2 == 0 ? 1.0f : -1.0f;
        const float low = reaction.attackVariant >= 2 ? 1.0f : 0.0f;
        const float reach = smoothStep01(sweepT);
        pose.torsoPitch += windup * 0.10f - reach * (0.24f + low * 0.08f) + recover * 0.07f;
        pose.torsoRoll += side * (strike * 0.18f - windup * 0.24f - recover * 0.06f);
        pose.headPitch += windup * 0.04f - reach * 0.10f;
        const float lead = reach * (1.34f + low * 0.22f) + strike * 0.34f - windup * 0.74f;
        const float rear = -reach * 0.24f + windup * 0.14f;
        if (side > 0) { pose.rightArmSwing -= lead; pose.leftArmSwing += rear; }
        else { pose.leftArmSwing -= lead; pose.rightArmSwing += rear; }
    }
    return pose;
}

inline HumanVisualPose makeHumanVisualPose(float yaw, float scale, float walkPhase, float time, float hitFlash, float soulMorphPhase, bool aliveHuman) {
    const HumanReactionVisual reaction = makeHumanReactionVisual(walkPhase, aliveHuman ? 1.0f : 0.0f, hitFlash, 0.0f, 0.0f, 0.0f, soulMorphPhase, true);
    return makeHumanVisualPose(yaw, scale, time, reaction, aliveHuman);
}
