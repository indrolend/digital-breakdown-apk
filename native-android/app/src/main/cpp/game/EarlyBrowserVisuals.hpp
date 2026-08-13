#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Math.hpp"

namespace early_browser_visuals {

enum class RoomPremise : unsigned char { Field, City, Sterile };

struct RoomEnvironmentPlan {
    RoomPremise premise = RoomPremise::Field;
    int obstacleCount = 0;
    bool recovery = false;
    bool grass = true;
    bool sidewalks = false;
    float grassAmount = 1.0f;
    int enemyAdjustment = 0;
};

struct ObstacleSpec { Vec3 center; Vec3 size; };
struct GrassBlade { Vec3 root; float height = 0.3f; float width = 0.035f; float phase = 0.0f; };
struct GrassReactionInputs { Vec3 player; Vec3 vacuumOrigin; Vec3 shotOrigin; float vacuumStrength=0.0f; float shotAge=9999.0f; };

constexpr int GrassBladeCountLow = 160;
constexpr int GrassBladeCountHigh = 320;

inline std::uint32_t mix(std::uint32_t value) {
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

inline float unit(std::uint32_t value) {
    return static_cast<float>(mix(value) & 0x00ffffffu) / 16777215.0f;
}

inline std::uint32_t roomKey(int roomSeed,int roomIndex) {
    return mix(static_cast<std::uint32_t>(roomSeed)^static_cast<std::uint32_t>(roomIndex)*0x9e3779b9u);
}

inline RoomEnvironmentPlan roomPlan(int roomSeed,int roomIndex) {
    RoomEnvironmentPlan plan;
    const std::uint32_t key=roomKey(roomSeed,roomIndex);
    const float recoveryChance=std::min(0.22f,0.10f+std::max(0,roomIndex-4)*0.004f);
    plan.recovery=roomIndex>=4&&unit(key+91u)<recoveryChance;
    const float roll=unit(key+17u);
    if(roomIndex==1||plan.recovery) plan.premise=RoomPremise::Field;
    else if(roomIndex<8) plan.premise=roll<0.46f?RoomPremise::Field:(roll<0.80f?RoomPremise::City:RoomPremise::Sterile);
    else plan.premise=roll<0.30f?RoomPremise::Field:(roll<0.68f?RoomPremise::City:RoomPremise::Sterile);
    if(plan.premise==RoomPremise::Field){plan.obstacleCount=plan.recovery?1:3;plan.grass=true;plan.grassAmount=plan.recovery?1.0f:0.82f;plan.enemyAdjustment=plan.recovery?-2:0;}
    else if(plan.premise==RoomPremise::City){plan.obstacleCount=10;plan.grass=false;plan.sidewalks=true;}
    else {plan.obstacleCount=6;plan.grass=false;}
    return plan;
}

inline ObstacleSpec obstacle(const RoomEnvironmentPlan& plan,int roomSeed,int roomIndex,int index) {
    const std::uint32_t key=roomKey(roomSeed,roomIndex)+static_cast<std::uint32_t>(index)*131u;
    if(plan.premise==RoomPremise::Field){
        const float side=(index&1)?1.0f:-1.0f;
        const float x=side*(6.8f+unit(key+1u)*4.8f),z=-7.0f+index*7.0f+unit(key+2u)*1.4f;
        const float w=1.1f+unit(key+3u)*1.4f,d=1.1f+unit(key+4u)*1.4f,h=0.45f+unit(key+5u)*0.65f;
        return {{x,h*0.5f,z},{w,h,d}};
    }
    if(plan.premise==RoomPremise::City){
        const float side=(index&1)?1.0f:-1.0f;
        const int row=index/2;
        const float w=3.4f+unit(key+3u)*1.6f,d=3.0f+unit(key+4u)*1.8f,h=1.2f+unit(key+5u)*2.3f;
        return {{side*(7.0f+unit(key+1u)*2.2f),h*0.5f,-12.0f+row*6.0f+unit(key+2u)*0.7f},{w,h,d}};
    }
    const int row=index/2;const float side=(index&1)?1.0f:-1.0f;
    const float w=2.4f+unit(key+3u)*0.8f,d=2.4f+unit(key+4u)*0.8f,h=0.75f+row*0.28f;
    return {{side*(4.1f+row*1.35f),h*0.5f,-8.0f+row*8.0f},{w,h,d}};
}

inline GrassBlade grassBlade(int roomSeed,int roomIndex,int tileIndex,int index) {
    const std::uint32_t room=roomKey(roomSeed,roomIndex)^static_cast<std::uint32_t>(tileIndex*4099);
    const std::uint32_t key=room^static_cast<std::uint32_t>(index*131);
    float x,z;
    if(unit(key+19u)<0.82f){
        const int patch=static_cast<int>(unit(key+23u)*12.0f)%12;
        const std::uint32_t patchKey=room+static_cast<std::uint32_t>(patch*977);
        const float centerX=-11.4f+unit(patchKey+1u)*22.8f,centerZ=-15.8f+unit(patchKey+2u)*31.6f;
        const float radius=1.0f+unit(patchKey+3u)*2.3f,angle=unit(key+29u)*6.2831853f,radiusSample=std::sqrt(unit(key+31u))*radius;
        x=centerX+std::cos(angle)*radiusSample;z=centerZ+std::sin(angle)*radiusSample;
    } else {x=-13.2f+unit(key)*26.4f;z=-18.0f+unit(key+1u)*36.0f;}
    x=std::max(-13.2f,std::min(13.2f,x+(unit(key+37u)-0.5f)*0.45f));
    z=std::max(-18.0f,std::min(18.0f,z+(unit(key+41u)-0.5f)*0.45f));
    return {{x,0.02f,z},0.225f+unit(key+2u)*0.56f,0.055f+unit(key+3u)*0.035f,unit(key+4u)*6.2831853f};
}

inline float smooth01(float value){value=std::max(0.0f,std::min(1.0f,value));return value*value*(3.0f-2.0f*value);}
inline Vec3 grassTip(const GrassBlade& blade,float time,const GrassReactionInputs& input) {
    Vec3 tip{blade.root.x,blade.root.y+blade.height,blade.root.z};
    const Vec3 playerDelta=blade.root-input.player;const float playerDistance=std::sqrt(playerDelta.x*playerDelta.x+playerDelta.z*playerDelta.z);
    const float windMask=1.0f-smooth01((playerDistance-4.0f)/8.0f);
    tip.x+=std::sin(time*2.0f+blade.root.x*0.65f+blade.root.z*0.45f+blade.phase)*0.07f*windMask;
    if(playerDistance<0.9f&&playerDistance>0.001f){const float power=1.0f-playerDistance/0.9f;tip.x+=playerDelta.x/playerDistance*power*0.3f;tip.z+=playerDelta.z/playerDistance*power*0.3f;tip.y-=power*0.08f;}
    if(input.shotAge>=0.0f&&input.shotAge<1.4f){const Vec3 delta=blade.root-input.shotOrigin;const float distance=std::sqrt(delta.x*delta.x+delta.z*delta.z),inv=distance>0.001f?1.0f/distance:0.0f;const float wave=input.shotAge*7.5f,ring=1.0f-smooth01(std::abs(distance-wave)/0.85f),range=1.0f-smooth01(distance/7.5f),decay=std::exp(-input.shotAge*2.4f),wobble=std::sin(input.shotAge*18.0f-distance*2.0f)*decay,blast=ring*range*decay,after=wobble*range*0.22f;tip.x+=delta.x*inv*(blast*0.9f+after);tip.z+=delta.z*inv*(blast*0.9f+after);tip.y-=blast*0.14f;}
    if(input.vacuumStrength>0.01f){const Vec3 delta=input.vacuumOrigin-blade.root;const float distance=std::sqrt(delta.x*delta.x+delta.z*delta.z),inv=distance>0.001f?1.0f/distance:0.0f,pullMask=1.0f-smooth01((distance-0.5f)/7.5f),pulse=0.75f+0.25f*std::sin(time*2.0f*18.0f+distance*3.0f),pull=pullMask*pulse*input.vacuumStrength;tip.x+=delta.x*inv*pull*0.45f;tip.z+=delta.z*inv*pull*0.45f;tip.y-=pull*0.1f;}
    return tip;
}

} // namespace early_browser_visuals
