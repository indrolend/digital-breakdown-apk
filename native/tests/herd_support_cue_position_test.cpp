#include <cassert>
#include <cmath>
#include <cstdio>
#include "../game/gameplay/PhysicalEnemyBody.hpp"

int main(){
    gameplay::PhysicalEnemyBodyState body{};
    const Vec3 bodyPosition{9.0f,2.0f,-4.0f};
    Vec3 cue=gameplay::physicalSupportCuePosition(body,bodyPosition);
    assert(cue.x==bodyPosition.x&&cue.y==bodyPosition.y&&cue.z==bodyPosition.z);

    body.leftFootPlanted=true; body.leftPlantWeight=1.0f; body.leftFootPlant={1.0f,0.0f,2.0f};
    body.rightFootPlanted=true; body.rightPlantWeight=1.0f; body.rightFootPlant={3.0f,0.0f,2.0f};
    cue=gameplay::physicalSupportCuePosition(body,bodyPosition);
    assert(std::abs(cue.x-2.0f)<0.0001f&&std::abs(cue.y)<0.0001f&&std::abs(cue.z-2.0f)<0.0001f);

    body.leftPlantWeight=0.25f; body.rightPlantWeight=0.75f;
    cue=gameplay::physicalSupportCuePosition(body,bodyPosition);
    assert(std::abs(cue.x-2.5f)<0.0001f);
    std::printf("HERD_SUPPORT_CUE_POSITION_OK %.3f %.3f %.3f\n",cue.x,cue.y,cue.z);
}
