#include "../game/gameplay/EnemyMotor.hpp"
#include <cassert>
#include <cstdio>
using namespace gameplay;
int main(){
    EnemyMotorInput stable{};
    stable.toPlayer={0.25f,0.0f,0.97f}; stable.playerDistance=5.0f;
    stable.playerVelocity={0.1f,0.0f,0.0f}; stable.nearestAllyDistance=5.0f;
    stable.roomPressure=0.55f; stable.bodySpeed=1.8f; stable.pursuitProgress=0.45f;
    stable.traction=1.0f; stable.physicalDisruption=0.0f;
    EnemyMotorInput disrupted=stable; disrupted.physicalDisruption=1.0f;
    EnemyMotorMemory a{},b{};
    // Warm identical histories first, then discriminate only on current body state.
    for(int i=0;i<90;++i){ updateEnemyMotor(stable,a,1.0f/60.0f,0.21f); updateEnemyMotor(stable,b,1.0f/60.0f,0.21f); }
    const auto calm=updateEnemyMotor(stable,a,1.0f/60.0f,0.21f);
    const auto recovery=updateEnemyMotor(disrupted,b,1.0f/60.0f,0.21f);
    assert(recovery.speedScale < calm.speedScale);
    assert(recovery.attackCommitment < calm.attackCommitment);
    assert(recovery.brace > calm.brace);
    assert(recovery.speedScale >= 0.36f && recovery.attackCommitment >= 0.0f && recovery.brace <= 1.0f);
    std::printf("ENEMY_MOTOR_PHYSICAL_RECOVERY_OK speed %.3f<%.3f attack %.3f<%.3f brace %.3f>%.3f\n",
        recovery.speedScale,calm.speedScale,recovery.attackCommitment,calm.attackCommitment,recovery.brace,calm.brace);
}
