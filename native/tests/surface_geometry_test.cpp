#include "SurfaceGeometry.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

struct TestMesh { std::array<float,27> positions{};int vertexCount=0; };

static void emit(TestMesh& mesh,const Vec3& a,const Vec3& b,const Vec3& c){
    for(const Vec3& p:{a,b,c}){mesh.positions[mesh.vertexCount*3]=p.x;mesh.positions[mesh.vertexCount*3+1]=p.y;mesh.positions[mesh.vertexCount*3+2]=p.z;++mesh.vertexCount;}
}

int main(){
    TestMesh mesh{};
    emit(mesh,{-1,0,-1},{-1,1,1},{1,1,1});
    emit(mesh,{-1,0,-1},{1,1,1},{1,0,-1});
    const auto middle=surface_geometry::sample(mesh,0,0,0,0.819152f);
    assert(middle.inside&&std::abs(middle.height-0.5f)<0.0001f&&middle.normal.y>0.89f);
    assert(!surface_geometry::sample(mesh,1.1f,0,0,0.819152f).inside);
    assert(surface_geometry::sample(mesh,1.1f,0,0.11f,0.819152f).inside);

    TestMesh steep{};
    emit(steep,{-0.1f,0,-1},{-0.1f,2,1},{0.1f,2,1});
    assert(!surface_geometry::sample(steep,0,0,0.1f,0.819152f).inside);
    const auto envelope=surface_geometry::sample(steep,0,0,0.1f,0.0f);
    assert(envelope.inside&&std::isfinite(envelope.height));
    const auto free=surface_geometry::resolveHorizontal(steep,-1,0,-0.5f,0,0.5f,0.1f);
    assert(!free.blocked&&std::abs(free.x+0.5f)<0.0001f);
    const auto stopped=surface_geometry::resolveHorizontal(steep,-1,0,0,0,0.5f,0.1f);
    assert(stopped.blocked&&stopped.x<-0.19f&&stopped.x>-1.0f&&std::isfinite(stopped.normal.x));
    std::puts("SURFACE_GEOMETRY_OK bounded walkable footprint steep-rejection envelope");
    return 0;
}
