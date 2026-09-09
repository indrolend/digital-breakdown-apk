#include "Game.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
bool near(float a,float b){return std::fabs(a-b)<0.0001f;}
}

int main(){
    using namespace early_browser_visuals;
    int seed=-1,room=-1,ruinIndex=-1;
    for(int candidateSeed=1;candidateSeed<512&&seed<0;++candidateSeed)for(int candidateRoom=1;candidateRoom<32&&seed<0;++candidateRoom){
        const auto plan=roomPlan(candidateSeed,candidateRoom);
        for(int i=0;i<environmentPropCount(plan);++i){
            if(environmentProp(plan,candidateSeed,candidateRoom,i).primitive==EnvironmentPrimitive::Ruin){seed=candidateSeed;room=candidateRoom;ruinIndex=i;break;}
        }
    }
    assert(seed>=0);
    const auto plan=roomPlan(seed,room);
    const auto prop=environmentProp(plan,seed,room,ruinIndex);
    const auto first=ruin_geometry::parts(prop),second=ruin_geometry::parts(prop);
    assert(environmentPropColliderCount(prop)==ruin_geometry::PartCount);
    for(int i=0;i<ruin_geometry::PartCount;++i){
        assert(first[i].center.x==second[i].center.x&&first[i].center.y==second[i].center.y&&first[i].center.z==second[i].center.z);
        assert(first[i].size.x>0&&first[i].size.y>0&&first[i].size.z>0);
    }

    Game game;game.debugStartGeneratedRoomFixture(seed,room);
    const auto body=ruin_geometry::collider(first[0]),remnant=ruin_geometry::collider(first[1]);
    const float bodyTop=body.center.y+body.size.y*0.5f;
    const float remnantTop=remnant.center.y+remnant.size.y*0.5f;
    const float bodyOnlyX=body.center.x-body.size.x*0.40f;
    const auto bodySupport=game.debugPlayerSupportAt(bodyOnlyX,body.center.z);
    const auto remnantSupport=game.debugPlayerSupportAt(remnant.center.x,remnant.center.z);
    assert(near(bodySupport.height,bodyTop+GROUND_Y));
    assert(near(remnantSupport.height,remnantTop+GROUND_Y));
    assert(bodySupport.height<prop.size.y+GROUND_Y);
    assert(remnantSupport.height>bodySupport.height);

    const auto geometry=roomGeometryCapacityPlan(plan,seed,room,ROOM_COLLIDER_COUNT);
    assert(geometry.totalColliderCount<=ROOM_COLLIDER_COUNT);
    assert(geometry.identityColliderCount>=ruin_geometry::PartCount);
    std::puts("RUIN_GEOMETRY_OK deterministic-parts exact-body-support exact-remnant-support no-invisible-envelope-top bounded-capacity");
    return 0;
}
