#pragma once

#include "EarlyBrowserVisuals.hpp"

#include <array>
#include <cmath>

namespace tree_geometry {

struct Part { Vec3 center{};Vec3 size{1,1,1};float yaw=0; };
constexpr int TrunkPartCount=3;
constexpr int CrownPartCount=7;

inline std::array<Part,TrunkPartCount> trunkParts(const early_browser_visuals::EnvironmentPropSpec& prop,float zOffset=0.0f){
    const Vec3 origin=prop.center+Vec3{0,0,zOffset};
    return {{
        {origin+Vec3{-prop.size.x*0.018f,prop.size.y*0.18f,0},{prop.size.x*0.26f,prop.size.y*0.36f,prop.size.z*0.26f},prop.yaw-0.035f},
        {origin+Vec3{ prop.size.x*0.012f,prop.size.y*0.49f,0},{prop.size.x*0.21f,prop.size.y*0.30f,prop.size.z*0.21f},prop.yaw+0.025f},
        {origin+Vec3{-prop.size.x*0.010f,prop.size.y*0.70f,0},{prop.size.x*0.16f,prop.size.y*0.18f,prop.size.z*0.16f},prop.yaw-0.018f}
    }};
}

inline std::array<Part,CrownPartCount> crownParts(const early_browser_visuals::EnvironmentPropSpec& prop,float zOffset=0.0f){
    std::array<Part,CrownPartCount> result{};const Vec3 origin=prop.center+Vec3{0,0,zOffset};
    const float c=std::cos(prop.yaw),s=std::sin(prop.yaw);int index=0;
    for(const auto& cluster:early_browser_visuals::treeFoliageClusters()){
        const Vec3 local{cluster.offset.x*prop.size.x,cluster.offset.y*prop.size.y,cluster.offset.z*prop.size.z};
        result[index++]={origin+Vec3{local.x*c+local.z*s,local.y,-local.x*s+local.z*c},{cluster.scale.x*prop.size.x,cluster.scale.y*prop.size.y,cluster.scale.z*prop.size.z},prop.yaw+cluster.yawOffset};
    }
    return result;
}

inline float climbTopY(const early_browser_visuals::EnvironmentPropSpec& prop){return prop.center.y+prop.size.y*1.18f;}

} // namespace tree_geometry
