#include "Game.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

struct EnemyMotorRuntimeIntegrationAccess {
    static const gameplay::EnemyMotorMemory& memory(const Game& game,int index){return game.enemyMotorMemory_[index];}
    static gameplay::EnemyMotorMemory& memory(Game& game,int index){return game.enemyMotorMemory_[index];}
    static void respawn(Game& game,int index){game.respawnTarget(index);}
};

namespace {
constexpr float Dt=1.0f/60.0f;

void configureComparableEnemies(Game& game,bool multiplayer){
    game.reset();
    if(multiplayer)game.configureNetworkHost();else game.disableNetwork();
    auto& state=game.networkMutableState();
    state.uiPaused=false;state.started=true;state.dead=false;
    state.debug.colliderCount=0;state.slopeSupportCount=0;state.rockSupportCount=0;
    state.player.pos={0.0f,0.08f,0.0f};state.player.vel={1.2f,0.0f,-0.6f};state.player.grounded=true;
    state.progression.run.roomHeat=0.65f;state.vacuum.active=true;state.vacuum.power=0.75f;
    for(auto& target:state.targets)target=TargetState{};
    for(int i=0;i<2;++i){
        auto& target=state.targets[i];target.alive=true;target.pos={i==0?-5.0f:5.0f,0.08f,8.0f};
        target.walkTarget=target.pos;target.armor=2.0f;target.health=1.0f;target.attackCooldown=999.0f;
    }
}

std::array<Vec3,2> runSolo(Game& game,int frames){
    for(int frame=0;frame<frames;++frame){
        game.setTouchControls(0,0,0,0,false,false,false,false,false,false);
        game.update(Dt);
    }
    return {game.state().targets[0].pos,game.state().targets[1].pos};
}

bool memoryIsZero(const gameplay::EnemyMotorMemory& memory){
    for(float value:memory.hidden)if(value!=0.0f)return false;
    return true;
}
}

int main(){
    Game first,repeat;
    configureComparableEnemies(first,false);configureComparableEnemies(repeat,false);
    const auto firstPositions=runSolo(first,180);const auto repeatPositions=runSolo(repeat,180);
    assert(!memoryIsZero(EnemyMotorRuntimeIntegrationAccess::memory(first,0)));
    assert(!memoryIsZero(EnemyMotorRuntimeIntegrationAccess::memory(first,1)));
    const Vec3 firstTravel=firstPositions[0]-Vec3{-5.0f,0.08f,8.0f};
    const Vec3 secondTravel=firstPositions[1]-Vec3{5.0f,0.08f,8.0f};
    const float mirroredDifference=std::abs(firstTravel.x+secondTravel.x)+std::abs(firstTravel.z-secondTravel.z);
    assert(mirroredDifference>0.05f);
    for(int i=0;i<2;++i){
        assert(std::abs(firstPositions[i].x-repeatPositions[i].x)<0.00001f);
        assert(std::abs(firstPositions[i].z-repeatPositions[i].z)<0.00001f);
    }

    Game multiplayer,multiplayerControl;
    configureComparableEnemies(multiplayer,true);configureComparableEnemies(multiplayerControl,true);
    EnemyMotorRuntimeIntegrationAccess::memory(multiplayer,0).hidden.fill(0.91f);
    const Vec3 before=multiplayer.state().targets[0].pos;
    const auto multiplayerPositions=runSolo(multiplayer,90);
    const auto controlPositions=runSolo(multiplayerControl,90);
    for(float value:EnemyMotorRuntimeIntegrationAccess::memory(multiplayer,0).hidden)assert(value==0.91f);
    for(int i=0;i<2;++i){
        assert(multiplayerPositions[i].x==controlPositions[i].x);
        assert(multiplayerPositions[i].z==controlPositions[i].z);
    }
    const Vec3 multiplayerTravel=multiplayerPositions[0]-before;

    EnemyMotorRuntimeIntegrationAccess::respawn(first,0);
    assert(memoryIsZero(EnemyMotorRuntimeIntegrationAccess::memory(first,0)));
    first.reset();
    assert(memoryIsZero(EnemyMotorRuntimeIntegrationAccess::memory(first,1)));

    std::printf("ENEMY_MOTOR_RUNTIME_OK deterministic divergence=%.3f solo0=(%.3f,%.3f) solo1=(%.3f,%.3f) multiplayerStep=(%.4f,%.4f) respawn-reset\n",
        mirroredDifference,firstPositions[0].x,firstPositions[0].z,firstPositions[1].x,firstPositions[1].z,multiplayerTravel.x,multiplayerTravel.z);
    return 0;
}
