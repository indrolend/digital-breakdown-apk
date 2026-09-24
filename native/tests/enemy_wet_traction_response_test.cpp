#include "../game/gameplay/PhysicalEnemyBody.hpp"
#include <cassert>
#include <cstdio>
using namespace gameplay;

static PhysicalEnemyBodyState settledBody() {
    PhysicalEnemyBodyState body{};
    body.initialized=true;
    body.leftFootPlanted=true;
    body.rightFootPlanted=true;
    body.leftPlantWeight=1.0f;
    body.rightPlantWeight=1.0f;
    body.leftFootPlant={-0.16f,0.0f,0.0f};
    body.rightFootPlant={0.16f,0.0f,0.0f};
    return body;
}

int main(){
    PhysicalEnemyBodyInput dry{};
    dry.desiredVelocity={3.0f,0.0f,0.0f};
    dry.actualVelocity={0.0f,0.0f,0.0f};
    dry.bodyPosition={0.0f,0.0f,0.0f};
    dry.centerOfMassHeight=0.9f;
    dry.dt=1.0f/60.0f;
    dry.grounded=true;
    dry.leftFootContact=1.0f; dry.rightFootContact=1.0f;
    dry.leftFootPosition={-0.16f,0.0f,0.0f};
    dry.rightFootPosition={0.16f,0.0f,0.0f};
    dry.supportNormal={0.0f,1.0f,0.0f};
    dry.surfaceTraction=1.0f;
    auto wet=dry; wet.surfaceTraction=0.55f;
    auto dryBody=settledBody(); auto wetBody=settledBody();
    updatePhysicalEnemyBody(dryBody,dry,0.0f);
    updatePhysicalEnemyBody(wetBody,wet,0.0f);
    assert(dryBody.scrambleAmount < 0.001f);
    assert(wetBody.scrambleAmount > 0.20f);
    assert(wetBody.disruption > dryBody.disruption);
    assert(wetBody.disruption < 1.0f);
    std::printf("ENEMY_WET_TRACTION_RESPONSE_OK dry=%.3f wet=%.3f disruption=%.3f\\n",dryBody.scrambleAmount,wetBody.scrambleAmount,wetBody.disruption);
}
