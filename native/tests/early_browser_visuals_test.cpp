#include "EarlyBrowserVisuals.hpp"
#include <cassert>
#include <cmath>

int main(){
    using namespace early_browser_visuals;
    const auto first=roomPlan(12345,1);
    assert(first.premise==RoomPremise::Field&&first.grass&&!first.sidewalks);
    assert(first.obstacleCount==3&&!first.recovery);

    bool sawField=false,sawCity=false,sawSterile=false,sawRecovery=false;
    for(int seed=1;seed<=128;++seed) for(int room=1;room<=32;++room){
        const auto a=roomPlan(seed,room),b=roomPlan(seed,room);
        assert(a.premise==b.premise&&a.obstacleCount==b.obstacleCount&&a.recovery==b.recovery);
        sawField|=a.premise==RoomPremise::Field;sawCity|=a.premise==RoomPremise::City;sawSterile|=a.premise==RoomPremise::Sterile;sawRecovery|=a.recovery;
        if(a.premise==RoomPremise::Field){assert(a.grass&&!a.sidewalks);assert(a.obstacleCount==1||a.obstacleCount==3);}
        if(a.premise==RoomPremise::City){assert(!a.grass&&a.sidewalks&&a.obstacleCount==10);}
        if(a.premise==RoomPremise::Sterile){assert(!a.grass&&!a.sidewalks&&a.obstacleCount==6);}
        if(a.recovery){assert(a.premise==RoomPremise::Field&&a.enemyAdjustment==-2&&a.obstacleCount==1);}
        for(int i=0;i<a.obstacleCount;++i){const auto obstacleA=obstacle(a,seed,room,i),obstacleB=obstacle(a,seed,room,i);assert(obstacleA.center.x==obstacleB.center.x&&obstacleA.size.y==obstacleB.size.y);assert(std::abs(obstacleA.center.x)>3.0f);}
    }
    assert(sawField&&sawCity&&sawSterile&&sawRecovery);

    const auto blade=grassBlade(12345,2,0,7),sameBlade=grassBlade(12345,2,0,7);
    assert(blade.root.x==sameBlade.root.x&&blade.height==sameBlade.height);
    GrassReactionInputs calmInput;calmInput.player={100,0,100};
    const Vec3 calm=grassTip(blade,0.5f,calmInput);
    GrassReactionInputs playerInput;playerInput.player=blade.root;
    const Vec3 displaced=grassTip(blade,0.5f,playerInput);
    GrassReactionInputs shotInput;shotInput.player={100,0,100};shotInput.shotOrigin=blade.root;shotInput.shotAge=0.08f;
    const Vec3 shot=grassTip(blade,0.5f,shotInput);
    GrassReactionInputs vacuumInput;vacuumInput.player={100,0,100};vacuumInput.vacuumOrigin=blade.root+Vec3{2,0,0};vacuumInput.vacuumStrength=1.0f;
    const Vec3 vacuum=grassTip(blade,0.5f,vacuumInput);
    assert(std::isfinite(calm.x)&&std::isfinite(displaced.x)&&std::isfinite(shot.x)&&std::isfinite(vacuum.x));
    assert(vacuum.x>calm.x);
}
