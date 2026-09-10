#pragma once

#include "EarlyBrowserVisuals.hpp"
#include "SurfaceGeometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace faceted_rock {

constexpr int RingVertexCount=6;
constexpr int TriangleCount=RingVertexCount*3;
constexpr int VertexCount=TriangleCount*3;

struct Mesh {
    std::array<float,VertexCount*3> positions{};
    std::array<float,VertexCount*3> normals{};
    int vertexCount=0;
};

constexpr float WalkableNormalY=0.819152044f; // cos(35 degrees)

struct Support {
    early_browser_visuals::EnvironmentPropSpec prop{};
    int roomSeed=0;
    int roomIndex=0;
    int propIndex=0;
};

using SurfaceSample=surface_geometry::Sample;

inline Vec3 faceNormal(const Vec3& a,const Vec3& b,const Vec3& c){
    return surface_geometry::faceNormal(a,b,c);
}

inline bool eligible(early_browser_visuals::RoomSetting setting,
                     early_browser_visuals::EnvironmentRole role) {
    using early_browser_visuals::EnvironmentRole;
    using early_browser_visuals::RoomSetting;
    if(setting!=RoomSetting::Field&&setting!=RoomSetting::Coastal)return false;
    return role==EnvironmentRole::Mass||role==EnvironmentRole::Landmark;
}

inline std::uint32_t shapeKey(int roomSeed,int roomIndex,int propIndex,
                              early_browser_visuals::EnvironmentRole role) {
    return early_browser_visuals::mix(early_browser_visuals::roomKey(roomSeed,roomIndex)
        ^static_cast<std::uint32_t>(propIndex+1)*0x45d9f3bu
        ^static_cast<std::uint32_t>(role)*0x9e3779b9u);
}

inline Mesh makeMesh(const early_browser_visuals::EnvironmentPropSpec& prop,
                     int roomSeed,int roomIndex,int propIndex,float zOffset=0.0f) {
    Mesh mesh{};
    const std::uint32_t key=shapeKey(roomSeed,roomIndex,propIndex,prop.role);
    const bool landmark=prop.role==early_browser_visuals::EnvironmentRole::Landmark;
    const float halfX=prop.size.x*0.5f,halfZ=prop.size.z*0.5f;
    std::array<Vec3,RingVertexCount> base{},shoulder{};
    const float cosine=std::cos(prop.yaw),sine=std::sin(prop.yaw);
    const auto place=[&](float x,float y,float z){
        const float rotatedX=x*cosine+z*sine,rotatedZ=-x*sine+z*cosine;
        return prop.center+Vec3{clampf(rotatedX,-halfX*0.94f,halfX*0.94f),y,
                                clampf(rotatedZ,-halfZ*0.94f,halfZ*0.94f)+zOffset};
    };
    for(int i=0;i<RingVertexCount;++i){
        const float phase=6.283185307f*static_cast<float>(i)/RingVertexCount;
        const float angle=phase+(early_browser_visuals::unit(key+static_cast<std::uint32_t>(i)*11u)-0.5f)*(landmark?0.18f:0.10f);
        const float baseRadius=0.88f+early_browser_visuals::unit(key+static_cast<std::uint32_t>(i)*11u+1u)*0.08f;
        const float shoulderRadius=(landmark?0.55f:0.63f)+early_browser_visuals::unit(key+static_cast<std::uint32_t>(i)*11u+2u)*(landmark?0.20f:0.10f);
        base[i]=place(std::cos(angle)*halfX*baseRadius,0.0f,std::sin(angle)*halfZ*baseRadius);
        shoulder[i]=place(std::cos(angle)*halfX*shoulderRadius,
            prop.size.y*(0.58f+early_browser_visuals::unit(key+static_cast<std::uint32_t>(i)*11u+3u)*0.12f),
            std::sin(angle)*halfZ*shoulderRadius);
    }
    const Vec3 crown=place((early_browser_visuals::unit(key+101u)-0.5f)*halfX*(landmark?0.28f:0.14f),
                           prop.size.y*(0.91f+early_browser_visuals::unit(key+103u)*0.08f),
                           (early_browser_visuals::unit(key+107u)-0.5f)*halfZ*(landmark?0.28f:0.14f));
    const auto emit=[&](const Vec3& a,const Vec3& b,const Vec3& c){
        const Vec3 normal=faceNormal(a,b,c);
        for(const Vec3& p:{a,b,c}){
            mesh.positions[mesh.vertexCount*3]=p.x;mesh.positions[mesh.vertexCount*3+1]=p.y;mesh.positions[mesh.vertexCount*3+2]=p.z;
            mesh.normals[mesh.vertexCount*3]=normal.x;mesh.normals[mesh.vertexCount*3+1]=normal.y;mesh.normals[mesh.vertexCount*3+2]=normal.z;
            ++mesh.vertexCount;
        }
    };
    for(int i=0;i<RingVertexCount;++i){const int next=(i+1)%RingVertexCount;emit(base[i],shoulder[next],base[next]);emit(base[i],shoulder[i],shoulder[next]);emit(shoulder[i],crown,shoulder[next]);}
    return mesh;
}

inline bool triangleWalkable(const Vec3& normal){return normal.y>=WalkableNormalY;}

inline bool sampleProjectedTriangle(const Vec3& a,const Vec3& b,const Vec3& c,float x,float z,float& height){
    return surface_geometry::projectedTriangleHeight(a,b,c,x,z,height);
}

inline SurfaceSample sampleSurface(const Mesh& mesh,float x,float z,bool walkableOnly){
    return surface_geometry::sample(mesh,x,z,0.0f,walkableOnly?WalkableNormalY:0.0f);
}

inline SurfaceSample sampleSurface(const Support& support,float x,float z,bool walkableOnly){
    return sampleSurface(makeMesh(support.prop,support.roomSeed,support.roomIndex,support.propIndex),x,z,walkableOnly);
}

inline SurfaceSample sampleSupport(const Support& support,float x,float z){return sampleSurface(support,x,z,true);}
inline SurfaceSample sampleEnvelope(const Support& support,float x,float z){return sampleSurface(support,x,z,false);}

inline bool projectedTriangleOverlapsCircle(const Vec3& a,const Vec3& b,const Vec3& c,float x,float z,float radius){
    return surface_geometry::projectedTriangleOverlapsCircle(a,b,c,x,z,radius);
}

inline SurfaceSample sampleMeshFootprint(const Mesh& mesh,float x,float z,float radius,bool requireWalkable){
    return surface_geometry::sample(mesh,x,z,radius,requireWalkable?WalkableNormalY:0.0f);
}

// Treat the player as a bounded cylindrical footprint. Any upward walkable
// facet beneath that footprint can support it, just as a real body remains on
// an edge until its footprint clears. Once eligible, every upward neighboring
// face contributes to the required clearance height.
inline SurfaceSample sampleSupportFootprint(const Support& support,float x,float z,float radius){
    const auto mesh=makeMesh(support.prop,support.roomSeed,support.roomIndex,support.propIndex);
    return sampleMeshFootprint(mesh,x,z,radius,true);
}

inline SurfaceSample sampleEnvelopeFootprint(const Mesh& mesh,float x,float z,float radius){
    return sampleMeshFootprint(mesh,x,z,radius,false);
}

inline SurfaceSample sampleEnvelopeFootprint(const Support& support,float x,float z,float radius){
    return sampleEnvelopeFootprint(makeMesh(support.prop,support.roomSeed,support.roomIndex,support.propIndex),x,z,radius);
}

} // namespace faceted_rock
