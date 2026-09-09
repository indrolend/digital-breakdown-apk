#include "Game.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace { bool near(float a,float b){return std::fabs(a-b)<0.0002f;} }

int main(){
    using namespace early_browser_visuals;
    int seed=-1,room=-1,houseIndex=-1;
    for(int s=1;s<512&&seed<0;++s)for(int r=1;r<32&&seed<0;++r){const auto plan=roomPlan(s,r);if(plan.setting!=RoomSetting::Field)continue;for(int i=0;i<environmentPropCount(plan);++i)if(environmentProp(plan,s,r,i).primitive==EnvironmentPrimitive::House){seed=s;room=r;houseIndex=i;break;}}
    assert(seed>=0);
    const auto plan=roomPlan(seed,room);const auto prop=environmentProp(plan,seed,room,houseIndex);
    const auto parts=house_geometry::parts(prop),repeat=house_geometry::parts(prop);
    assert(environmentPropColliderCount(plan.setting,prop)==house_geometry::PhysicalPartCount);
    for(int i=0;i<house_geometry::PartCount;++i){assert(parts[i].center.x==repeat[i].center.x&&parts[i].size.y==repeat[i].size.y);assert(std::isfinite(parts[i].center.x)&&parts[i].size.x>0&&parts[i].size.y>0&&parts[i].size.z>0);}
    assert(!parts[3].physical);

    Game game;game.debugStartGeneratedRoomFixture(seed,room);
    const float groundCenter=game.debugPlayerSupportAt(0,0).height;
    const Vec3 localX{std::cos(prop.yaw),0,-std::sin(prop.yaw)};
    const Vec3 bodyPoint=prop.center+localX*(prop.size.x*0.47f);
    const Vec3 lowerRoofPoint=prop.center+localX*(prop.size.x*0.40f);
    const auto bodySupport=game.debugPlayerSupportAt(bodyPoint.x,bodyPoint.z);
    const auto lowerRoofSupport=game.debugPlayerSupportAt(lowerRoofPoint.x,lowerRoofPoint.z);
    const auto upperRoofSupport=game.debugPlayerSupportAt(prop.center.x,prop.center.z);
    const float bodyTop=parts[0].center.y+parts[0].size.y*0.5f+groundCenter;
    const float lowerRoofTop=parts[1].center.y+parts[1].size.y*0.5f+groundCenter;
    const float upperRoofTop=parts[2].center.y+parts[2].size.y*0.5f+groundCenter;
    assert(near(bodySupport.height,bodyTop));
    assert(near(lowerRoofSupport.height,lowerRoofTop));
    assert(near(upperRoofSupport.height,upperRoofTop));
    assert(bodySupport.height<lowerRoofSupport.height&&lowerRoofSupport.height<upperRoofSupport.height);
    const auto geometry=roomGeometryCapacityPlan(plan,seed,room,ROOM_COLLIDER_COUNT);
    assert(geometry.totalColliderCount<=ROOM_COLLIDER_COUNT);
    std::puts("HOUSE_GEOMETRY_OK deterministic-parts body-support stepped-roof-support decorative-door bounded-capacity");
    return 0;
}
