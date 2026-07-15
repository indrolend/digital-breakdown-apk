#include <cassert>
#include <cmath>
#include <iostream>
#include "../../main/cpp/game/HumanVisual.hpp"

int main() {
    const auto& s = PASS7_HUMAN_VISUAL_SPEC;
    assert(std::fabs(s.totalHeight - 1.16f) < 0.0001f);
    assert(std::fabs(s.normalScale - 1.0f) < 0.0001f);
    assert(std::fabs(s.bruteScale - 1.7f) < 0.0001f);
    assert(s.rootGroundOffset == 0.0f);
    assert(s.shoulderWidth > s.torsoWidth);
    assert(s.footLength > s.handSize);
    const HumanVisualPose idle = makeHumanVisualPose(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, true);
    assert(std::fabs(idle.yaw - DB_PI) < 0.0001f);
    assert(idle.scale > 0.99f && idle.scale < 1.01f);
    const HumanVisualPose morph = makeHumanVisualPose(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, true);
    assert(morph.scale <= 0.001f);
    const HumanVisualPose brute = makeHumanVisualPose(0.0f, s.bruteScale, 0.0f, 0.0f, 0.0f, 0.0f, true);
    assert(brute.scale > 1.69f && brute.scale < 1.71f);
    const HumanReactionVisual idleReaction = makeHumanReactionVisual(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true);
    const HumanVisualPose idlePose = makeHumanVisualPose(0.0f, 1.0f, 0.0f, idleReaction, true);
    assert(std::fabs(idlePose.leftLegSwing) < 0.0001f);
    assert(std::fabs(idlePose.rightLegSwing) < 0.0001f);
    const HumanReactionVisual walkReaction = makeHumanReactionVisual(DB_PI * 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true);
    const HumanVisualPose walkPose = makeHumanVisualPose(0.0f, 1.0f, 0.0f, walkReaction, true);
    assert(walkPose.leftArmSwing < 0.0f);
    assert(walkPose.rightArmSwing > 0.0f);
    assert(walkPose.leftLegSwing > 0.0f);
    assert(walkPose.rightLegSwing < 0.0f);
    const HumanReactionVisual hitReaction = makeHumanReactionVisual(0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, true);
    const HumanVisualPose hitPose = makeHumanVisualPose(0.0f, 1.0f, 0.0f, hitReaction, true);
    assert(hitPose.torsoRoll < 0.0f);
    assert(hitPose.hitLean > 0.07f);
    const HumanReactionVisual vacuumReaction = makeHumanReactionVisual(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.8f, 0.0f, true);
    const HumanVisualPose vacuumPose = makeHumanVisualPose(0.0f, 1.0f, 0.0f, vacuumReaction, true);
    assert(vacuumPose.vacuumLean > 0.99f);
    assert(vacuumPose.collapse > 0.85f);
    assert(vacuumPose.torsoPitch > 0.0f);
    const HumanReactionVisual storedReaction = makeHumanReactionVisual(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, false);
    const HumanVisualPose storedPose = makeHumanVisualPose(0.0f, 1.0f, 0.0f, storedReaction, true);
    assert(storedPose.scale <= 0.001f);
    std::cout << "human visual spec ok\n";
    return 0;
}
