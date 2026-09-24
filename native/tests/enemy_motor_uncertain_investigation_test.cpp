#include "../game/gameplay/EnemyMotor.hpp"
#include <cassert>
#include <cstdio>
using namespace gameplay;
int main(){
    EnemyMotorInput certain{};
    certain.toPlayer={0.2f,0.0f,0.98f}; certain.playerDistance=5.0f;
    certain.nearestAllyDistance=6.0f; certain.traction=1.0f; certain.pursuitCertainty=1.0f;
    EnemyMotorInput uncertain=certain; uncertain.pursuitCertainty=0.22f;
    EnemyMotorMemory a{},b{};
    for(int i=0;i<120;++i){updateEnemyMotor(certain,a,1.0f/60.0f,0.17f);updateEnemyMotor(uncertain,b,1.0f/60.0f,0.17f);}
    const auto chase=updateEnemyMotor(certain,a,1.0f/60.0f,0.17f);
    const auto investigate=updateEnemyMotor(uncertain,b,1.0f/60.0f,0.17f);
    assert(investigate.speedScale<chase.speedScale);
    assert(investigate.attackCommitment<chase.attackCommitment);
    assert(investigate.brace>chase.brace);
    assert(investigate.speedScale>=0.36f);
    std::printf("ENEMY_MOTOR_UNCERTAIN_INVESTIGATION_OK speed %.3f<%.3f attack %.3f<%.3f brace %.3f>%.3f\\n",investigate.speedScale,chase.speedScale,investigate.attackCommitment,chase.attackCommitment,investigate.brace,chase.brace);
}
