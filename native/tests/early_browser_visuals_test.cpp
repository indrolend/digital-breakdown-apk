#include "EarlyBrowserVisuals.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>

int main(){
    using namespace early_browser_visuals;
    const auto first=roomPlan(12345,1);
    assert(first.setting==RoomSetting::Field&&first.form==RoomForm::Open&&first.grass&&!first.sidewalks);
    assert(first.obstacleCount==3&&!first.recovery());
    assert(first.playstyle==RoomPlaystyle::Playground);
    assert(requiredRouteIsTraversable(first,12345,1));

    bool sawField=false,sawCity=false,sawSterile=false,sawCoastal=false,sawRecovery=false,sawCourtyard=false,sawCanyon=false,sawSkyline=false,sawChamber=false;
    bool sawCompactCourtyard=false,sawStandardCourtyard=false,sawLargeCourtyard=false;
    bool sawPlayground=false,sawFunnel=false,sawOrbit=false,sawVertical=false;
    for(int seed=1;seed<=128;++seed) for(int room=1;room<=32;++room){
        const auto a=roomPlan(seed,room),b=roomPlan(seed,room);
        assert(a.setting==b.setting&&a.form==b.form&&a.scale==b.scale&&a.condition==b.condition&&a.playstyle==b.playstyle&&a.obstacleCount==b.obstacleCount);
        assert(validFormForSetting(a.setting,a.form));
        assert(gameplay::validTraversalGraphTopology(a.traversal));
        assert(a.traversal.surfaceCount==b.traversal.surfaceCount&&a.traversal.edgeCount==b.traversal.edgeCount);
        for(int surface=0;surface<a.traversal.surfaceCount;++surface){const auto& x=a.traversal.surfaces[surface];const auto& y=b.traversal.surfaces[surface];assert(x.center.x==y.center.x&&x.center.y==y.center.y&&x.center.z==y.center.z&&x.halfSize.x==y.halfSize.x&&x.halfSize.y==y.halfSize.y&&x.halfSize.z==y.halfSize.z&&x.required==y.required);}
        for(int edge=0;edge<a.traversal.edgeCount;++edge){const auto& x=a.traversal.edges[edge];const auto& y=b.traversal.edges[edge];assert(x.from==y.from&&x.to==y.to&&x.action==y.action&&x.difficulty==y.difficulty&&x.role==y.role);}
        if(room>1){const auto previous=roomPlan(seed,room-1);if(!a.recovery()&&!previous.recovery())assert(a.playstyle!=previous.playstyle);}
        assert(requiredRouteIsTraversable(a,seed,room));
        int requiredEdges=0,optionalEdges=0,nonWalkOptionalEdges=0;
        for(int edge=0;edge<a.traversal.edgeCount;++edge){
            const auto& traversalEdge=a.traversal.edges[edge];
            if(gameplay::isRequired(traversalEdge)){++requiredEdges;assert(traversalEdge.action==gameplay::TraversalAction::Walk);assert(traversalEdge.difficulty==gameplay::TraversalDifficulty::Automatic);}
            else {++optionalEdges;if(traversalEdge.action!=gameplay::TraversalAction::Walk)++nonWalkOptionalEdges;}
        }
        assert(requiredEdges==3);
        if(a.recovery()){assert(a.playstyle==RoomPlaystyle::Recovery);assert(optionalEdges==0);}
        else {assert(optionalEdges>=2);assert(nonWalkOptionalEdges==optionalEdges);}
        sawPlayground|=a.playstyle==RoomPlaystyle::Playground;
        sawFunnel|=a.playstyle==RoomPlaystyle::Funnel;
        sawOrbit|=a.playstyle==RoomPlaystyle::Orbit;
        sawVertical|=a.playstyle==RoomPlaystyle::Vertical;
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
        int leftMasses=0,rightMasses=0;
        for(int i=0;i<a.obstacleCount;++i){const auto obstacleA=obstacle(a,seed,room,i),obstacleB=obstacle(a,seed,room,i);assert(obstacleA.center.x==obstacleB.center.x&&obstacleA.center.y==obstacleB.center.y&&obstacleA.center.z==obstacleB.center.z&&obstacleA.size.x==obstacleB.size.x&&obstacleA.size.y==obstacleB.size.y&&obstacleA.size.z==obstacleB.size.z);assert(std::abs(obstacleA.center.x)>3.0f);
            if(a.setting==RoomSetting::City&&a.form==RoomForm::Corridor){
                (obstacleA.center.x<0?leftMasses:rightMasses)++;
                assert(std::abs(obstacleA.center.x)-obstacleA.size.x*0.5f>gameplay::WORLD_SCALE.narrowPassageHalfWidth+3.0f);
                const float stories=obstacleA.size.y/gameplay::WORLD_SCALE.storyHeight;
                assert(stories>=2.0f&&stories<=5.0f&&std::abs(stories-std::round(stories))<0.0001f);
            }
        }
        if(a.setting==RoomSetting::City&&a.form==RoomForm::Corridor){assert(leftMasses==5&&rightMasses==5);assert(environmentRoleCount(a,seed,room,EnvironmentRole::Landmark)==1);assert(environmentRoleCount(a,seed,room,EnvironmentRole::Mass)==9);}
        const int propCount=environmentPropCount(a);assert(propCount>=0&&propCount<=6);
        if(a.setting==RoomSetting::City&&a.form==RoomForm::Corridor)assert(propCount==0);
        const bool propsValid=environmentPropsValid(a,seed,room);
        int solidProps=0;for(int i=0;i<propCount;++i)solidProps+=environmentPropSolid(environmentProp(a,seed,room,i))?1:0;
        assert(propsValid==(a.obstacleCount+solidProps+physicalTraversalSurfaceCount(a)<=15));
        int landmarks=0;
        for(int i=0;i<propCount;++i){const auto propA=environmentProp(a,seed,room,i),propB=environmentProp(a,seed,room,i);assert(propA.primitive==propB.primitive&&propA.role==propB.role&&propA.center.x==propB.center.x&&propA.size.y==propB.size.y);assert(settingAllowsPrimitive(a.setting,propA.primitive));assert(std::abs(propA.center.x)>7.0f);landmarks+=propA.role==EnvironmentRole::Landmark?1:0;}
        assert(landmarks<=1);
    }
    assert(sawField&&sawCity&&sawSterile&&sawCoastal&&sawRecovery&&sawCourtyard&&sawCanyon&&sawSkyline&&sawChamber);
    assert(sawCompactCourtyard&&sawStandardCourtyard&&sawLargeCourtyard);
    assert(sawPlayground&&sawFunnel&&sawOrbit&&sawVertical);
    const auto cityTraversal=traversalPresentationFor(RoomSetting::City,false);
    const auto sterileTraversal=traversalPresentationFor(RoomSetting::Sterile,false);
    const auto debugTraversal=traversalPresentationFor(RoomSetting::City,true);
    assert(!cityTraversal.debug&&!sterileTraversal.debug&&debugTraversal.debug);
    assert(cityTraversal.color.x!=debugTraversal.color.x&&sterileTraversal.color.x!=cityTraversal.color.x);

    RoomEnvironmentPlan full;
    full.playstyle=RoomPlaystyle::Orbit;
    full.traversal.surfaceCount=gameplay::TraversalGraph::SurfaceCapacity-1;
    full.traversal.edgeCount=gameplay::TraversalGraph::EdgeCapacity-2;
    appendOptionalTraversal(full,roomKey(7,9));
    assert(full.traversal.surfaceCount==gameplay::TraversalGraph::SurfaceCapacity-1);
    assert(full.traversal.edgeCount==gameplay::TraversalGraph::EdgeCapacity-2);

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
    std::printf("ROOM_GRAMMAR_SCALE_ROLES_OK seeds=4096 city_corridor=SIDE_BANDS story_bands=2-5 landmark=1 props=0 traversal_presentation=SETTING_DEBUG_CYAN\n");
}
