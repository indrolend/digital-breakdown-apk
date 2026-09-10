#pragma once

#include "EarlyBrowserVisuals.hpp"

#include <array>
#include <cmath>

namespace house_geometry {

constexpr int PartCount=4;
constexpr int PhysicalPartCount=3;

struct Part { Vec3 center{};Vec3 size{1,1,1};float yaw=0;unsigned char surface=0;bool physical=true; };

inline std::array<Part,PartCount> parts(const early_browser_visuals::EnvironmentPropSpec& prop,float zOffset=0.0f){
    const float w=prop.size.x,h=prop.size.y,d=prop.size.z;
    const Vec3 origin=prop.center+Vec3{0,0,zOffset};
    return {{
        {origin+Vec3{0,h*0.38f,0},{w,h*0.76f,d},prop.yaw,0,true},
        {origin+Vec3{0,h*0.86f,0},{w*0.88f,h*0.20f,d*0.90f},prop.yaw,1,true},
        {origin+Vec3{0,h*1.03f,0},{w*0.62f,h*0.16f,d*0.72f},prop.yaw,2,true},
        {origin+Vec3{std::sin(prop.yaw)*d*0.505f,h*0.25f,std::cos(prop.yaw)*d*0.505f},{w*0.22f,h*0.42f,0.035f},prop.yaw,3,false}
    }};
}

inline early_browser_visuals::ObstacleSpec collider(const Part& part){
    const float c=std::abs(std::cos(part.yaw)),s=std::abs(std::sin(part.yaw));
    return {part.center,{part.size.x*c+part.size.z*s,part.size.y,part.size.x*s+part.size.z*c}};
}

} // namespace house_geometry
