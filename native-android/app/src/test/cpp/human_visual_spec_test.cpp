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
    std::cout << "human visual spec ok\n";
    return 0;
}
