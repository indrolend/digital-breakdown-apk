#include <cassert>
#include <cmath>
#include <cstdio>
#include "../game/gameplay/EnemyPerception.hpp"
#include "../game/gameplay/PhysicalEnemyBody.hpp"

int main(){
    gameplay::PhysicalEnemyBodyState body{};
    body.leftFootPlanted=true; body.leftPlantWeight=1.0f; body.leftFootPlant={1.0f,0.0f,2.0f};
    body.rightFootPlanted=true; body.rightPlantWeight=1.0f; body.rightFootPlant={3.0f,0.0f,2.0f};
    const Vec3 bodyPosition{2.0f,1.2f,2.0f};
    const Vec3 support=gameplay::physicalSupportCuePosition(body,bodyPosition);
    const float movement=gameplay::herdMovementContactActivity(4.0f,1.0f,1.0f);
    const float disruption=gameplay::herdDisruptionContactActivity(0.8f,1.0f,1.0f);
    const float floorActivity=std::max(movement,disruption);
    assert(floorActivity>0.0f);
    assert(std::abs(support.y)<0.0001f);
    const float injuryActivity=0.52f;
    assert(injuryActivity>floorActivity);
    assert(bodyPosition.y>support.y);
    const float softDisruption=gameplay::herdDisruptionContactActivity(0.8f,1.0f,0.35f);
    assert(softDisruption<disruption);
    std::printf("HERD_CUE_ORIGIN_AUTHORITY_OK floor %.3f injury %.3f soft %.3f\n",floorActivity,injuryActivity,softDisruption);
}
