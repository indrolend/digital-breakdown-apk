#include "Game.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

struct EnemyMotorRuntimeIntegrationAccess {
    static const gameplay::EnemyMotorMemory& memory(const Game& game,int index){return game.enemyRuntime_->motors[index];}
    static gameplay::EnemyMotorMemory& memory(Game& game,int index){return game.enemyRuntime_->motors[index];}
    static const gameplay::PhysicalEnemyBodyState& body(const Game& game,int index){return game.enemyRuntime_->bodies[index];}
    static const gameplay::EnemyPerceptionState& perception(const Game& game,int index){return game.enemyRuntime_->perceptions[index];}
    static gameplay::EnemyPerceptionState& perception(Game& game,int index){return game.enemyRuntime_->perceptions[index];}
    static int perceptionCursor(const Game& game){return game.enemyRuntime_->perceptionCursor;}
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
    for(float value:memory.tendency)if(value!=0.0f)return false;
    return memory.disruptionExposure==0.0f&&memory.arousal==0.0f&&memory.caution==0.0f&&
           memory.fixation==0.0f&&memory.animalPhase==0.0f;
}

void configurePursuitLaw(Game& game,float playerZ){
    game.reset();game.disableNetwork();
    auto& state=game.networkMutableState();
    state.uiPaused=false;state.started=true;state.dead=false;
    state.debug.colliderCount=0;state.slopeSupportCount=0;state.rockSupportCount=0;
    state.player.pos={0.0f,0.08f,playerZ};state.player.vel={};state.player.grounded=true;
    state.vacuum.active=false;state.progression.run.roomHeat=0.0f;
    for(auto& target:state.targets)target=TargetState{};
    auto& target=state.targets[0];target.alive=true;target.pos={0.0f,0.08f,0.0f};
    target.visualYaw=DB_PI;
    target.walkTarget={0.0f,0.08f,12.0f};target.armor=2.0f;target.health=1.0f;target.attackCooldown=999.0f;
}

void configureSeamSight(Game& game){
    game.reset();game.disableNetwork();auto& state=game.networkMutableState();
    state.uiPaused=false;state.started=true;state.dead=false;state.debug.colliderCount=0;state.slopeSupportCount=0;state.rockSupportCount=0;
    state.player.pos={0.0f,0.08f,21.1f};state.player.vel={};state.player.grounded=true;
    for(auto& target:state.targets)target=TargetState{};
    auto& target=state.targets[0];target.alive=true;target.pos={0.0f,0.08f,20.9f};target.walkTarget=target.pos;
    target.visualYaw=DB_PI;target.armor=2.0f;target.health=1.0f;target.attackCooldown=999.0f;
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
        const auto& target=first.state().targets[i];
        assert(target.physicalBodyMarker<0.0f);
        assert(EnemyMotorRuntimeIntegrationAccess::body(first,i).initialized);
        // Physical embodiment must not borrow loose-soul/capture storage.
        assert(target.capture==0.0f);
        assert(target.latticeVisualPull==0.0f);
        assert(target.latticeVisualPullVelocity==0.0f);
        assert(target.tetherWidth==0.0f);
        const auto& body=EnemyMotorRuntimeIntegrationAccess::body(first,i);
        assert(std::max(body.leftPlantWeight,body.rightPlantWeight)>0.20f);
        assert(std::max(target.physicalLeftFootWeight,target.physicalRightFootWeight)>0.20f);
    }

    Game multiplayer,multiplayerControl;
    configureComparableEnemies(multiplayer,true);configureComparableEnemies(multiplayerControl,true);
    EnemyMotorRuntimeIntegrationAccess::memory(multiplayer,0).hidden.fill(0.91f);
    EnemyMotorRuntimeIntegrationAccess::memory(multiplayer,0).tendency.fill(-0.73f);
    EnemyMotorRuntimeIntegrationAccess::memory(multiplayer,0).disruptionExposure=0.42f;
    EnemyMotorRuntimeIntegrationAccess::perception(multiplayer,0).confidence=0.67f;
    EnemyMotorRuntimeIntegrationAccess::perception(multiplayer,0).lastSeenPosition={31.0f,2.0f,-17.0f};
    const Vec3 before=multiplayer.state().targets[0].pos;
    const auto multiplayerPositions=runSolo(multiplayer,90);
    const auto controlPositions=runSolo(multiplayerControl,90);
    for(float value:EnemyMotorRuntimeIntegrationAccess::memory(multiplayer,0).hidden)assert(value==0.91f);
    for(float value:EnemyMotorRuntimeIntegrationAccess::memory(multiplayer,0).tendency)assert(value==-0.73f);
    assert(EnemyMotorRuntimeIntegrationAccess::memory(multiplayer,0).disruptionExposure==0.42f);
    assert(EnemyMotorRuntimeIntegrationAccess::perception(multiplayer,0).confidence==0.67f);
    assert(EnemyMotorRuntimeIntegrationAccess::perception(multiplayer,0).lastSeenPosition.x==31.0f);
    for(int i=0;i<2;++i){
        assert(multiplayerPositions[i].x==controlPositions[i].x);
        assert(multiplayerPositions[i].z==controlPositions[i].z);
    }
    const Vec3 multiplayerTravel=multiplayerPositions[0]-before;

    Game wandering,pursuing;
    configurePursuitLaw(wandering,80.0f);configurePursuitLaw(pursuing,12.0f);
    runSolo(wandering,180);runSolo(pursuing,180);
    const float wanderingPressure=EnemyMotorRuntimeIntegrationAccess::memory(wandering,0).tendency[4];
    const float pursuingPressure=EnemyMotorRuntimeIntegrationAccess::memory(pursuing,0).tendency[4];
    assert(wanderingPressure<=0.001f);
    assert(pursuingPressure>wanderingPressure+0.02f);

    // Z tiles repeat, but X is bounded and does not wrap. A short sightline
    // across the Z seam must stay short in the observer's local tile frame.
    Game seamClear;configureSeamSight(seamClear);runSolo(seamClear,1);
    assert(EnemyMotorRuntimeIntegrationAccess::perception(seamClear,0).confirmed);

    Game seamUnrelated;configureSeamSight(seamUnrelated);auto& unrelatedState=seamUnrelated.networkMutableState();
    unrelatedState.debug.colliderCount=1;auto& unrelated=unrelatedState.roomColliders[0];unrelated={};
    unrelated.minX=-1.0f;unrelated.maxX=1.0f;unrelated.minZ=-0.2f;unrelated.maxZ=0.2f;unrelated.bottomY=0.0f;unrelated.topY=3.0f;
    runSolo(seamUnrelated,1);
    assert(EnemyMotorRuntimeIntegrationAccess::perception(seamUnrelated,0).confirmed);

    Game seamBlocked;configureSeamSight(seamBlocked);auto& seamBlockedState=seamBlocked.networkMutableState();
    seamBlockedState.debug.colliderCount=1;auto& seamBlocker=seamBlockedState.roomColliders[0];seamBlocker={};
    seamBlocker.minX=-1.0f;seamBlocker.maxX=1.0f;seamBlocker.minZ=-20.98f;seamBlocker.maxZ=-20.92f;seamBlocker.bottomY=0.0f;seamBlocker.topY=3.0f;
    runSolo(seamBlocked,1);
    assert(!EnemyMotorRuntimeIntegrationAccess::perception(seamBlocked,0).confirmed);
    assert(EnemyMotorRuntimeIntegrationAccess::perception(seamBlocked,0).confidence==0.0f);

    // Acquire in front, then put a hard wall between creature and player.
    // Hidden live motion must not refresh the last-confirmed spatial belief.
    Game occluded;configurePursuitLaw(occluded,8.0f);runSolo(occluded,48);
    assert(EnemyMotorRuntimeIntegrationAccess::perception(occluded,0).confirmed);
    auto& occludedState=occluded.networkMutableState();
    occludedState.debug.colliderCount=1;auto& wall=occludedState.roomColliders[0];wall={};
    wall.minX=-3.0f;wall.maxX=3.0f;wall.minZ=3.8f;wall.maxZ=4.2f;wall.bottomY=0.0f;wall.topY=3.0f;
    wall.width=6.0f;wall.depth=0.4f;wall.height=3.0f;wall.center={0.0f,1.5f,4.0f};
    runSolo(occluded,48);
    assert(!EnemyMotorRuntimeIntegrationAccess::perception(occluded,0).confirmed);
    const Vec3 lastSeen=EnemyMotorRuntimeIntegrationAccess::perception(occluded,0).lastSeenPosition;
    occludedState.player.pos.x=2.5f;occludedState.player.pos.z=8.0f;occludedState.player.vel={4.0f,0.0f,0.0f};
    runSolo(occluded,64);
    const auto& hidden=EnemyMotorRuntimeIntegrationAccess::perception(occluded,0);
    assert(hidden.lastSeenPosition.x==lastSeen.x&&hidden.lastSeenPosition.z==lastSeen.z);
    assert(hidden.lastSeenPosition.x!=occludedState.player.pos.x);
    assert(EnemyMotorRuntimeIntegrationAccess::perceptionCursor(occluded)>=0&&EnemyMotorRuntimeIntegrationAccess::perceptionCursor(occluded)<TARGET_COUNT);

    Game hiddenMirror;configurePursuitLaw(hiddenMirror,8.0f);runSolo(hiddenMirror,48);
    auto& mirrorState=hiddenMirror.networkMutableState();mirrorState.debug.colliderCount=1;mirrorState.roomColliders[0]=wall;
    runSolo(hiddenMirror,48);mirrorState.player.pos={-2.5f,0.08f,8.0f};mirrorState.player.vel={-4.0f,0.0f,0.0f};runSolo(hiddenMirror,64);
    const auto& mirrorPerception=EnemyMotorRuntimeIntegrationAccess::perception(hiddenMirror,0);
    assert(mirrorPerception.lastSeenPosition.x==lastSeen.x&&mirrorPerception.lastSeenPosition.z==lastSeen.z);
    const auto& hiddenMotor=EnemyMotorRuntimeIntegrationAccess::memory(occluded,0);
    const auto& mirrorMotor=EnemyMotorRuntimeIntegrationAccess::memory(hiddenMirror,0);
    for(std::size_t component=0;component<hiddenMotor.hidden.size();++component)assert(hiddenMotor.hidden[component]==mirrorMotor.hidden[component]);
    for(std::size_t component=0;component<hiddenMotor.tendency.size();++component)assert(hiddenMotor.tendency[component]==mirrorMotor.tendency[component]);
    assert(occluded.state().targets[0].pos.x==hiddenMirror.state().targets[0].pos.x);
    assert(occluded.state().targets[0].pos.z==hiddenMirror.state().targets[0].pos.z);

    Game treeBlocked;configurePursuitLaw(treeBlocked,8.0f);auto& treeState=treeBlocked.networkMutableState();
    treeState.debug.colliderCount=1;auto& trunk=treeState.roomColliders[0];trunk={};trunk.kind=RoomColliderKind::TreeTrunk;
    trunk.minX=-0.45f;trunk.maxX=0.45f;trunk.minZ=3.4f;trunk.maxZ=4.6f;trunk.bottomY=0.0f;trunk.topY=3.2f;
    runSolo(treeBlocked,64);
    assert(!EnemyMotorRuntimeIntegrationAccess::perception(treeBlocked,0).confirmed);
    assert(EnemyMotorRuntimeIntegrationAccess::perception(treeBlocked,0).confidence==0.0f);

    EnemyMotorRuntimeIntegrationAccess::respawn(first,0);
    assert(memoryIsZero(EnemyMotorRuntimeIntegrationAccess::memory(first,0)));
    assert(!EnemyMotorRuntimeIntegrationAccess::body(first,0).initialized);
    assert(EnemyMotorRuntimeIntegrationAccess::perception(first,0).confidence==0.0f);
    first.reset();
    assert(memoryIsZero(EnemyMotorRuntimeIntegrationAccess::memory(first,1)));
    assert(!EnemyMotorRuntimeIntegrationAccess::body(first,1).initialized);

    std::printf("ENEMY_MOTOR_RUNTIME_OK deterministic divergence=%.3f solo0=(%.3f,%.3f) solo1=(%.3f,%.3f) multiplayerStep=(%.4f,%.4f) respawn-reset\n",
        mirroredDifference,firstPositions[0].x,firstPositions[0].z,firstPositions[1].x,firstPositions[1].z,multiplayerTravel.x,multiplayerTravel.z);
    return 0;
}
