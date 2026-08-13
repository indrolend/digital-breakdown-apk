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

constexpr int GrassBladeCountLow = 40;
constexpr int GrassBladeCountHigh = 96;

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
    const std::uint32_t key=roomKey(roomSeed,roomIndex)^static_cast<std::uint32_t>(tileIndex*4099+index*131);
    return {{-13.2f+unit(key)*26.4f,0.02f,-18.0f+unit(key+1u)*36.0f},0.20f+unit(key+2u)*0.28f,0.025f+unit(key+3u)*0.025f,unit(key+4u)*6.2831853f};
}

inline Vec3 grassTip(const GrassBlade& blade,float time,const Vec3& player,float vacuumStrength,float shotImpulse) {
    const float wind=std::sin(time*1.7f+blade.phase+blade.root.z*0.19f)*0.075f;
    const Vec3 delta=blade.root-player;const float distance=std::sqrt(delta.x*delta.x+delta.z*delta.z);
    const float proximity=distance<1.7f?(1.0f-distance/1.7f):0.0f,invDistance=distance>0.001f?1.0f/distance:0.0f;
    const float away=proximity*0.22f,vacuum=vacuumStrength*proximity*0.16f,impulse=shotImpulse*std::max(0.0f,1.0f-distance/4.0f)*0.24f;
    return {blade.root.x+wind+delta.x*invDistance*(away+impulse-vacuum),blade.root.y+blade.height,blade.root.z+delta.z*invDistance*(away+impulse-vacuum)};
}

} // namespace early_browser_visuals
