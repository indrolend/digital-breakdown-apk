#pragma once

#include "Math.hpp"

#include <array>
#include <cmath>

enum class SlopeAxis : unsigned char { PositiveX, NegativeX, PositiveZ, NegativeZ };
enum class SupportClassification : unsigned char { Ordinary, TraversableSlope, Steep };

struct SlopeSupport {
    float minX=0.0f,maxX=0.0f,minZ=0.0f,maxZ=0.0f;
    float lowHeight=0.0f,highHeight=0.0f;
    SlopeAxis axis=SlopeAxis::NegativeZ;
};

struct SlopeSupportSample {
    bool inside=false;
    float height=0.0f;
    Vec3 normal{0.0f,1.0f,0.0f};
    SupportClassification classification=SupportClassification::Ordinary;
};

inline float slopeRun(const SlopeSupport& slope){
    return slope.axis==SlopeAxis::PositiveX||slope.axis==SlopeAxis::NegativeX
        ?slope.maxX-slope.minX:slope.maxZ-slope.minZ;
}

inline Vec3 slopeSupportNormal(const SlopeSupport& slope){
    const float run=std::max(0.0001f,slopeRun(slope));
    const float grade=(slope.highHeight-slope.lowHeight)/run;
    switch(slope.axis){
        case SlopeAxis::PositiveX:return normalized({-grade,1.0f,0.0f});
        case SlopeAxis::NegativeX:return normalized({grade,1.0f,0.0f});
        case SlopeAxis::PositiveZ:return normalized({0.0f,1.0f,-grade});
        case SlopeAxis::NegativeZ:return normalized({0.0f,1.0f,grade});
    }
    return {0.0f,1.0f,0.0f};
}

inline SupportClassification classifySupport(const SlopeSupport& slope){
    if(std::fabs(slope.highHeight-slope.lowHeight)<0.0001f)return SupportClassification::Ordinary;
    constexpr float MinimumWalkableNormalY=0.819152f; // 35 degrees from world up.
    return slopeSupportNormal(slope).y>=MinimumWalkableNormalY
        ?SupportClassification::TraversableSlope:SupportClassification::Steep;
}

inline SlopeSupportSample sampleSlopeSupport(const SlopeSupport& slope,float x,float z){
    SlopeSupportSample sample{};
    if(x<slope.minX||x>slope.maxX||z<slope.minZ||z>slope.maxZ)return sample;
    const float run=std::max(0.0001f,slopeRun(slope));
    float t=0.0f;
    switch(slope.axis){
        case SlopeAxis::PositiveX:t=(x-slope.minX)/run;break;
        case SlopeAxis::NegativeX:t=(slope.maxX-x)/run;break;
        case SlopeAxis::PositiveZ:t=(z-slope.minZ)/run;break;
        case SlopeAxis::NegativeZ:t=(slope.maxZ-z)/run;break;
    }
    sample.inside=true;sample.height=slope.lowHeight+(slope.highHeight-slope.lowHeight)*clampf(t,0.0f,1.0f);
    sample.normal=slopeSupportNormal(slope);sample.classification=classifySupport(slope);
    return sample;
}

constexpr int SlopeWedgeVertexCount=36;
struct SlopeWedgeMesh { std::array<float,SlopeWedgeVertexCount*3> positions{};std::array<float,SlopeWedgeVertexCount*3> normals{};int vertexCount=0; };

inline Vec3 slopeFaceNormal(const Vec3& a,const Vec3& b,const Vec3& c){
    const Vec3 u=b-a,v=c-a;return normalized({u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x});
}

inline SlopeWedgeMesh makeSlopeWedgeMesh(const SlopeSupport& slope,float zOffset=0.0f){
    SlopeWedgeMesh mesh{};
    const auto heightAt=[&](float x,float z){return sampleSlopeSupport(slope,x,z).height;};
    const Vec3 a{slope.minX,heightAt(slope.minX,slope.minZ),slope.minZ+zOffset},b{slope.maxX,heightAt(slope.maxX,slope.minZ),slope.minZ+zOffset};
    const Vec3 c{slope.maxX,heightAt(slope.maxX,slope.maxZ),slope.maxZ+zOffset},d{slope.minX,heightAt(slope.minX,slope.maxZ),slope.maxZ+zOffset};
    const Vec3 ab{slope.minX,0.0f,slope.minZ+zOffset},bb{slope.maxX,0.0f,slope.minZ+zOffset},cb{slope.maxX,0.0f,slope.maxZ+zOffset},db{slope.minX,0.0f,slope.maxZ+zOffset};
    const auto emit=[&](const Vec3& p0,const Vec3& p1,const Vec3& p2){const Vec3 n=slopeFaceNormal(p0,p1,p2);for(const Vec3& p:{p0,p1,p2}){mesh.positions[mesh.vertexCount*3]=p.x;mesh.positions[mesh.vertexCount*3+1]=p.y;mesh.positions[mesh.vertexCount*3+2]=p.z;mesh.normals[mesh.vertexCount*3]=n.x;mesh.normals[mesh.vertexCount*3+1]=n.y;mesh.normals[mesh.vertexCount*3+2]=n.z;++mesh.vertexCount;}};
    emit(a,d,c);emit(a,c,b); // support plane
    emit(ab,bb,cb);emit(ab,cb,db); // underside
    emit(ab,db,d);emit(ab,d,a);emit(bb,b,c);emit(bb,c,cb); // sides
    emit(ab,a,b);emit(ab,b,bb);emit(db,cb,c);emit(db,c,d); // bounded ends
    return mesh;
}
