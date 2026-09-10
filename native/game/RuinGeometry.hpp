#pragma once

#include "EarlyBrowserVisuals.hpp"

#include <array>

namespace ruin_geometry {

constexpr int PartCount=2;

struct Part {
    Vec3 center{};
    Vec3 size{1.0f,1.0f,1.0f};
    unsigned char surface=0;
};

// Ruins are deliberately assembled from axis-aligned authored blocks. This
// lets the existing RoomCollider describe every visible face exactly instead
// of promising rotated geometry that gameplay cannot represent.
inline std::array<Part,PartCount> parts(const early_browser_visuals::EnvironmentPropSpec& prop,float zOffset=0.0f){
    const float w=prop.size.x,h=prop.size.y,d=prop.size.z;
    const Vec3 origin=prop.center+Vec3{0,0,zOffset};
    return {{
        {origin+Vec3{0,h*0.38f,0},{w,h*0.76f,d},0},
        {origin+Vec3{w*0.28f,h*0.88f,0},{w*0.34f,h*0.24f,d*0.82f},1}
    }};
}

inline early_browser_visuals::ObstacleSpec collider(const Part& part){return {part.center,part.size};}

} // namespace ruin_geometry
