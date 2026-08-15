#include "EarlyBrowserVisuals.hpp"
#include <cassert>
#include <cmath>

int main(){
    using namespace early_browser_visuals;
    const auto first=roomPlan(12345,1);
    assert(first.setting==RoomSetting::Field&&first.form==RoomForm::Open&&first.grass&&!first.sidewalks);
    assert(first.obstacleCount==3&&!first.recovery());
    assert(requiredRouteIsTraversable(first,12345,1));

    bool sawField=false,sawCity=false,sawSterile=false,sawCoastal=false,sawRecovery=false,sawCourtyard=false,sawCanyon=false,sawSkyline=false,sawChamber=false;
    bool sawCompactCourtyard=false,sawStandardCourtyard=false,sawLargeCourtyard=false;
    for(int seed=1;seed<=128;++seed) for(int room=1;room<=32;++room){
        const auto a=roomPlan(seed,room),b=roomPlan(seed,room);
        assert(a.setting==b.setting&&a.form==b.form&&a.scale==b.scale&&a.condition==b.condition&&a.obstacleCount==b.obstacleCount);
        assert(gameplay::validTraversalGraphTopology(a.traversal));
        assert(a.traversal.surfaceCount==b.traversal.surfaceCount&&a.traversal.edgeCount==b.traversal.edgeCount);
        assert(requiredRouteIsTraversable(a,seed,room));
        sawField|=a.setting==RoomSetting::Field;sawCity|=a.setting==RoomSetting::City;sawSterile|=a.setting==RoomSetting::Sterile;sawCoastal|=a.setting==RoomSetting::Coastal;sawRecovery|=a.recovery();
        if(a.setting==RoomSetting::Field){assert(a.form==RoomForm::Open&&a.grass&&!a.sidewalks);assert(a.obstacleCount==1||a.obstacleCount==3);}
        if(a.setting==RoomSetting::City){assert(!a.grass&&a.sidewalks&&a.obstacleCount==10);}
        if(a.setting==RoomSetting::Sterile){assert((a.form==RoomForm::Corridor||a.form==RoomForm::Chamber)&&!a.grass&&!a.sidewalks&&a.obstacleCount==6);}
        if(a.setting==RoomSetting::Coastal){assert(a.form==RoomForm::Shore&&!a.grass&&!a.sidewalks&&a.obstacleCount==5);}
        if(a.recovery()){assert(a.setting==RoomSetting::Field&&a.enemyAdjustment==-2&&a.obstacleCount==1);}
        if(a.form==RoomForm::Courtyard){
            assert(a.setting==RoomSetting::City);sawCourtyard=true;
            sawCompactCourtyard|=a.scale==RoomScale::Compact;
            sawStandardCourtyard|=a.scale==RoomScale::Standard;
            sawLargeCourtyard|=a.scale==RoomScale::Large;
        }
        if(a.form==RoomForm::Canyon){assert(a.setting==RoomSetting::City);sawCanyon=true;}
        if(a.form==RoomForm::Skyline){assert(a.setting==RoomSetting::City);sawSkyline=true;}
        if(a.form==RoomForm::Chamber){assert(a.setting==RoomSetting::Sterile);sawChamber=true;}
        for(int i=0;i<a.obstacleCount;++i){const auto obstacleA=obstacle(a,seed,room,i),obstacleB=obstacle(a,seed,room,i);assert(obstacleA.center.x==obstacleB.center.x&&obstacleA.size.y==obstacleB.size.y);assert(std::abs(obstacleA.center.x)>3.0f);}
        const int propCount=environmentPropCount(a);assert(propCount>=3&&propCount<=6);
        if(!environmentPropsValid(a,seed,room))return 2;
        for(int i=0;i<propCount;++i){const auto propA=environmentProp(a,seed,room,i),propB=environmentProp(a,seed,room,i);assert(propA.primitive==propB.primitive&&propA.center.x==propB.center.x&&propA.size.y==propB.size.y);assert(std::abs(propA.center.x)>7.0f);}
    }
    assert(sawField&&sawCity&&sawSterile&&sawCoastal&&sawRecovery&&sawCourtyard&&sawCanyon&&sawSkyline&&sawChamber);
    assert(sawCompactCourtyard&&sawStandardCourtyard&&sawLargeCourtyard);

    const auto capabilities=gameplay::TRAVERSAL_CAPABILITIES;
    assert(std::abs(capabilities.maximumGroundJumpHeight()-0.7232142f)<0.0001f);
    assert(std::abs(capabilities.maximumDoubleJumpAddedHeight()-0.6450892f)<0.0001f);
    assert(std::abs(capabilities.airLungeDistance-5.40f)<0.0001f);

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
    GrassBlade translatedBlade=blade;translatedBlade.root.z+=36.0f;
    GrassReactionInputs translatedInput=vacuumInput;translatedInput.player.z+=36.0f;translatedInput.vacuumOrigin.z+=36.0f;translatedInput.shotOrigin.z+=36.0f;
    const Vec3 translated=grassTip(translatedBlade,0.5f,translatedInput);
    assert(std::abs((translated.x-translatedBlade.root.x)-(vacuum.x-blade.root.x))<0.0001f);
}
