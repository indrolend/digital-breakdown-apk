#include "Game.hpp"
#include "gameplay/TraversalGraph.hpp"

#include <array>
#include <cstdio>

namespace {

constexpr float Dt = 1.0f / 60.0f;
constexpr float PlatformTop = 1.0f;

void setBox(RoomCollider& box,float x,float z,float width,float depth,float height){
    box={};box.minX=x-width*0.5f;box.maxX=x+width*0.5f;
    box.minZ=z-depth*0.5f;box.maxZ=z+depth*0.5f;box.bottomY=0.0f;box.topY=height;
    box.width=width;box.depth=depth;box.height=height;box.center={x,height*0.5f,z};
}

bool runJumpTrial(float gap,int timingOffset,float steeringError){
    Game game;game.reset();
    GameState& state=game.networkMutableState();
    state.debug.colliderCount=2;
    for(auto& collider:state.roomColliders)collider={};
    constexpr float depth=6.0f;
    const float sourceCenterZ=4.0f;
    const float sourceFront=sourceCenterZ-depth*0.5f;
    const float targetBack=sourceFront-gap;
    const float targetCenterZ=targetBack-depth*0.5f;
    setBox(state.roomColliders[0],0.0f,sourceCenterZ,5.0f,depth,PlatformTop);
    setBox(state.roomColliders[1],0.0f,targetCenterZ,5.0f,depth,PlatformTop);
    state.player.pos={0.0f,PlatformTop+0.08f,sourceCenterZ+1.15f};
    state.player.vel={};state.player.jumpVel=0.0f;state.player.grounded=true;
    state.player.airJumpsRemaining=1;state.player.battery=100.0f;
    state.camera.yaw=steeringError;state.camera.pitch=0.0f;
    for(auto& target:state.targets)target={};

    bool leftSource=false;
    bool jumpIssued=false;
    for(int frame=0;frame<150;++frame){
        const float triggerDistance=0.85f-static_cast<float>(timingOffset)*0.14f;
        const bool jump=!jumpIssued&&state.player.pos.z<=sourceFront+triggerDistance;
        jumpIssued|=jump;
        game.setTouchControls(0.0f,1.0f,0.0f,0.0f,false,false,jump,false,false,false);
        game.update(Dt);
        const GameState& after=game.state();
        leftSource|=after.player.pos.z<sourceFront-0.05f;
        const RoomCollider& landing=after.roomColliders[1];
        const bool inside=after.player.pos.x>landing.minX&&after.player.pos.x<landing.maxX&&
                          after.player.pos.z>landing.minZ&&after.player.pos.z<landing.maxZ;
        if(leftSource&&after.player.grounded)return inside&&after.player.pos.y>PlatformTop-0.02f;
    }
    return false;
}

int successes(float gap){
    constexpr std::array<int,7> timingOffsets{{-3,-2,-1,0,1,2,3}};
    constexpr std::array<float,3> steering{{-0.035f,0.0f,0.035f}};
    int result=0;
    for(int offset:timingOffsets)for(float error:steering)if(runJumpTrial(gap,offset,error))++result;
    return result;
}

} // namespace

int main(){
    gameplay::TraversalGraph graph;
    graph.surfaceCount=2;graph.edgeCount=1;
    graph.surfaces[0]={{0,1,0},{2.5f,0,3},true};
    graph.surfaces[1]={{0,1,-7},{2.5f,0,3},true};
    graph.edges[0]={0,1,gameplay::TraversalAction::Jump,gameplay::TraversalDifficulty::Comfortable,true};
    if(!gameplay::validTraversalGraphTopology(graph))return 1;

    const int easy=successes(1.50f),medium=successes(2.00f),hard=successes(2.50f);
    const int repeatEasy=successes(1.50f),repeatMedium=successes(2.00f),repeatHard=successes(2.50f);
    std::printf("TRAVERSAL_CALIBRATION jump gap=1.50 success=%d/21 gap=2.00 success=%d/21 gap=2.50 success=%d/21\n",easy,medium,hard);
    if(easy!=repeatEasy||medium!=repeatMedium||hard!=repeatHard){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL repeated controller sweep was not deterministic\n");
        return 1;
    }
    if(easy<18||easy<medium||medium<hard||hard>=21){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL invalid reliability ordering or insufficient easy margin\n");
        return 1;
    }
    return 0;
}
