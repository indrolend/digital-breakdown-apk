#pragma once

#include "EarlyBrowserVisuals.hpp"

#include <array>

namespace marker_pillar_geometry {

constexpr int PartCount=2;

struct Part { Vec3 center{};Vec3 size{1,1,1};unsigned char surface=0; };

// Sterile markers are intentionally axis-aligned manufactured forms. Keeping
// the shared definition unrotated lets RoomCollider exactly match both visible
// pieces instead of approximating a decorative rotated cap.
inline std::array<Part,PartCount> parts(const early_browser_visuals::EnvironmentPropSpec& prop,float zOffset=0.0f){
    const Vec3 origin=prop.center+Vec3{0,0,zOffset};
    return {{
        {origin+Vec3{0,prop.size.y*0.5f,0},prop.size,0},
        {origin+Vec3{0,prop.size.y+0.08f,0},{prop.size.x*1.28f,0.16f,prop.size.z*1.28f},1}
    }};
}

inline early_browser_visuals::ObstacleSpec collider(const Part& part){return {part.center,part.size};}

} // namespace marker_pillar_geometry
