#include "FacetedRock.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

int main(){
    using namespace early_browser_visuals;
    const EnvironmentPropSpec mass{EnvironmentPrimitive::Rock,EnvironmentRole::Mass,{8.0f,0.0f,-3.0f},{2.1f,1.2f,1.7f},0.37f,0};
    const auto a=faceted_rock::makeMesh(mass,91,7,2);
    const auto b=faceted_rock::makeMesh(mass,91,7,2);
    assert(a.vertexCount==faceted_rock::VertexCount&&b.vertexCount==a.vertexCount);
    assert(a.positions==b.positions&&a.normals==b.normals);
    bool grounded=false,upwardFace=false;
    for(int i=0;i<a.vertexCount;++i){
        const float x=a.positions[i*3],y=a.positions[i*3+1],z=a.positions[i*3+2];
        assert(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));
        assert(x>=mass.center.x-mass.size.x*0.5f&&x<=mass.center.x+mass.size.x*0.5f);
        assert(z>=mass.center.z-mass.size.z*0.5f&&z<=mass.center.z+mass.size.z*0.5f);
        assert(y>=mass.center.y&&y<=mass.center.y+mass.size.y);
        grounded|=std::fabs(y-mass.center.y)<0.0001f;
        upwardFace|=a.normals[i*3+1]>0.5f;
    }
    assert(grounded&&upwardFace);
    assert(faceted_rock::eligible(RoomSetting::Coastal,EnvironmentRole::Mass));
    assert(faceted_rock::eligible(RoomSetting::Field,EnvironmentRole::Landmark));
    assert(!faceted_rock::eligible(RoomSetting::City,EnvironmentRole::Mass));
    assert(!faceted_rock::eligible(RoomSetting::Sterile,EnvironmentRole::Landmark));
    assert(!faceted_rock::eligible(RoomSetting::Field,EnvironmentRole::Traversal));
    assert(faceted_rock::shapeKey(91,7,2,EnvironmentRole::Mass)!=faceted_rock::shapeKey(91,7,2,EnvironmentRole::Landmark));
    const faceted_rock::Support support{mass,91,7,2};
    const auto supportAgain=faceted_rock::sampleSupport(support,8.0f,-3.0f);
    const auto supportFirst=faceted_rock::sampleSupport(support,8.0f,-3.0f);
    assert(supportFirst.inside&&supportAgain.inside);
    assert(supportFirst.height==supportAgain.height&&supportFirst.normal.x==supportAgain.normal.x&&supportFirst.normal.y==supportAgain.normal.y&&supportFirst.normal.z==supportAgain.normal.z);
    assert(supportFirst.normal.y>=faceted_rock::WalkableNormalY&&std::isfinite(supportFirst.height));
    assert(!faceted_rock::sampleSupport(support,20.0f,-3.0f).inside);
    bool sawRejectedSteepFace=false;
    for(int vertex=0;vertex+2<a.vertexCount;vertex+=3){const Vec3 n{a.normals[vertex*3],a.normals[vertex*3+1],a.normals[vertex*3+2]};sawRejectedSteepFace|=n.y>0.0f&&!faceted_rock::triangleWalkable(n);}
    assert(sawRejectedSteepFace);
    std::puts("Faceted rock tests passed.");
    return 0;
}
