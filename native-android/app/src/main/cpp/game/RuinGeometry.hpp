#pragma once

#include "EarlyBrowserVisuals.hpp"

#include <array>
#include <cmath>

namespace ruin_geometry {

constexpr int PartCount=2;

struct Part {
    Vec3 center{};
    Vec3 size{1.0f,1.0f,1.0f};
    float yaw=0.0f;
    unsigned char surface=0;
};

struct Bounds { float minX=0,maxX=0,minZ=0,maxZ=0,bottomY=0,topY=0; };

inline std::array<Part,PartCount> parts(const early_browser_visuals::EnvironmentPropSpec& prop,float zOffset=0.0f){
    const float w=prop.size.x,h=prop.size.y,d=prop.size.z;
    const Vec3 origin=prop.center+Vec3{0,0,zOffset};
    return {{
        {origin+Vec3{0,h*0.38f,0},{w,h*0.76f,d},prop.yaw,0},
        {origin+Vec3{w*0.28f,h*0.88f,0},{w*0.34f,h*0.24f,d*0.82f},prop.yaw,1}
    }};
}

inline Bounds bounds(const Part& part){
    const float c=std::abs(std::cos(part.yaw)),s=std::abs(std::sin(part.yaw));
    const float halfX=(part.size.x*c+part.size.z*s)*0.5f;
    const float halfZ=(part.size.x*s+part.size.z*c)*0.5f;
    return {part.center.x-halfX,part.center.x+halfX,part.center.z-halfZ,part.center.z+halfZ,
            part.center.y-part.size.y*0.5f,part.center.y+part.size.y*0.5f};
}

} // namespace ruin_geometry
