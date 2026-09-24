#include <cassert>
#include "gameplay/PhysicalEnemyBody.hpp"

int main(){
    using namespace gameplay;
    PhysicalEnemyBodyState body{};
    body.initialized=true;
    body.leftFootPlanted=true; body.rightFootPlanted=true;
    body.leftPlantWeight=1.0f; body.rightPlantWeight=1.0f;
    body.leftFootPlant={-0.12f,0.0f,0.0f}; body.rightFootPlant={0.12f,0.0f,0.0f};
    PhysicalEnemyBodyInput in{}; in.dt=1.0f/60.0f; in.grounded=true;
    in.leftFootContact=1.0f; in.rightFootContact=1.0f; in.bodyPosition={0.55f,0.0f,0.0f};
    in.actualVelocity={0.0f,0.0f,0.0f}; in.desiredVelocity={1.0f,0.0f,0.0f};
    auto out=updatePhysicalEnemyBody(body,in,0.0f);
    assert(out.velocity.x < 1.0f); // planted support resists COM escape
    assert(!body.fallen);
    in.bodyPosition={0.95f,0.0f,0.0f};
    updatePhysicalEnemyBody(body,in,0.0f);
    assert(body.fallen); // no imaginary force holds a body outside support
    return 0;
}
