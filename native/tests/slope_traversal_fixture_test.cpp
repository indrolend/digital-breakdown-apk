#include "Game.hpp"
#include "gameplay/PhoneBody.hpp"

#include <cmath>
#include <cstdio>

namespace {
constexpr float Dt=1.0f/60.0f;
void input(Game& game,float x,float z,bool vacuum=false,bool jump=false,bool melee=false,bool shoot=false){game.setTouchControls(x,z,0,0,vacuum,false,jump,melee,shoot,false);game.update(Dt);}
void placeOnSlope(Game& game,float x,float z){auto& state=game.networkMutableState();const auto sample=sampleSlopeSupport(state.slopeSupports[0],x,z);state.player.pos={x,sample.height+0.08f,z};state.player.vel={};state.player.jumpVel=0;state.player.grounded=true;state.player.battery=100;state.camera.yaw=0;}
bool near(float a,float b,float tolerance=0.025f){return std::fabs(a-b)<=tolerance;}
float horizontalSpeed(const Vec3& velocity){return std::sqrt(velocity.x*velocity.x+velocity.z*velocity.z);}
float denseRockHeight(const faceted_rock::Support& rock,float x,float z,float radius){float height=-1.0f;for(int ring=0;ring<=3;++ring){const float r=radius*static_cast<float>(ring)/3.0f;const int steps=ring==0?1:96;for(int step=0;step<steps;++step){const float angle=6.283185307f*static_cast<float>(step)/static_cast<float>(steps);const auto sample=faceted_rock::sampleEnvelope(rock,x+std::cos(angle)*r,z+std::sin(angle)*r);if(sample.inside)height=std::max(height,sample.height);}}return height;}
struct PhoneWorldBounds{float bottom=0.0f;float horizontalRadius=0.0f;};
PhoneWorldBounds phoneWorldBounds(const GameState& state){
    const Vec3 axisX=rotate(state.phoneTransform.orientation,{1,0,0});
    const Vec3 axisY=rotate(state.phoneTransform.orientation,{0,1,0});
    const Vec3 axisZ=rotate(state.phoneTransform.orientation,{0,0,1});
    const float halfX=PHONE_BODY_WIDTH*state.phoneVisual.bodyScale.x*0.5f;
    const float halfY=PHONE_BODY_HEIGHT*state.phoneVisual.bodyScale.y*0.5f;
    const float halfZ=PHONE_BODY_DEPTH*state.phoneVisual.bodyScale.z*0.5f;
    const float extentX=std::abs(axisX.x)*halfX+std::abs(axisY.x)*halfY+std::abs(axisZ.x)*halfZ;
    const float extentY=std::abs(axisX.y)*halfX+std::abs(axisY.y)*halfY+std::abs(axisZ.y)*halfZ;
    const float extentZ=std::abs(axisX.z)*halfX+std::abs(axisY.z)*halfY+std::abs(axisZ.z)*halfZ;
    return {state.phoneTransform.position.y-extentY,std::sqrt(extentX*extentX+extentZ*extentZ)};
}
}

int main(){
    Game ascent;ascent.debugStartSlopeLab();const auto fixture=ascent.state().slopeSupports[0];
    if(!ascent.state().slopeLab||ascent.state().slopeSupportCount!=1||ascent.state().debug.colliderCount!=1||classifySupport(fixture)!=SupportClassification::TraversableSlope){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL authority\n");return 1;}
    bool crossedSlope=false,reachedPlateau=false;float previousY=ascent.state().player.pos.y;Vec3 previousCamera=ascent.state().camera.pos;
    for(int frame=0;frame<600;++frame){input(ascent,0,1);const auto& p=ascent.state().player;const Vec3 cameraDelta=ascent.state().camera.pos-previousCamera;if(!std::isfinite(ascent.state().camera.pos.x)||!std::isfinite(ascent.state().camera.pos.y)||!std::isfinite(ascent.state().camera.pos.z)||(frame>0&&length(cameraDelta)>1.0f)||horizontalSpeed(p.vel)>7.0f){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL bounded camera/speed frame=%d\n",frame);return 1;}previousCamera=ascent.state().camera.pos;if(p.pos.z<fixture.maxZ&&p.pos.z>fixture.minZ){const auto support=sampleSlopeSupport(fixture,p.pos.x,p.pos.z);crossedSlope=true;if(!p.grounded||!near(p.pos.y,support.height+0.08f)||p.pos.y+0.001f<previousY){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL ascent frame=%d pos=(%.3f,%.3f,%.3f)\n",frame,p.pos.x,p.pos.y,p.pos.z);return 1;}}previousY=p.pos.y;if(p.pos.z<fixture.minZ-0.3f){reachedPlateau=p.grounded&&near(p.pos.y,1.68f);break;}}
    if(!crossedSlope||!reachedPlateau){const auto& p=ascent.state().player;std::fprintf(stderr,"SLOPE_FIXTURE_FAIL seam ascent crossed=%d plateau=%d pos=(%.3f,%.3f,%.3f) grounded=%d\n",crossedSlope?1:0,reachedPlateau?1:0,p.pos.x,p.pos.y,p.pos.z,p.grounded?1:0);return 1;}

    Game stop;stop.debugStartSlopeLab();placeOnSlope(stop,0,7);for(int i=0;i<45;++i)input(stop,0,1);for(int i=0;i<120;++i)input(stop,0,0);const auto stopped=stop.state().player;const auto stoppedSupport=sampleSlopeSupport(fixture,stopped.pos.x,stopped.pos.z);if(!stopped.grounded||horizontalSpeed(stopped.vel)>0.02f||!near(stopped.pos.y,stoppedSupport.height+0.08f)){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL stopping speed=%.3f\n",horizontalSpeed(stopped.vel));return 1;}

    Game reverse;reverse.debugStartSlopeLab();placeOnSlope(reverse,0,7);for(int i=0;i<24;++i)input(reverse,0,1);const float uphillZ=reverse.state().player.pos.z;for(int i=0;i<24;++i)input(reverse,0,-1);if(reverse.state().player.vel.z<=0||reverse.state().player.pos.z<=uphillZ){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL reversal velZ=%.3f\n",reverse.state().player.vel.z);return 1;}

    Game downhill;downhill.debugStartSlopeLab();placeOnSlope(downhill,0,3);for(int i=0;i<45;++i)input(downhill,0,-1);if(!downhill.state().player.grounded||downhill.state().player.pos.z<=3.5f){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL downhill\n");return 1;}
    Game lateral;lateral.debugStartSlopeLab();placeOnSlope(lateral,0,7);for(int i=0;i<30;++i)input(lateral,1,0);if(!lateral.state().player.grounded||lateral.state().player.pos.x<0.5f||std::fabs(lateral.state().player.pos.z-7)>0.05f){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL lateral pos=(%.3f,%.3f)\n",lateral.state().player.pos.x,lateral.state().player.pos.z);return 1;}

    Game jump;jump.debugStartSlopeLab();placeOnSlope(jump,0,7);input(jump,0,0,false,true);if(jump.state().player.grounded||jump.state().player.jumpVel<=4.0f){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL jump\n");return 1;}for(int i=0;i<8;++i)input(jump,0,0);const float firstJumpVelocity=jump.state().player.jumpVel;input(jump,0,0,false,true);if(jump.state().player.grounded||jump.state().player.jumpVel<=firstJumpVelocity||jump.state().player.airJumpsRemaining!=0){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL double jump\n");return 1;}for(int i=0;i<240&&!jump.state().player.grounded;++i)input(jump,0,0);const auto landedSupport=sampleSlopeSupport(fixture,jump.state().player.pos.x,jump.state().player.pos.z);if(!jump.state().player.grounded||!near(jump.state().player.pos.y,landedSupport.height+0.08f)){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL landing\n");return 1;}
    Game melee;melee.debugStartSlopeLab();placeOnSlope(melee,0,7);input(melee,0,0,false,false,true);if(melee.state().meleeVisual.visualTimer<=0.0f){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL melee\n");return 1;}
    Game vacuum;vacuum.debugStartSlopeLab();placeOnSlope(vacuum,0,7);input(vacuum,0,0,true);if(!vacuum.state().vacuum.active){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL vacuum\n");return 1;}
    Game shot;shot.debugStartSlopeLab();placeOnSlope(shot,0,7);if(!shot.debugSpawnStoredSoul()){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL shot setup\n");return 1;}input(shot,0,0,false,false,false,true);bool pending=false;for(const auto& request:shot.state().pendingShots)pending|=request.active;if(!pending){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL shot\n");return 1;}
    Game lunge;lunge.debugStartSlopeLab();placeOnSlope(lunge,0,7);input(lunge,0,0,false,true);input(lunge,0,0,false,false,true);if(!lunge.state().meleeVisual.airLungeLandingPending){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL lunge\n");return 1;}

    Game rock;rock.debugStartSlopeLab();const auto rockAuthority=rock.state().rockSupports[0];const auto& rockProp=rockAuthority.prop;
    if(rock.state().rockSupportCount!=1){std::fprintf(stderr,"ROCK_SUPPORT_FAIL fixture authority\n");return 1;}
    const auto centerSurface=faceted_rock::sampleSupportFootprint(rockAuthority,rockProp.center.x,rockProp.center.z,gameplay::PHONE_BODY.collisionRadius);
    const auto centerGameplay=rock.debugPlayerSupportAt(rockProp.center.x,rockProp.center.z);
    if(!centerSurface.inside||!near(centerGameplay.height,centerSurface.height+0.08f)||centerGameplay.normal.y<faceted_rock::WalkableNormalY){std::fprintf(stderr,"ROCK_SUPPORT_FAIL shared facet support\n");return 1;}
    const float emptyX=rockProp.center.x+rockProp.size.x*0.5f+gameplay::PHONE_BODY.collisionRadius+0.05f,emptyZ=rockProp.center.z+rockProp.size.z*0.5f+gameplay::PHONE_BODY.collisionRadius+0.05f;
    if(faceted_rock::sampleSupport(rockAuthority,emptyX,emptyZ).inside||!near(rock.debugPlayerSupportAt(emptyX,emptyZ).height,0.08f)){std::fprintf(stderr,"ROCK_SUPPORT_FAIL invisible box top\n");return 1;}
    auto& rockPlayer=rock.networkMutableState().player;rockPlayer.pos={rockProp.center.x,centerGameplay.height,rockProp.center.z};rockPlayer.vel={};rockPlayer.jumpVel=0;rockPlayer.grounded=true;rockPlayer.battery=100;rock.networkMutableState().camera.yaw=-1.5707963f;
    input(rock,0,0,false,true);for(int frame=0;frame<150&&!rock.state().player.grounded;++frame)input(rock,0,0);
    if(!rock.state().player.grounded||!near(rock.state().player.pos.y,centerGameplay.height)||!std::isfinite(rock.state().player.pos.y)){std::fprintf(stderr,"ROCK_SUPPORT_FAIL jump landing\n");return 1;}
    bool fellFromFacet=false;for(int frame=0;frame<180;++frame){input(rock,0,1);fellFromFacet|=!rock.state().player.grounded;if(rock.state().player.pos.x>rockProp.center.x+rockProp.size.x)break;}
    if(!fellFromFacet||!std::isfinite(rock.state().player.pos.x)||!std::isfinite(rock.state().player.pos.y)){std::fprintf(stderr,"ROCK_SUPPORT_FAIL traversal/fall\n");return 1;}
    Game rockSide;rockSide.debugStartSlopeLab();auto& sidePlayer=rockSide.networkMutableState().player;sidePlayer.pos={rockProp.center.x-rockProp.size.x,0.08f,rockProp.center.z};sidePlayer.vel={};sidePlayer.grounded=true;rockSide.networkMutableState().camera.yaw=-1.5707963f;for(int frame=0;frame<180;++frame)input(rockSide,0,1);const auto sideSupport=faceted_rock::sampleSupport(rockAuthority,rockSide.state().player.pos.x,rockSide.state().player.pos.z);if(sideSupport.inside||rockSide.state().player.pos.x>=rockProp.center.x-rockProp.size.x*0.25f){std::fprintf(stderr,"ROCK_SUPPORT_FAIL steep side obstruction pos=(%.3f,%.3f,%.3f)\n",rockSide.state().player.pos.x,rockSide.state().player.pos.y,rockSide.state().player.pos.z);return 1;}
    Game rockCombat;rockCombat.debugStartSlopeLab();auto& combatPlayer=rockCombat.networkMutableState().player;combatPlayer.pos={rockProp.center.x,centerGameplay.height,rockProp.center.z};combatPlayer.vel={};combatPlayer.grounded=true;combatPlayer.battery=100;input(rockCombat,0,0,false,false,true);if(rockCombat.state().meleeVisual.visualTimer<=0){std::fprintf(stderr,"ROCK_SUPPORT_FAIL combat authority\n");return 1;}
    rockCombat.debugStartSlopeLab();auto& vacuumPlayer=rockCombat.networkMutableState().player;vacuumPlayer.pos={rockProp.center.x,centerGameplay.height,rockProp.center.z};vacuumPlayer.vel={};vacuumPlayer.grounded=true;vacuumPlayer.battery=100;input(rockCombat,0,0,true);if(!rockCombat.state().vacuum.active){std::fprintf(stderr,"ROCK_SUPPORT_FAIL vacuum authority\n");return 1;}

    for(int variant=0;variant<12;++variant){
        for(int direction=0;direction<8;++direction){
            auto generated=std::make_unique<Game>();generated->debugStartSlopeLab();auto& generatedState=generated->networkMutableState();auto& generatedRock=generatedState.rockSupports[0];generatedRock.roomSeed=73+variant*97;generatedRock.roomIndex=variant;generatedRock.propIndex=variant+3;generatedRock.prop.yaw=0.19f*variant;
            const float angle=6.283185307f*static_cast<float>(direction)/8.0f;const Vec3 travel{std::cos(angle),0,std::sin(angle)};const auto startSupport=generated->debugPlayerSupportAt(generatedRock.prop.center.x,generatedRock.prop.center.z);auto& generatedPlayer=generatedState.player;generatedPlayer.pos={generatedRock.prop.center.x,startSupport.height,generatedRock.prop.center.z};generatedPlayer.vel={};generatedPlayer.jumpVel=0;generatedPlayer.grounded=true;generatedPlayer.battery=100;generatedState.camera.yaw=std::atan2(-travel.x,-travel.z);
            for(int frame=0;frame<150;++frame){const bool jumpPressed=frame==12;const bool meleePressed=frame==32;const bool vacuumHeld=frame>=52&&frame<76;input(*generated,0,1,vacuumHeld,jumpPressed,meleePressed);const auto& current=generated->state();const float required=denseRockHeight(generatedRock,current.player.pos.x,current.player.pos.z,gameplay::PHONE_BODY.collisionRadius);const float bodyBottom=current.player.pos.y-PHONE_BODY_HEIGHT*0.5f;if(required>=0.0f&&bodyBottom<required-0.001f){std::fprintf(stderr,"GENERATED_SURFACE_FAIL body penetration variant=%d direction=%d frame=%d bottom=%.3f surface=%.3f\n",variant,direction,frame,bodyBottom,required);return 1;}const auto visiblePhone=phoneWorldBounds(current);const float visibleSurface=denseRockHeight(generatedRock,current.phoneTransform.position.x,current.phoneTransform.position.z,visiblePhone.horizontalRadius);if(visibleSurface>=0.0f&&visiblePhone.bottom<visibleSurface-0.002f){std::fprintf(stderr,"GENERATED_SURFACE_FAIL visible phone penetration variant=%d direction=%d frame=%d bottom=%.3f surface=%.3f radius=%.3f\n",variant,direction,frame,visiblePhone.bottom,visibleSurface,visiblePhone.horizontalRadius);return 1;}const float cameraSurface=denseRockHeight(generatedRock,current.camera.pos.x,current.camera.pos.z,0.0f);if(cameraSurface>=0.0f&&current.camera.pos.y<cameraSurface+0.02f){std::fprintf(stderr,"GENERATED_SURFACE_FAIL camera penetration variant=%d direction=%d frame=%d cameraY=%.3f surface=%.3f\n",variant,direction,frame,current.camera.pos.y,cameraSurface);return 1;}}
        }
    }
    Game normal;normal.reset();ascent.reset();if(normal.state().slopeLab||normal.state().slopeSupportCount!=0){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL leaked into normal rooms\n");return 1;}
    if(normal.state().rockSupportCount<=0||normal.state().rockSupportCount>ROCK_SUPPORT_COUNT||normal.state().rockSupportCount!=ascent.state().rockSupportCount){std::fprintf(stderr,"ROCK_SUPPORT_FAIL deterministic room deployment count=%d repeat=%d\n",normal.state().rockSupportCount,ascent.state().rockSupportCount);return 1;}
    int rockSlots=0;for(int i=0;i<normal.state().debug.colliderCount;++i)rockSlots+=normal.state().roomColliders[i].kind==RoomColliderKind::RockAuthoritySlot?1:0;
    if(rockSlots!=normal.state().rockSupportCount){std::fprintf(stderr,"ROCK_SUPPORT_FAIL capacity slots=%d supports=%d\n",rockSlots,normal.state().rockSupportCount);return 1;}
    for(int i=0;i<normal.state().rockSupportCount;++i){const auto& deployed=normal.state().rockSupports[i];const auto plan=early_browser_visuals::roomPlan(normal.state().roomSeed,normal.state().roomIndex);if(!faceted_rock::eligible(plan.setting,deployed.prop.role)||deployed.roomSeed!=normal.state().roomSeed||deployed.roomIndex!=normal.state().roomIndex){std::fprintf(stderr,"ROCK_SUPPORT_FAIL room authority\n");return 1;}const auto visual=faceted_rock::sampleSupportFootprint(deployed,deployed.prop.center.x,deployed.prop.center.z,gameplay::PHONE_BODY.collisionRadius);const auto gameplaySupport=normal.debugPlayerSupportAt(deployed.prop.center.x,deployed.prop.center.z);if(!visual.inside||!near(gameplaySupport.height,visual.height+0.08f)){std::fprintf(stderr,"ROCK_SUPPORT_FAIL generated shared facet\n");return 1;}}
    std::puts("SLOPE_FIXTURE_OK approach ascent plateau descent lateral stop reversal jump double-jump landing melee vacuum shot lunge camera bounded-speed rock-facet-support rock-jump rock-fall rock-side-obstruction rock-combat no-box-top deterministic-room-rock-deployment generated-surface-controller-sweep body-clearance visible-phone-clearance action-sweep camera-clearance");
    return 0;
}
