#include "../game/HumanVisual.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
    const float left=physicalSupportLateralShift(1.0f,0.0f,1.0f);
    const float right=physicalSupportLateralShift(0.0f,1.0f,1.0f);
    const float balanced=physicalSupportLateralShift(0.8f,0.8f,1.0f);
    const float weak=physicalSupportLateralShift(0.2f,0.0f,1.0f);
    assert(left<0.0f && right>0.0f);
    assert(std::abs(left+right)<0.00001f);
    assert(std::abs(balanced)<0.00001f);
    assert(std::abs(weak)<std::abs(left));
    assert(std::abs(left)<=0.04501f);
    std::printf("ENEMY_VISUAL_SUPPORT_TRANSFER_OK left=%.4f right=%.4f weak=%.4f\n",left,right,weak);
}
