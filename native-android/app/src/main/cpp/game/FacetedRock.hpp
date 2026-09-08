#pragma once

#include "EarlyBrowserVisuals.hpp"

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

struct SurfaceSample {
    bool inside=false;
    float height=0.0f;
    Vec3 normal{0.0f,1.0f,0.0f};
};

inline Vec3 faceNormal(const Vec3& a,const Vec3& b,const Vec3& c){
    const Vec3 u=b-a,v=c-a;
    return normalized({u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x});
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
    const float denominator=(b.z-c.z)*(a.x-c.x)+(c.x-b.x)*(a.z-c.z);
    if(std::abs(denominator)<0.000001f)return false;
    const float u=((b.z-c.z)*(x-c.x)+(c.x-b.x)*(z-c.z))/denominator;
    const float v=((c.z-a.z)*(x-c.x)+(a.x-c.x)*(z-c.z))/denominator;
    const float w=1.0f-u-v;
    constexpr float EdgeTolerance=0.0001f;
    if(u<-EdgeTolerance||v<-EdgeTolerance||w<-EdgeTolerance)return false;
    height=u*a.y+v*b.y+w*c.y;
    return std::isfinite(height);
}

inline SurfaceSample sampleSurface(const Support& support,float x,float z,bool walkableOnly){
    SurfaceSample result{};const auto mesh=makeMesh(support.prop,support.roomSeed,support.roomIndex,support.propIndex);
    for(int vertex=0;vertex+2<mesh.vertexCount;vertex+=3){
        const auto point=[&](int index){return Vec3{mesh.positions[index*3],mesh.positions[index*3+1],mesh.positions[index*3+2]};};
        const Vec3 a=point(vertex),b=point(vertex+1),c=point(vertex+2),normal=faceNormal(a,b,c);
        if(normal.y<=0.0001f||(walkableOnly&&!triangleWalkable(normal)))continue;
        float height=0.0f;if(!sampleProjectedTriangle(a,b,c,x,z,height))continue;
        if(!result.inside||height>result.height){result.inside=true;result.height=height;result.normal=normal;}
    }
    return result;
}

inline SurfaceSample sampleSupport(const Support& support,float x,float z){return sampleSurface(support,x,z,true);}
inline SurfaceSample sampleEnvelope(const Support& support,float x,float z){return sampleSurface(support,x,z,false);}

} // namespace faceted_rock
