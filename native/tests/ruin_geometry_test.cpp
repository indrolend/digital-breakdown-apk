#include "RuinGeometry.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

int main(){
    using namespace early_browser_visuals;
    const EnvironmentPropSpec ruin{EnvironmentPrimitive::Ruin,EnvironmentRole::Mass,{-6,0,7},{3.2f,2.0f,3.0f},0.0f,0};
    const auto first=ruin_geometry::parts(ruin),second=ruin_geometry::parts(ruin);
    for(int i=0;i<ruin_geometry::PartCount;++i){
        assert(first[i].center.x==second[i].center.x&&first[i].center.y==second[i].center.y&&first[i].center.z==second[i].center.z);
        assert(first[i].size.x==second[i].size.x&&first[i].size.y==second[i].size.y&&first[i].size.z==second[i].size.z);
        const auto bounds=ruin_geometry::bounds(first[i]);
        assert(std::isfinite(bounds.minX)&&std::isfinite(bounds.maxX)&&std::isfinite(bounds.minZ)&&std::isfinite(bounds.maxZ)&&std::isfinite(bounds.bottomY)&&std::isfinite(bounds.topY));
        assert(bounds.minX<bounds.maxX&&bounds.minZ<bounds.maxZ&&bounds.bottomY<bounds.topY);
    }
    const auto body=ruin_geometry::bounds(first[0]),remnant=ruin_geometry::bounds(first[1]);
    assert(std::abs(body.topY-1.52f)<0.0001f&&std::abs(remnant.topY-2.0f)<0.0001f);
    assert(remnant.minX>body.minX&&remnant.maxX<body.maxX&&remnant.minZ>body.minZ&&remnant.maxZ<body.maxZ);
    assert(first[0].surface==0&&first[1].surface==1);
    std::puts("Ruin geometry tests passed.");
    return 0;
}
