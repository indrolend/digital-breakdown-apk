#include "../game/HumanVisual.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
    const HumanReactionVisual calm{};
    const auto forward=makeEnemyVisualPose(0.0f,1.0f,0.0f,calm,true,true,0.31f,0.0f,0.0f,0.0f,0.0f);
    const auto backward=makeEnemyVisualPose(0.0f,1.0f,0.0f,calm,true,true,-0.31f,0.0f,0.0f,0.0f,0.0f);
    const auto upright=makeEnemyVisualPose(0.0f,1.0f,0.0f,calm,true,true,0.0f,0.0f,0.0f,0.0f,0.0f);
    assert(forward.bodyCompression>0.0f&&forward.bodyCompression<0.07f);
    assert(std::abs(forward.bodyCompression-backward.bodyCompression)<0.0001f);
    assert(upright.bodyCompression==0.0f);
    std::printf("ENEMY_VISUAL_PHYSICAL_POSTURE_OK compression=%.4f symmetric=YES upright=ZERO\n",forward.bodyCompression);
}
