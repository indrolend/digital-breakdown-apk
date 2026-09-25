#include "../game/HumanVisual.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
    const HumanReactionVisual calm{};
    const auto forward=makeEnemyVisualPose(0.0f,1.0f,0.0f,calm,true,true,0.31f,0.0f,0.0f,0.0f,0.0f);
    const auto backward=makeEnemyVisualPose(0.0f,1.0f,0.0f,calm,true,true,-0.31f,0.0f,0.0f,0.0f,0.0f);
    const auto upright=makeEnemyVisualPose(0.0f,1.0f,0.0f,calm,true,true,0.0f,0.0f,0.0f,0.0f,0.0f);
    const auto crouched=makeEnemyVisualPose(0.0f,1.0f,0.0f,calm,true,true,0.0f,0.0f,0.0f,0.0f,0.0f,0.8f);
    assert(forward.bodyCompression>0.0f&&forward.bodyCompression<0.07f);
    assert(std::abs(forward.bodyCompression-backward.bodyCompression)<0.0001f);
    assert(upright.bodyCompression==0.0f);
    assert(crouched.bodyCompression>forward.bodyCompression);
    // Whole-body physical lean belongs to the rendered root. The torso must not
    // repeat that same angle or a 20-degree support lean appears roughly doubled.
    assert(std::abs(forward.rootPitch-0.31f)<0.0001f);
    assert(std::abs(forward.human.torsoPitch)<0.0001f);
    assert(std::abs(backward.human.torsoPitch)<0.0001f);
    HumanReactionVisual hit{};hit.hitAmount=1.0f;hit.hitDirectionLocal=1.0f;
    const auto expressive=makeEnemyVisualPose(0.0f,1.0f,0.0f,hit,true,true,0.0f,0.0f,0.0f,0.0f,0.0f);
    assert(expressive.expressiveScale.x>1.0f&&expressive.expressiveScale.y<1.0f&&expressive.expressiveScale.z>1.0f);
    std::printf("ENEMY_VISUAL_PHYSICAL_POSTURE_OK compression=%.4f symmetric=YES upright=ZERO\n",forward.bodyCompression);
}
