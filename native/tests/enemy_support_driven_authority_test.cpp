#include "gameplay/PhysicalEnemyBody.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace gameplay;
    PhysicalEnemyBodyState body{};
    PhysicalEnemyBodyInput input{};
    input.dt=1.0f/60.0f;
    input.grounded=true;
    input.supportDrivenLocomotion=true;
    input.desiredVelocity={2.0f,0.0f,0.0f};
    input.actualVelocity={};
    input.leftFootContact=input.rightFootContact=1.0f;
    input.leftFootPosition={0.0f,0.0f,0.0f};
    input.rightFootPosition={0.30f,0.0f,0.0f};
    input.leftFootLoad=1.0f;
    input.rightFootLoad=0.0f;
    auto anchored=updatePhysicalEnemyBody(body,input,0.0f);
    assert(horizontalLength(anchored.velocity)<0.0001f);

    // A motor request alone still cannot move the root while loaded support is fixed.
    anchored=updatePhysicalEnemyBody(body,input,0.0f);
    assert(horizontalLength(anchored.velocity)<0.0001f);

    // Root motion appears only when load transfers onto the advanced contact.
    input.leftFootLoad=0.0f;
    input.rightFootLoad=1.0f;
    const auto transferred=updatePhysicalEnemyBody(body,input,0.0f);
    assert(transferred.velocity.x>0.05f);
    assert(transferred.velocity.x<=2.21f);
    std::cout<<"ENEMY_SUPPORT_DRIVEN_AUTHORITY_OK intent=NO_TRANSLATION transfer=EARNS_TRANSLATION\n";
}
