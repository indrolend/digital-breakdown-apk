#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Math.hpp"
#include "gameplay/TraversalCapabilities.hpp"
#include "gameplay/TraversalGraph.hpp"

namespace early_browser_visuals {

enum class RoomSetting : unsigned char { Field, City, Sterile, Coastal };
enum class RoomForm : unsigned char { Open, Corridor, Courtyard, Canyon, Skyline, Shore };
enum class RoomScale : unsigned char { Compact, Standard, Large, Arena };
enum class RoomCondition : unsigned char { Normal, Recovery };

struct RoomEnvironmentPlan {
    RoomSetting setting = RoomSetting::Field;
    RoomForm form = RoomForm::Open;
    RoomScale scale = RoomScale::Standard;
    RoomCondition condition = RoomCondition::Normal;
    int obstacleCount = 0;
    bool grass = true;
    bool sidewalks = false;
    float grassAmount = 1.0f;
    int enemyAdjustment = 0;
    gameplay::TraversalGraph traversal{};

    constexpr bool recovery() const { return condition == RoomCondition::Recovery; }
};

struct ObstacleSpec { Vec3 center; Vec3 size; };
enum class EnvironmentPrimitive : unsigned char { House, Tree, LawnFragment, MarkerPillar, Ruin };
struct EnvironmentPropSpec { EnvironmentPrimitive primitive=EnvironmentPrimitive::MarkerPillar;Vec3 center{};Vec3 size{1,1,1};float yaw=0;unsigned char variant=0; };
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
    plan.condition=roomIndex>=4&&unit(key+91u)<recoveryChance?RoomCondition::Recovery:RoomCondition::Normal;
    const float roll=unit(key+17u);
    if(roomIndex==1||plan.recovery()) plan.setting=RoomSetting::Field;
    else if(roomIndex<8) plan.setting=roll<0.42f?RoomSetting::Field:(roll<0.74f?RoomSetting::City:(roll<0.91f?RoomSetting::Sterile:RoomSetting::Coastal));
    else plan.setting=roll<0.27f?RoomSetting::Field:(roll<0.60f?RoomSetting::City:(roll<0.82f?RoomSetting::Sterile:RoomSetting::Coastal));

    plan.scale=unit(key+143u)<0.18f?RoomScale::Compact:(unit(key+143u)<0.82f?RoomScale::Standard:RoomScale::Large);
    if(plan.setting==RoomSetting::Field){plan.form=RoomForm::Open;plan.obstacleCount=plan.recovery()?1:3;plan.grass=true;plan.grassAmount=plan.recovery()?1.0f:0.82f;plan.enemyAdjustment=plan.recovery()?-2:0;}
    else if(plan.setting==RoomSetting::City){
        const float formRoll=unit(key+151u);
        plan.form=roomIndex<4?RoomForm::Corridor:
            (formRoll<0.22f?RoomForm::Courtyard:
             formRoll<0.44f?RoomForm::Canyon:
             formRoll<0.62f?RoomForm::Skyline:RoomForm::Corridor);
        plan.obstacleCount=10;plan.grass=false;plan.sidewalks=true;
    }
    else if(plan.setting==RoomSetting::Coastal){plan.form=RoomForm::Shore;plan.obstacleCount=5;plan.grass=false;plan.sidewalks=false;}
    else {plan.form=RoomForm::Corridor;plan.obstacleCount=6;plan.grass=false;}

    plan.traversal.surfaceCount=4;
    plan.traversal.edgeCount=3;
    plan.traversal.surfaces[0]={{0.0f,0.0f,15.5f},{1.8f,0.0f,1.8f},true};
    plan.traversal.surfaces[1]={{0.0f,0.0f,5.0f},{1.8f,0.0f,1.8f},true};
    plan.traversal.surfaces[2]={{0.0f,0.0f,-11.5f},{1.8f,0.0f,1.8f},true};
    plan.traversal.surfaces[3]={{0.0f,0.0f,-19.4f},{1.8f,0.0f,1.0f},true};
    for(int edge=0;edge<plan.traversal.edgeCount;++edge){
        plan.traversal.edges[edge]={edge,edge+1,gameplay::TraversalAction::Walk,gameplay::TraversalDifficulty::Automatic,true};
    }
    return plan;
}

inline ObstacleSpec obstacle(const RoomEnvironmentPlan& plan,int roomSeed,int roomIndex,int index) {
    const std::uint32_t key=roomKey(roomSeed,roomIndex)+static_cast<std::uint32_t>(index)*131u;
    if(plan.setting==RoomSetting::Field){
        const float side=(index&1)?1.0f:-1.0f;
        const float x=side*(6.8f+unit(key+1u)*4.8f),z=-7.0f+index*7.0f+unit(key+2u)*1.4f;
        const float w=1.1f+unit(key+3u)*1.4f,d=1.1f+unit(key+4u)*1.4f,h=0.45f+unit(key+5u)*0.65f;
        return {{x,h*0.5f,z},{w,h,d}};
    }
    if(plan.setting==RoomSetting::City&&plan.form==RoomForm::Courtyard){
        const float scale=plan.scale==RoomScale::Compact?0.86f:(plan.scale==RoomScale::Large?1.12f:1.0f);
        const int sideIndex=index&3,row=index/4;
        const float radius=(7.2f+static_cast<float>(row)*2.1f)*scale;
        const float angle=static_cast<float>(sideIndex)*1.5707963f+0.7853982f+(unit(key+1u)-0.5f)*0.10f;
        const float w=(3.0f+unit(key+3u)*1.1f)*scale,d=(2.8f+unit(key+4u)*1.0f)*scale,h=1.35f+unit(key+5u)*2.0f;
        return {{std::cos(angle)*radius,h*0.5f,std::sin(angle)*radius-1.5f},{w,h,d}};
    }
    if(plan.setting==RoomSetting::City&&plan.form==RoomForm::Canyon){
        const float side=(index&1)?1.0f:-1.0f;const int row=index/2;
        const float w=3.0f+unit(key+3u)*0.8f,d=5.0f+unit(key+4u)*1.8f,h=4.6f+unit(key+5u)*2.4f;
        return {{side*(6.0f+unit(key+1u)*0.65f),h*0.5f,-15.5f+row*7.8f+(unit(key+2u)-0.5f)*0.5f},{w,h,d}};
    }
    if(plan.setting==RoomSetting::City&&plan.form==RoomForm::Skyline){
        const float side=(index&1)?1.0f:-1.0f;const int row=index/2;
        const float w=2.2f+unit(key+3u)*1.2f,d=2.8f+unit(key+4u)*1.6f,h=3.4f+unit(key+5u)*3.6f;
        return {{side*(8.7f+unit(key+1u)*0.55f),h*0.5f,-15.0f+row*7.5f+(unit(key+2u)-0.5f)*1.0f},{w,h,d}};
    }
    if(plan.setting==RoomSetting::City){
        const float side=(index&1)?1.0f:-1.0f;
        const int row=index/2;
        const float w=3.4f+unit(key+3u)*1.6f,d=3.0f+unit(key+4u)*1.8f,h=1.2f+unit(key+5u)*2.3f;
        return {{side*(7.0f+unit(key+1u)*2.2f),h*0.5f,-12.0f+row*6.0f+unit(key+2u)*0.7f},{w,h,d}};
    }
    if(plan.setting==RoomSetting::Coastal){
        const float side=(index&1)?1.0f:-1.0f;
        const float w=1.2f+unit(key+3u)*1.5f,d=1.2f+unit(key+4u)*1.6f,h=0.35f+unit(key+5u)*0.75f;
        return {{side*(5.4f+unit(key+1u)*2.0f),h*0.5f,-13.5f+index*6.7f+(unit(key+2u)-0.5f)*1.0f},{w,h,d}};
    }
    const int row=index/2;const float side=(index&1)?1.0f:-1.0f;
    const float w=2.4f+unit(key+3u)*0.8f,d=2.4f+unit(key+4u)*0.8f,h=0.75f+row*0.28f;
    return {{side*(4.1f+row*1.35f),h*0.5f,-8.0f+row*8.0f},{w,h,d}};
}

inline int environmentPropCount(const RoomEnvironmentPlan& plan){
    if(plan.setting==RoomSetting::Field)return 3;
    if(plan.setting==RoomSetting::City)return plan.form==RoomForm::Courtyard?5:4;
    if(plan.setting==RoomSetting::Coastal)return 4;
    return 4;
}

inline bool environmentPropSolid(const EnvironmentPropSpec& prop){return prop.primitive!=EnvironmentPrimitive::LawnFragment;}
inline ObstacleSpec environmentPropCollider(const EnvironmentPropSpec& prop){
    if(prop.primitive==EnvironmentPrimitive::Tree)return {prop.center+Vec3{0,prop.size.y*0.35f,0},{prop.size.x*0.28f,prop.size.y*0.70f,prop.size.z*0.28f}};
    if(prop.primitive==EnvironmentPrimitive::House||prop.primitive==EnvironmentPrimitive::Ruin)return {prop.center+Vec3{0,prop.size.y*0.52f,0},{prop.size.x,prop.size.y*1.04f,prop.size.z}};
    return {prop.center+Vec3{0,prop.size.y*0.5f,0},prop.size};
}

inline EnvironmentPropSpec environmentProp(const RoomEnvironmentPlan& plan,int roomSeed,int roomIndex,int index){
    const std::uint32_t key=roomKey(roomSeed,roomIndex)+0x51f15e5du+static_cast<std::uint32_t>(index)*313u;
    const float side=(index&1)?1.0f:-1.0f;
    if(plan.setting==RoomSetting::Field){
        const float x=side*(9.2f+unit(key+1u)*2.4f),z=-12.0f+static_cast<float>(index)*11.0f+unit(key+2u)*1.2f;
        return {EnvironmentPrimitive::LawnFragment,{x,0.035f,z},{3.0f+unit(key+3u)*1.8f,0.07f,4.0f+unit(key+4u)*2.2f},unit(key+5u)*0.08f,static_cast<unsigned char>(index%3)};
    }
    if(plan.setting==RoomSetting::City){
        const int row=index/2;const float x=side*(13.05f+unit(key+1u)*0.12f),z=-12.0f+row*(plan.form==RoomForm::Courtyard?8.0f:12.0f)+unit(key+2u)*0.7f;
        const float scale=0.86f+unit(key+3u)*0.20f;
        return {EnvironmentPrimitive::House,{x,0,z},{1.35f*scale,1.45f*scale,1.55f*scale},side*1.5707963f,static_cast<unsigned char>(index%3)};
    }
    if(plan.setting==RoomSetting::Coastal){
        const float x=side*(10.7f+unit(key+1u)*0.8f),z=-12.0f+static_cast<float>(index)*8.0f;
        if(index==1||index==3)return {EnvironmentPrimitive::LawnFragment,{x,0.035f,z},{2.4f+unit(key+3u)*1.2f,0.07f,3.0f+unit(key+4u)*1.6f},unit(key+5u)*0.10f,static_cast<unsigned char>(index)};
        return {EnvironmentPrimitive::Ruin,{x,0,z},{1.7f+unit(key+3u)*0.5f,1.05f+unit(key+4u)*0.45f,1.6f+unit(key+5u)*0.6f},side*1.5707963f,static_cast<unsigned char>(index)};
    }
    const float x=side*(9.6f+unit(key+1u)*1.8f),z=-12.0f+static_cast<float>(index/2)*16.0f;
    return {EnvironmentPrimitive::MarkerPillar,{x,0,z},{0.55f+unit(key+2u)*0.25f,2.0f+unit(key+3u)*1.6f,0.55f+unit(key+4u)*0.25f},unit(key+5u)*0.35f,static_cast<unsigned char>(index%3)};
}

inline bool environmentPropsValid(const RoomEnvironmentPlan& plan,int roomSeed,int roomIndex){
    constexpr float wallInset=0.55f,separation=0.55f;int solidCount=0;
    const auto overlaps=[&](const ObstacleSpec& a,const ObstacleSpec& b){return a.center.x+a.size.x*0.5f+separation>b.center.x-b.size.x*0.5f&&a.center.x-a.size.x*0.5f-separation<b.center.x+b.size.x*0.5f&&a.center.z+a.size.z*0.5f+separation>b.center.z-b.size.z*0.5f&&a.center.z-a.size.z*0.5f-separation<b.center.z+b.size.z*0.5f;};
    for(int i=0;i<environmentPropCount(plan);++i){const auto prop=environmentProp(plan,roomSeed,roomIndex,i);if(!environmentPropSolid(prop))continue;++solidCount;const auto box=environmentPropCollider(prop);
        if(box.center.x-box.size.x*0.5f<-15.0f+wallInset||box.center.x+box.size.x*0.5f>15.0f-wallInset||box.center.z-box.size.z*0.5f<-21.0f+wallInset||box.center.z+box.size.z*0.5f>21.0f-wallInset)return false;
        for(int obstacleIndex=0;obstacleIndex<plan.obstacleCount;++obstacleIndex)if(overlaps(box,obstacle(plan,roomSeed,roomIndex,obstacleIndex)))return false;
        for(int prior=0;prior<i;++prior){const auto priorProp=environmentProp(plan,roomSeed,roomIndex,prior);if(environmentPropSolid(priorProp)&&overlaps(box,environmentPropCollider(priorProp)))return false;}
        for(int node=0;node<plan.traversal.surfaceCount;++node){const auto& surface=plan.traversal.surfaces[node];const ObstacleSpec route{{surface.center.x,0,surface.center.z},{surface.halfSize.x*2,0,surface.halfSize.z*2}};if(overlaps(box,route))return false;}
    }
    return plan.obstacleCount+solidCount<=15;
}

inline bool requiredRouteIsTraversable(const RoomEnvironmentPlan& plan,int roomSeed,int roomIndex,
                                       const gameplay::TraversalCapabilities& capabilities=gameplay::TRAVERSAL_CAPABILITIES) {
    if(!gameplay::validTraversalGraphTopology(plan.traversal))return false;
    const float clearance=capabilities.comfortableClearanceRadius;
    for(int point=0;point<plan.traversal.surfaceCount;++point){
        const Vec3 p=plan.traversal.surfaces[point].center;
        if(std::abs(p.x)>15.0f-clearance||std::abs(p.z)>21.0f-clearance)return false;
    }
    for(int segment=0;segment<plan.traversal.edgeCount;++segment){
        const gameplay::TraversalEdge& edge=plan.traversal.edges[segment];
        if(!edge.required)continue;
        if(edge.action!=gameplay::TraversalAction::Walk)return false;
        const Vec3 a=plan.traversal.surfaces[edge.from].center,b=plan.traversal.surfaces[edge.to].center;
        const float dx=b.x-a.x,dz=b.z-a.z,length=std::sqrt(dx*dx+dz*dz);
        const int samples=std::max(1,static_cast<int>(std::ceil(length/0.25f)));
        for(int sample=0;sample<=samples;++sample){
            const float t=static_cast<float>(sample)/static_cast<float>(samples),x=a.x+dx*t,z=a.z+dz*t;
            for(int index=0;index<plan.obstacleCount;++index){
                const ObstacleSpec spec=obstacle(plan,roomSeed,roomIndex,index);
                if(x>spec.center.x-spec.size.x*0.5f-clearance&&x<spec.center.x+spec.size.x*0.5f+clearance&&
                   z>spec.center.z-spec.size.z*0.5f-clearance&&z<spec.center.z+spec.size.z*0.5f+clearance)return false;
            }
        }
    }
    return true;
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
