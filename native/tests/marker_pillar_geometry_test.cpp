#include "Game.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace { bool near(float a,float b){return std::fabs(a-b)<0.0002f;} }

int main(){
    using namespace early_browser_visuals;
    int seed=-1,room=-1,markerIndex=-1;
    for(int s=1;s<512&&seed<0;++s)for(int r=1;r<32&&seed<0;++r){const auto plan=roomPlan(s,r);if(plan.setting!=RoomSetting::Sterile)continue;for(int i=0;i<environmentPropCount(plan);++i)if(environmentProp(plan,s,r,i).primitive==EnvironmentPrimitive::MarkerPillar){seed=s;room=r;markerIndex=i;break;}}
    assert(seed>=0);
    const auto plan=roomPlan(seed,room);const auto prop=environmentProp(plan,seed,room,markerIndex);
    const auto parts=marker_pillar_geometry::parts(prop),repeat=marker_pillar_geometry::parts(prop);
    assert(environmentPropColliderCount(plan.setting,prop)==marker_pillar_geometry::PartCount);
    for(int i=0;i<marker_pillar_geometry::PartCount;++i){assert(parts[i].center.x==repeat[i].center.x&&parts[i].center.y==repeat[i].center.y&&parts[i].size.x==repeat[i].size.x);assert(std::isfinite(parts[i].center.y)&&parts[i].size.x>0&&parts[i].size.y>0&&parts[i].size.z>0);}
    assert(parts[1].size.x>parts[0].size.x&&parts[1].size.z>parts[0].size.z);

    Game game;game.debugStartGeneratedRoomFixture(seed,room);
    const float groundHeight=game.debugPlayerSupportAt(0.0f,0.0f).height;
    const float capTop=parts[1].center.y+parts[1].size.y*0.5f+groundHeight;
    const auto support=game.debugPlayerSupportAt(prop.center.x,prop.center.z);
    assert(near(support.height,capTop));
    assert(support.height>prop.size.y+groundHeight);
    const auto geometry=roomGeometryCapacityPlan(plan,seed,room,ROOM_COLLIDER_COUNT);
    assert(geometry.totalColliderCount<=ROOM_COLLIDER_COUNT);
    assert(geometry.identityColliderCount>=marker_pillar_geometry::PartCount);
    std::puts("MARKER_PILLAR_GEOMETRY_OK deterministic-parts exact-shaft exact-cap-support no-visual-cap-clipping bounded-capacity");
    return 0;
}
