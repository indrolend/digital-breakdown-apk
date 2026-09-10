#pragma once

#include "Math.hpp"

#include <cmath>

namespace surface_geometry {

struct Sample {
    bool inside=false;
    float height=0.0f;
    Vec3 normal{0.0f,1.0f,0.0f};
};

inline Vec3 faceNormal(const Vec3& a,const Vec3& b,const Vec3& c){
    const Vec3 u=b-a,v=c-a;
    return normalized({u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x});
}

inline bool projectedTriangleHeight(const Vec3& a,const Vec3& b,const Vec3& c,float x,float z,float& height){
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

inline bool projectedTriangleOverlapsCircle(const Vec3& a,const Vec3& b,const Vec3& c,float x,float z,float radius){
    float ignored=0.0f;if(projectedTriangleHeight(a,b,c,x,z,ignored))return true;
    const float radiusSq=radius*radius;
    const auto vertexInside=[&](const Vec3& p){const float dx=p.x-x,dz=p.z-z;return dx*dx+dz*dz<=radiusSq;};
    if(vertexInside(a)||vertexInside(b)||vertexInside(c))return true;
    const auto edgeNear=[&](const Vec3& p0,const Vec3& p1){const float dx=p1.x-p0.x,dz=p1.z-p0.z,lengthSq=dx*dx+dz*dz;if(lengthSq<0.0000001f)return vertexInside(p0);const float t=clampf(((x-p0.x)*dx+(z-p0.z)*dz)/lengthSq,0.0f,1.0f);const float qx=p0.x+dx*t-x,qz=p0.z+dz*t-z;return qx*qx+qz*qz<=radiusSq;};
    return edgeNear(a,b)||edgeNear(b,c)||edgeNear(c,a);
}

template<class Mesh>
Sample sample(const Mesh& mesh,float x,float z,float radius,float minimumNormalY){
    Sample result{};bool eligible=minimumNormalY<=0.0f;
    const auto point=[&](int index){return Vec3{mesh.positions[index*3],mesh.positions[index*3+1],mesh.positions[index*3+2]};};
    if(!eligible)for(int vertex=0;vertex+2<mesh.vertexCount;vertex+=3){const Vec3 a=point(vertex),b=point(vertex+1),c=point(vertex+2),normal=faceNormal(a,b,c);if(normal.y>=minimumNormalY&&projectedTriangleOverlapsCircle(a,b,c,x,z,radius)){eligible=true;break;}}
    if(!eligible)return result;
    const float radiusSq=radius*radius;
    for(int vertex=0;vertex+2<mesh.vertexCount;vertex+=3){
        const Vec3 a=point(vertex),b=point(vertex+1),c=point(vertex+2),normal=faceNormal(a,b,c);if(normal.y<=0.0001f||!projectedTriangleOverlapsCircle(a,b,c,x,z,radius))continue;
        const auto consider=[&](float px,float pz){const float dx=px-x,dz=pz-z;if(dx*dx+dz*dz>radiusSq+0.000001f)return;float height=0.0f;if(projectedTriangleHeight(a,b,c,px,pz,height)&&(!result.inside||height>result.height)){result.inside=true;result.height=height;result.normal=normal;}};
        consider(x,z);consider(a.x,a.z);consider(b.x,b.z);consider(c.x,c.z);
        if(radius<=0.0f)continue;
        const float gradientX=-normal.x/normal.y,gradientZ=-normal.z/normal.y,gradientLength=std::sqrt(gradientX*gradientX+gradientZ*gradientZ);
        if(gradientLength>0.000001f)consider(x+radius*gradientX/gradientLength,z+radius*gradientZ/gradientLength);
        const auto edgeIntersections=[&](const Vec3& p0,const Vec3& p1){const float dx=p1.x-p0.x,dz=p1.z-p0.z,ox=p0.x-x,oz=p0.z-z;const float qa=dx*dx+dz*dz;if(qa<0.0000001f)return;const float qb=2.0f*(ox*dx+oz*dz),qc=ox*ox+oz*oz-radiusSq,disc=qb*qb-4.0f*qa*qc;if(disc<0.0f)return;const float root=std::sqrt(std::max(0.0f,disc));const float t0=(-qb-root)/(2.0f*qa),t1=(-qb+root)/(2.0f*qa);if(t0>=0.0f&&t0<=1.0f)consider(p0.x+dx*t0,p0.z+dz*t0);if(t1>=0.0f&&t1<=1.0f)consider(p0.x+dx*t1,p0.z+dz*t1);};
        edgeIntersections(a,b);edgeIntersections(b,c);edgeIntersections(c,a);
    }
    return result;
}

} // namespace surface_geometry
