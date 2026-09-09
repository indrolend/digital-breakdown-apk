#include "Game.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

int main(){
    using namespace early_browser_visuals;
    int seed=-1,room=-1,treeIndex=-1;
    for(int s=1;s<512&&seed<0;++s)for(int r=1;r<32&&seed<0;++r){const auto plan=roomPlan(s,r);for(int i=0;i<environmentPropCount(plan);++i)if(environmentProp(plan,s,r,i).primitive==EnvironmentPrimitive::Tree){seed=s;room=r;treeIndex=i;break;}}
    assert(seed>=0);
    const auto plan=roomPlan(seed,room),repeatPlan=roomPlan(seed,room);const auto prop=environmentProp(plan,seed,room,treeIndex);
    const auto trunks=tree_geometry::trunkParts(prop),repeatTrunks=tree_geometry::trunkParts(prop);
    const auto crowns=tree_geometry::crownParts(prop),repeatCrowns=tree_geometry::crownParts(prop);
    assert(trunks.size()==tree_geometry::TrunkPartCount&&crowns.size()==tree_geometry::CrownPartCount);
    for(int i=0;i<tree_geometry::TrunkPartCount;++i){assert(trunks[i].center.x==repeatTrunks[i].center.x&&trunks[i].size.y==repeatTrunks[i].size.y);assert(std::isfinite(trunks[i].center.y)&&trunks[i].size.x>0&&trunks[i].size.y>0&&trunks[i].size.z>0);}
    for(int i=0;i<tree_geometry::CrownPartCount;++i){assert(crowns[i].center.x==repeatCrowns[i].center.x&&crowns[i].size.y==repeatCrowns[i].size.y);assert(std::isfinite(crowns[i].center.y)&&crowns[i].size.x>0&&crowns[i].size.y>0&&crowns[i].size.z>0);}
    assert(plan.setting==repeatPlan.setting);
    assert(std::abs(tree_geometry::climbTopY(prop)-crowns.back().center.y)<0.0001f);

    Game game;game.debugStartGeneratedRoomFixture(seed,room);bool found=false;
    for(int i=0;i<game.state().debug.colliderCount;++i){const auto& collider=game.state().roomColliders[i];if(collider.kind!=RoomColliderKind::TreeTrunk)continue;if(std::abs(collider.center.x-prop.center.x)>0.05f||std::abs(collider.center.z-prop.center.z)>0.05f)continue;assert(std::abs(collider.climbTopY-(tree_geometry::climbTopY(prop)+0.08f))<0.0002f);found=true;break;}
    assert(found);
    std::puts("TREE_GEOMETRY_OK shared-trunk shared-crowns deterministic climb-top-authority production-tree");
    return 0;
}
