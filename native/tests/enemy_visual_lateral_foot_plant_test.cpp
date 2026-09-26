#include <cassert>
#include <cmath>
#include "../game/HumanModelData.hpp"

int main() {
    const float centered = humanLegLateralPlantAngle(0.0f);
    const float left = humanLegLateralPlantAngle(-0.16f);
    const float right = humanLegLateralPlantAngle(0.16f);
    const float clamped = humanLegLateralPlantAngle(9.0f);
    assert(std::abs(centered) < 0.00001f);
    assert(left < 0.0f && right > 0.0f);
    assert(std::abs(left + right) < 0.00001f);
    assert(clamped < 0.50f);
    assert(std::isfinite(humanLegLateralPlantAngle(NAN)));
    return 0;
}
