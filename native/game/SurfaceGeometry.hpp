#pragma once

#include "Math.hpp"

#include <cmath>

namespace surface_geometry {

struct Sample {
    bool inside=false;
    float height=0.0f;
    Vec3 normal{0.0f,1.0f,0.0f};
};

struct HorizontalResolution {
    float x=0.0f;
    float z=0.0f;
    Vec3 normal{};
    bool blocked=false;
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

template<class Mesh>
HorizontalResolution resolveHorizontal(const Mesh& mesh,float previousX,float previousZ,float desiredX,float desiredZ,float bodyBottom,float radius){
    HorizontalResolution result{desiredX,desiredZ,{},false};
    const auto blockedAt=[&](float x,float z){const auto envelope=sample(mesh,x,z,radius,0.0f);return envelope.inside&&bodyBottom<envelope.height-0.001f;};
    if(!blockedAt(desiredX,desiredZ))return result;
    result.blocked=true;
    if(blockedAt(previousX,previousZ)){result.x=previousX;result.z=previousZ;return result;}
    float safe=0.0f,blocked=1.0f;
    for(int iteration=0;iteration<12;++iteration){const float t=(safe+blocked)*0.5f;const float x=previousX+(desiredX-previousX)*t,z=previousZ+(desiredZ-previousZ)*t;if(blockedAt(x,z))blocked=t;else safe=t;}
    result.x=previousX+(desiredX-previousX)*safe;result.z=previousZ+(desiredZ-previousZ)*safe;
    const auto contact=sample(mesh,desiredX,desiredZ,radius,0.0f);
    Vec3 horizontal{contact.normal.x,0.0f,contact.normal.z};
    if(lengthSq(horizontal)>0.000001f)result.normal=normalized(horizontal);
    return result;
}

template<class Mesh>
bool obstructsBody(const Mesh& mesh,float x,float z,float bottom,float top,float radius,float walkableNormalY,Vec3* contactNormal=nullptr){
    const auto point=[&](int index){return Vec3{mesh.positions[index*3],mesh.positions[index*3+1],mesh.positions[index*3+2]};};
    Vec3 center{};for(int i=0;i<mesh.vertexCount;++i)center+=point(i);if(mesh.vertexCount>0)center*=1.0f/static_cast<float>(mesh.vertexCount);
    bool hit=false;float nearest=1.0e30f;
    for(int vertex=0;vertex+2<mesh.vertexCount;vertex+=3){
        const Vec3 a=point(vertex),b=point(vertex+1),c=point(vertex+2),normal=faceNormal(a,b,c);if(normal.y>=walkableNormalY)continue;
        const float low=std::min(a.y,std::min(b.y,c.y)),high=std::max(a.y,std::max(b.y,c.y));if(top<=low+0.001f||bottom>=high-0.001f||!projectedTriangleOverlapsCircle(a,b,c,x,z,radius))continue;
        hit=true;if(!contactNormal)continue;const Vec3 faceCenter=(a+b+c)*(1.0f/3.0f);const float dx=faceCenter.x-x,dz=faceCenter.z-z,distance=dx*dx+dz*dz;
        if(distance<nearest){Vec3 outward=normal;if(outward.x*(faceCenter.x-center.x)+outward.z*(faceCenter.z-center.z)<0.0f)outward*=-1.0f;*contactNormal=normalized(Vec3{outward.x,0,outward.z});nearest=distance;}
    }
    return hit;
}

template<class Mesh>
HorizontalResolution resolveBoundedHorizontal(const Mesh& mesh,float previousX,float previousZ,float desiredX,float desiredZ,float bottom,float top,float radius,float walkableNormalY,bool departingSupport){
    HorizontalResolution result{desiredX,desiredZ,{},false};if(departingSupport||!obstructsBody(mesh,desiredX,desiredZ,bottom,top,radius,walkableNormalY,&result.normal))return result;
    result.blocked=true;const auto blockedAt=[&](float x,float z){return obstructsBody(mesh,x,z,bottom,top,radius,walkableNormalY);};if(blockedAt(previousX,previousZ)){result.x=previousX;result.z=previousZ;return result;}
    float safe=0.0f,blocked=1.0f;for(int iteration=0;iteration<12;++iteration){const float t=(safe+blocked)*0.5f,x=previousX+(desiredX-previousX)*t,z=previousZ+(desiredZ-previousZ)*t;if(blockedAt(x,z))blocked=t;else safe=t;}result.x=previousX+(desiredX-previousX)*safe;result.z=previousZ+(desiredZ-previousZ)*safe;return result;
}

} // namespace surface_geometry
