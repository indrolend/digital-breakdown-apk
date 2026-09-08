#include "Game.hpp"

#include <cmath>
#include <cstdio>

namespace {
constexpr float Dt=1.0f/60.0f;
void input(Game& game,float x,float z,bool vacuum=false,bool jump=false,bool melee=false,bool shoot=false){game.setTouchControls(x,z,0,0,vacuum,false,jump,melee,shoot,false);game.update(Dt);}
void placeOnSlope(Game& game,float x,float z){auto& state=game.networkMutableState();const auto sample=sampleSlopeSupport(state.slopeSupports[0],x,z);state.player.pos={x,sample.height+0.08f,z};state.player.vel={};state.player.jumpVel=0;state.player.grounded=true;state.player.battery=100;state.camera.yaw=0;}
bool near(float a,float b,float tolerance=0.025f){return std::fabs(a-b)<=tolerance;}
float horizontalSpeed(const Vec3& velocity){return std::sqrt(velocity.x*velocity.x+velocity.z*velocity.z);}
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

    Game normal;normal.reset();if(normal.state().slopeLab||normal.state().slopeSupportCount!=0){std::fprintf(stderr,"SLOPE_FIXTURE_FAIL leaked into normal rooms\n");return 1;}
    std::puts("SLOPE_FIXTURE_OK approach ascent plateau descent lateral stop reversal jump double-jump landing melee vacuum shot lunge camera bounded-speed");
    return 0;
}
