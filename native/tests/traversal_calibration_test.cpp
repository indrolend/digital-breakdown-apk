#include "Game.hpp"
#include "EarlyBrowserVisuals.hpp"
#include "gameplay/TraversalGraph.hpp"
#include "gameplay/TargetRoles.hpp"
#include "gameplay/PhoneBody.hpp"

#include <array>
#include <cstdio>

namespace {

constexpr float Dt = 1.0f / 60.0f;
constexpr float PlatformTop = 1.0f;

float horizontalDot(const Vec3& a,const Vec3& b){return a.x*b.x+a.z*b.z;}

void setBox(RoomCollider& box,float x,float z,float width,float depth,float height){
    box={};box.minX=x-width*0.5f;box.maxX=x+width*0.5f;
    box.minZ=z-depth*0.5f;box.maxZ=z+depth*0.5f;box.bottomY=0.0f;box.topY=height;
    box.width=width;box.depth=depth;box.height=height;box.center={x,height*0.5f,z};
}

bool activeEnemyIntersectsCollider(const GameState& state){
    for(const auto& target:state.targets)if(gameplay::isActiveHuman(target))for(int index=0;index<state.debug.colliderCount;++index){const auto& collider=state.roomColliders[index];if(target.pos.x>collider.minX-0.20f&&target.pos.x<collider.maxX+0.20f&&target.pos.z>collider.minZ-0.20f&&target.pos.z<collider.maxZ+0.20f)return true;}
    return false;
}

bool runJumpTrial(float gap,int timingOffset,float steeringError,float targetHeight=PlatformTop,float targetWidth=5.0f){
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
    setBox(state.roomColliders[1],0.0f,targetCenterZ,targetWidth,depth,targetHeight);
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
        if(leftSource&&after.player.grounded)return inside&&after.player.pos.y>targetHeight-0.02f;
    }
    return false;
}

int successes(float gap,float targetHeight=PlatformTop,float targetWidth=5.0f){
    constexpr std::array<int,7> timingOffsets{{-3,-2,-1,0,1,2,3}};
    constexpr std::array<float,3> steering{{-0.035f,0.0f,0.035f}};
    int result=0;
    for(int offset:timingOffsets)for(float error:steering)if(runJumpTrial(gap,offset,error,targetHeight,targetWidth))++result;
    return result;
}

bool runTreeClimbContract(){
    Game game;game.reset();
    GameState& state=game.networkMutableState();
    for(auto& target:state.targets)target={};
    for(auto& collider:state.roomColliders)collider={};
    state.debug.colliderCount=1;
    RoomCollider& tree=state.roomColliders[0];
    setBox(tree,0.0f,0.0f,0.80f,0.80f,3.20f);
    tree.kind=RoomColliderKind::TreeTrunk;
    tree.climbTopY=5.60f;
    state.player.pos={0.0f,0.08f,0.80f};
    state.player.vel={};state.player.jumpVel=0.0f;state.player.grounded=true;state.player.battery=100.0f;
    state.camera.yaw=0.0f;state.camera.pitch=0.0f;

    game.setTouchControls(0.0f,1.0f,0.0f,0.0f,false,false,false,false,false,false);
    game.update(Dt);
    if(!game.state().player.treeClimbing||game.state().player.treeCollider!=0){std::fprintf(stderr,"TREE_CLIMB_STAGE attach climbing=%d collider=%d pos=(%.3f,%.3f,%.3f)\n",game.state().player.treeClimbing?1:0,game.state().player.treeCollider,game.state().player.pos.x,game.state().player.pos.y,game.state().player.pos.z);return false;}
    if(std::abs(game.state().player.pos.x-tree.center.x)>0.001f){std::fprintf(stderr,"TREE_CLIMB_STAGE center attach x=%.3f expected=%.3f\n",game.state().player.pos.x,tree.center.x);return false;}

    for(int frame=0;frame<150;++frame){
        game.setTouchControls(0.0f,1.0f,0.0f,0.0f,false,false,false,false,false,false);
        game.update(Dt);
    }
    const float crownY=game.state().player.pos.y;
    if(!game.state().player.treeClimbing||std::abs(crownY-tree.climbTopY)>0.001f){std::fprintf(stderr,"TREE_CLIMB_STAGE crown climbing=%d y=%.3f top=%.3f\n",game.state().player.treeClimbing?1:0,crownY,tree.climbTopY);return false;}

    const Vec3 initialTipNormal=game.state().player.treeNormal;
    const float expectedRadius=std::sqrt((game.state().player.pos.x-tree.center.x)*(game.state().player.pos.x-tree.center.x)+(game.state().player.pos.z-tree.center.z)*(game.state().player.pos.z-tree.center.z));
    for(int frame=0;frame<30;++frame){
        game.setTouchControls(1.0f,0.0f,0.0f,0.0f,false,false,false,false,false,false);
        game.update(Dt);
    }
    const PlayerState& rotated=game.state().player;
    const float tipLocalZ=rotated.pos.z;
    const float tipRadius=std::sqrt((rotated.pos.x-tree.center.x)*(rotated.pos.x-tree.center.x)+(tipLocalZ-tree.center.z)*(tipLocalZ-tree.center.z));
    if(!rotated.treeClimbing||std::abs(rotated.pos.y-crownY)>0.001f||horizontalDot(rotated.treeNormal,initialTipNormal)>0.75f||std::abs(tipRadius-expectedRadius)>0.002f){std::fprintf(stderr,"TREE_CLIMB_STAGE tip orbit climbing=%d y=%.3f dot=%.3f radius=%.3f expected=%.3f\n",rotated.treeClimbing?1:0,rotated.pos.y,horizontalDot(rotated.treeNormal,initialTipNormal),tipRadius,expectedRadius);return false;}
    const Vec3 launchNormal=rotated.treeNormal;
    game.setTouchControls(0.0f,0.0f,0.0f,0.0f,false,false,true,false,false,false);
    game.update(Dt);
    const PlayerState& launched=game.state().player;
    if(launched.treeClimbing||launched.treeCollider!=-1||launched.treeClimbCooldown<=0.0f||launched.jumpVel<=4.0f||horizontalDot(normalized(launched.vel),launchNormal)<0.99f){std::fprintf(stderr,"TREE_CLIMB_STAGE tip jump climbing=%d collider=%d cooldown=%.3f jump=%.3f alignment=%.3f\n",launched.treeClimbing?1:0,launched.treeCollider,launched.treeClimbCooldown,launched.jumpVel,horizontalDot(normalized(launched.vel),launchNormal));return false;}

    Game descent;descent.reset();GameState& descentState=descent.networkMutableState();
    for(auto& target:descentState.targets)target={};
    for(auto& collider:descentState.roomColliders)collider={};
    descentState.debug.colliderCount=1;setBox(descentState.roomColliders[0],0.0f,0.0f,0.80f,0.80f,3.20f);
    descentState.roomColliders[0].kind=RoomColliderKind::TreeTrunk;descentState.roomColliders[0].climbTopY=5.60f;
    descentState.player.pos={0.0f,crownY,-0.745f};descentState.player.vel={};descentState.player.grounded=false;
    descentState.player.treeClimbing=true;descentState.player.treeCollider=0;descentState.player.treeNormal={0.0f,0.0f,-1.0f};descentState.camera.yaw=0.0f;
    for(int frame=0;frame<15;++frame){
        descent.setTouchControls(0.0f,-1.0f,0.0f,0.0f,false,false,false,false,false,false);
        descent.update(Dt);
    }
    if(!descent.state().player.treeClimbing||descent.state().player.pos.y>=crownY-0.20f){std::fprintf(stderr,"TREE_CLIMB_STAGE descend climbing=%d y=%.3f crown=%.3f\n",descent.state().player.treeClimbing?1:0,descent.state().player.pos.y,crownY);return false;}

    Game offCenter;offCenter.reset();GameState& offCenterState=offCenter.networkMutableState();
    for(auto& target:offCenterState.targets)target={};
    for(auto& collider:offCenterState.roomColliders)collider={};
    offCenterState.debug.colliderCount=1;setBox(offCenterState.roomColliders[0],0.0f,0.0f,0.80f,0.80f,3.20f);
    offCenterState.roomColliders[0].kind=RoomColliderKind::TreeTrunk;offCenterState.roomColliders[0].climbTopY=5.60f;
    offCenterState.player.pos={0.34f,0.08f,0.80f};offCenterState.player.vel={};offCenterState.player.grounded=true;offCenterState.camera.yaw=0.0f;
    offCenter.setTouchControls(0.0f,1.0f,0.0f,0.0f,false,false,false,false,false,false);offCenter.update(Dt);
    if(offCenter.state().player.treeClimbing){std::fprintf(stderr,"TREE_CLIMB_STAGE off-center grip incorrectly attached x=%.3f\n",offCenter.state().player.pos.x);return false;}

    Game wall;wall.reset();GameState& wallState=wall.networkMutableState();
    for(auto& target:wallState.targets)target={};
    for(auto& collider:wallState.roomColliders)collider={};
    wallState.debug.colliderCount=1;setBox(wallState.roomColliders[0],0.0f,0.0f,0.80f,0.80f,3.20f);
    wallState.player.pos={0.0f,0.08f,0.80f};wallState.player.vel={};wallState.player.grounded=true;wallState.camera.yaw=0.0f;
    for(int frame=0;frame<20;++frame){wall.setTouchControls(0.0f,1.0f,0.0f,0.0f,false,false,false,false,false,false);wall.update(Dt);}
    if(wall.state().player.treeClimbing){std::fprintf(stderr,"TREE_CLIMB_STAGE isolation generic wall attached\n");return false;}
    return true;
}

bool runRaisedSupportContract(){
    Game game;game.reset();
    GameState& state=game.networkMutableState();
    for(auto& target:state.targets)target={};
    for(auto& collider:state.roomColliders)collider={};
    state.debug.colliderCount=1;
    RoomCollider& platform=state.roomColliders[0];
    setBox(platform,0.0f,0.0f,2.0f,4.0f,PlatformTop);

    // A rising phone whose center has passed the top has not physically
    // cleared the lip until its lower edge has also passed the top.
    state.player.pos={platform.minX-gameplay::PHONE_BODY.supportRadius-0.01f,platform.topY+0.02f,0.0f};
    state.player.vel={2.0f,0.0f,0.0f};state.player.jumpVel=1.0f;state.player.grounded=false;
    state.player.airJumpsRemaining=1;state.player.battery=100.0f;
    game.setTouchControls(0.0f,0.0f,0.0f,0.0f,false,false,false,false,false,false);
    game.update(Dt);
    const float blockedX=platform.minX-gameplay::PHONE_BODY.collisionRadius;
    if(game.state().player.pos.x>blockedX+0.001f){
        std::fprintf(stderr,"RAISED_SUPPORT_STAGE rising lip penetrated x=%.3f blocked=%.3f y=%.3f top=%.3f\n",game.state().player.pos.x,blockedX,game.state().player.pos.y,platform.topY);
        return false;
    }

    // Falling inside the visible footprint must settle on the exact physical
    // top and become grounded through the normal controller update.
    state.player.pos={0.0f,platform.topY+0.34f,0.0f};state.player.vel={};state.player.jumpVel=-3.0f;state.player.grounded=false;
    for(int frame=0;frame<30&&!game.state().player.grounded;++frame){
        game.setTouchControls(0.0f,0.0f,0.0f,0.0f,false,false,false,false,false,false);
        game.update(Dt);
    }
    const float expectedSupportY=platform.topY+PHONE_BODY_HEIGHT*0.5f;
    if(!game.state().player.grounded||std::abs(game.state().player.pos.y-expectedSupportY)>0.001f){
        std::fprintf(stderr,"RAISED_SUPPORT_STAGE landing grounded=%d y=%.3f expected=%.3f\n",game.state().player.grounded?1:0,game.state().player.pos.y,expectedSupportY);
        return false;
    }

    // Ordinary input can carry the grounded phone off the support; once the
    // visible support footprint clears the edge, it must become airborne.
    state.player.pos.x=platform.maxX-0.02f;state.player.vel={};
    state.camera.yaw=-1.57079632679f;
    bool leftSupport=false;
    for(int frame=0;frame<45;++frame){
        game.setTouchControls(0.0f,1.0f,0.0f,0.0f,false,false,false,false,false,false);
        game.update(Dt);
        if(game.state().player.pos.x>platform.maxX+gameplay::PHONE_BODY.supportRadius){leftSupport=true;break;}
    }
    if(!leftSupport||game.state().player.grounded){
        std::fprintf(stderr,"RAISED_SUPPORT_STAGE edge left=%d grounded=%d x=%.3f edge=%.3f\n",leftSupport?1:0,game.state().player.grounded?1:0,game.state().player.pos.x,platform.maxX);
        return false;
    }
    return true;
}

} // namespace

int main(){
    if(!runRaisedSupportContract()){
        std::fprintf(stderr,"RAISED_SUPPORT_FAIL lip blocking, landing, or edge departure regressed\n");
        return 1;
    }
    std::printf("RAISED_SUPPORT_OK lip=blocked landing=grounded edge_departure=airborne\n");
    if(!runTreeClimbContract()){
        std::fprintf(stderr,"TREE_CLIMB_FAIL trunk grip, crown clamp, descent, jump-off, or generic-wall isolation regressed\n");
        return 1;
    }
    std::printf("TREE_CLIMB_OK crown=reachable tip_orbit=controlled tip_jump=directional descent=controlled generic_walls=unchanged\n");
    gameplay::TraversalGraph graph;
    graph.surfaceCount=2;graph.edgeCount=1;
    graph.surfaces[0]={{0,1,0},{2.5f,0,3},true};
    graph.surfaces[1]={{0,1,-7},{2.5f,0,3},true};
    graph.edges[0]={0,1,gameplay::TraversalAction::Jump,gameplay::TraversalRole::Required};
    if(!gameplay::validTraversalGraphTopology(graph))return 1;
    const auto measured=gameplay::measureTraversalEdge(graph,graph.edges[0],gameplay::TRAVERSAL_CAPABILITIES.comfortableClearanceRadius);
    if(std::abs(measured.gap-1.0f)>0.001f||std::abs(measured.landingWidth-5.0f)>0.001f||measured.movement!=gameplay::TraversalAction::Jump||!gameplay::isRequired(graph.edges[0])){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL edge semantics or measurements are inconsistent\n");return 1;
    }
    gameplay::TraversalEdge optional=graph.edges[0];optional.role=gameplay::TraversalRole::Optional;
    if(gameplay::isRequired(optional)){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL optional edge collapsed into required traversal\n");return 1;
    }

    const int easy=successes(1.50f),medium=successes(2.00f),hard=successes(2.50f);
    const int repeatEasy=successes(1.50f),repeatMedium=successes(2.00f),repeatHard=successes(2.50f);
    std::printf("TRAVERSAL_CALIBRATION jump gap=1.50 success=%d/21 gap=2.00 success=%d/21 gap=2.50 success=%d/21\n",easy,medium,hard);
    const int ascentLow=successes(1.50f,1.30f),ascentHigh=successes(1.50f,1.50f);
    const int narrowLanding=successes(2.00f,PlatformTop,1.50f);
    const int repeatAscentLow=successes(1.50f,1.30f),repeatAscentHigh=successes(1.50f,1.50f);
    const int repeatNarrowLanding=successes(2.00f,PlatformTop,1.50f);
    std::printf("TRAVERSAL_CALIBRATION ascent gap=1.50 rise=0.30 success=%d/21 rise=0.50 success=%d/21 narrow_landing gap=2.00 width=1.50 success=%d/21\n",ascentLow,ascentHigh,narrowLanding);
    if(easy!=repeatEasy||medium!=repeatMedium||hard!=repeatHard){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL repeated controller sweep was not deterministic\n");
        return 1;
    }
    if(easy<18||easy<medium||medium<hard||hard>=21){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL invalid reliability ordering or insufficient easy margin\n");
        return 1;
    }
    if(ascentLow!=repeatAscentLow||ascentHigh!=repeatAscentHigh||narrowLanding!=repeatNarrowLanding){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL expanded controller sweep was not deterministic\n");
        return 1;
    }
    if(ascentLow<15||ascentHigh<15||narrowLanding<18){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL baseline ascent or landing reliability regressed\n");
        return 1;
    }
    Game lab;lab.debugStartTraversalLab();const GameState& labState=lab.state();
    if(!labState.traversalLab||labState.debug.colliderCount!=12||!labState.roomClear||labState.requiredSouls!=0){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL playable lab did not establish its bounded fixture\n");
        return 1;
    }
    for(const TargetState& target:labState.targets)if(target.alive){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL playable lab contains combat interference\n");
        return 1;
    }
    Game normal;normal.reset();
    if(normal.state().roomInspector||normal.state().traversalLab||normal.state().roomIndex!=1||normal.state().roomSeed!=12345){std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL normal solo startup inherited developer state\n");return 1;}
    Game inspector;inspector.debugStartRoomInspector();bool sawPhysicalPlayground=false,sawPhysicalFunnel=false;int physicalPlaygroundSeed=0,physicalFunnelSeed=0;
    for(int premiseIndex=0;premiseIndex<static_cast<int>(early_browser_visuals::RoomPremise::Count);++premiseIndex){
        const GameState& state=inspector.state();const auto plan=early_browser_visuals::roomPlan(state.roomSeed,state.roomIndex);
        const auto premise=static_cast<early_browser_visuals::RoomPremise>(premiseIndex);const auto& report=state.roomInspectorReport;
        int expectedRequiredEdges=0;for(int i=0;i<plan.traversal.edgeCount;++i)if(gameplay::isRequired(plan.traversal.edges[i]))++expectedRequiredEdges;
        const auto geometry=early_browser_visuals::roomGeometryCapacityPlan(plan,state.roomSeed,state.roomIndex,ROOM_COLLIDER_COUNT);
        const int expectedProps=early_browser_visuals::selectedEnvironmentPropCount(plan,geometry);
        const int physicalSurfaces=geometry.optionalTraversalColliderCount;
        const int expectedColliders=geometry.totalColliderCount;
        if(!state.roomInspector||state.roomInspectorPremise!=premise||!early_browser_visuals::matchesInspectorPremise(plan,premise)||!state.roomClear||state.requiredSouls!=0||!report.seedSelectionValid||
           report.seed!=state.roomSeed||report.roomIndex!=state.roomIndex||report.setting!=plan.setting||report.form!=plan.form||report.scale!=plan.scale||report.condition!=plan.condition||report.traversalIntent!=plan.traversalIntent||
           !report.requiredRouteValid||report.traversalSurfaceCount!=plan.traversal.surfaceCount||report.traversalEdgeCount!=plan.traversal.edgeCount||report.requiredEdgeCount!=expectedRequiredEdges||
           report.colliderCount!=state.debug.colliderCount||report.colliderCount!=expectedColliders||report.presentationPropCount!=expectedProps||report.enemyCount!=0){
            std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL room inspector premise %s report does not match production state\n",early_browser_visuals::premiseName(premise));return 1;
        }
        if(physicalSurfaces){
            if(plan.traversalIntent==early_browser_visuals::RoomTraversalIntent::Playground){sawPhysicalPlayground=true;physicalPlaygroundSeed=state.roomSeed;}
            if(plan.traversalIntent==early_browser_visuals::RoomTraversalIntent::Funnel){sawPhysicalFunnel=true;physicalFunnelSeed=state.roomSeed;}
            const auto& surface=plan.traversal.surfaces[plan.traversal.surfaceCount-1];const auto expected=early_browser_visuals::physicalTraversalObstacle(surface);const RoomCollider& collider=state.roomColliders[plan.obstacleCount];
            if((plan.traversalIntent!=early_browser_visuals::RoomTraversalIntent::Playground&&plan.traversalIntent!=early_browser_visuals::RoomTraversalIntent::Funnel)||surface.required||collider.bottomY!=0.0f||collider.topY!=expected.size.y||collider.center.x!=expected.center.x||collider.center.y!=expected.center.y||collider.center.z!=expected.center.z||collider.width!=expected.size.x||collider.height!=expected.size.y||collider.depth!=expected.size.z){std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL optional traversal surface was not materialized as one ground-supported shape\n");return 1;}
        }
        const std::string review=inspector.debugRoomReviewLine(RoomReviewRating::Tune);
        if(review!=inspector.debugRoomReviewLine(RoomReviewRating::Tune)||review.find(std::string("premise=")+early_browser_visuals::premiseName(premise))==std::string::npos||review.find("rating=TUNE")==std::string::npos||review.find("route=VALID")==std::string::npos){std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL deterministic review record incomplete\n");return 1;}
        inspector.debugToggleRoomInspectorEnemies();
        if(activeEnemyIntersectsCollider(inspector.state())){std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL room inspector premise %s placed an enemy inside production geometry\n",early_browser_visuals::premiseName(premise));return 1;}
        inspector.debugToggleRoomInspectorEnemies();
        if(premiseIndex+1<static_cast<int>(early_browser_visuals::RoomPremise::Count))inspector.debugStepRoomInspector(1);
    }
    if(!sawPhysicalPlayground){std::fprintf(stderr,"PLAYGROUND_TRAVERSAL_FAILED seed=unknown stage=inspector reason=not_exercised\n");return 1;}
    std::printf("PLAYGROUND_TRAVERSAL_OK seed=%d colliders=verified surfaces=1\n",physicalPlaygroundSeed);
    if(!sawPhysicalFunnel){std::fprintf(stderr,"FUNNEL_TRAVERSAL_FAILED seed=unknown stage=inspector reason=not_exercised\n");return 1;}
    std::printf("FUNNEL_TRAVERSAL_OK seed=%d colliders=verified surfaces=1\n",physicalFunnelSeed);
    const int previousSeed=inspector.state().roomSeed;inspector.debugStepRoomInspector(0,true);
    const int regeneratedSeed=inspector.state().roomSeed;const auto regeneratedPremise=inspector.state().roomInspectorPremise;
    if(regeneratedSeed==previousSeed||regeneratedPremise!=early_browser_visuals::RoomPremise::CoastalShore||!early_browser_visuals::matchesInspectorPremise(early_browser_visuals::roomPlan(regeneratedSeed,inspector.state().roomIndex),regeneratedPremise)){
        std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL room inspector seed regeneration failed\n");return 1;
    }
    Game reproduced;reproduced.debugStartRoomInspector();for(int i=0;i<7;++i)reproduced.debugStepRoomInspector(1);while(reproduced.state().roomSeed!=regeneratedSeed&&reproduced.state().roomSeed<regeneratedSeed)reproduced.debugStepRoomInspector(0,true);
    if(reproduced.state().roomSeed!=regeneratedSeed||reproduced.state().roomInspectorReport.colliderCount!=inspector.state().roomInspectorReport.colliderCount){std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL same inspector seed was not reproducible\n");return 1;}
    inspector.debugToggleRoomInspectorEnemies();bool sawEnemy=false;for(const auto& target:inspector.state().targets)sawEnemy|=target.alive;
    if(!inspector.state().roomInspectorEnemies||!sawEnemy||inspector.state().roomInspectorReport.enemyCount<=0||inspector.state().roomInspectorReport.enemyCount>inspector.state().roomInspectorReport.enemyBudget){std::fprintf(stderr,"TRAVERSAL_CALIBRATION_FAIL room inspector enemy toggle/count failed\n");return 1;}
    return 0;
}
