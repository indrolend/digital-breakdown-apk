#include "RenderContracts.hpp"
#include <cstdio>

int main(){
    using namespace render_contract;
    constexpr auto glass=sceneMatte(Pass7Visual::TvMembrane,0.25f);
    static_assert(glass.opacity==0.25f&&glass.fog&&glass.shading==ShadingModel::ColorGraded);
    constexpr auto fx=unlit(Pass7Visual::ElectricCyan,0.5f);
    static_assert(!fx.fog&&fx.shading==ShadingModel::Unlit);
    static_assert(shadowQualityFor(0,false,true)==ShadowQuality::Off);
    static_assert(shadowQualityFor(1,true,true)==ShadowQuality::Cheap);
    static_assert(shadowQualityFor(2,true,true)==ShadowQuality::Directional);
    static_assert(shadowQualityFor(2,true,false)==ShadowQuality::Cheap);
    static_assert(FieldOpenGround.texture==TextureId::FieldGrass&&FieldOpenGround.textureWorldScale==2.4f);
    static_assert(CityGround.texture==TextureId::CityAsphalt&&CityGround.textureWorldScale==3.2f);
    using early_browser_visuals::RoomForm;using early_browser_visuals::RoomSetting;
    const auto opening=roomLightingProfile(RoomSetting::Field,RoomForm::Open,12345,1,0.0f,0.0f);
    const auto deepRoom=roomLightingProfile(RoomSetting::Field,RoomForm::Open,12345,19,0.0f,1.0f);
    if(!(deepRoom.fogDensity>opening.fogDensity)){
        std::fputs("RENDER_CONTRACTS_FAIL atmosphere progression\n",stderr);return 1;
    }
    if(opening.phone.r!=0.0f||deepRoom.phone.b!=1.32f){
        std::fputs("RENDER_CONTRACTS_FAIL phone light response\n",stderr);return 1;
    }
    const auto sterile=roomLightingProfile(RoomSetting::Sterile,RoomForm::Corridor,12345,4,0.0f,0.0f);
    const auto chamber=roomLightingProfile(RoomSetting::Sterile,RoomForm::Chamber,12345,4,0.0f,0.0f);
    const auto city=roomLightingProfile(RoomSetting::City,RoomForm::Corridor,12345,4,0.0f,0.0f);
    const auto coastal=roomLightingProfile(RoomSetting::Coastal,RoomForm::Shore,12345,4,0.0f,0.0f);
    const auto alternateField=roomLightingProfile(RoomSetting::Field,RoomForm::Open,54321,1,0.0f,0.0f);
    if(!(opening.primarySource==PrimaryLightSource::OutdoorSun&&coastal.primarySource==PrimaryLightSource::OutdoorSun&&city.primarySource==PrimaryLightSource::UrbanSky&&sterile.primarySource==PrimaryLightSource::CeilingFixtures)){
        std::fputs("RENDER_CONTRACTS_FAIL source ownership\n",stderr);return 1;
    }
    if(!(opening.ambient.r>sterile.ambient.r&&opening.primary.r>sterile.primary.r&&sterile.fogDensity>opening.fogDensity&&city.primary.r>city.primary.b&&coastal.fill.b>coastal.fill.r&&alternateField.ambient.r!=opening.ambient.r&&opening.skyHorizon.r>opening.skyTop.r)){
        std::fputs("RENDER_CONTRACTS_FAIL room lighting identities\n",stderr);return 1;
    }
    if(sterile.localLightCount!=2||chamber.localLightCount!=3||sterile.primaryDirection.y<=0.0f){std::fputs("RENDER_CONTRACTS_FAIL fixture layout or contact-shadow direction\n",stderr);return 1;}
    if(!(sterile.contactOcclusion>city.contactOcclusion&&city.contactOcclusion>opening.contactOcclusion&&opening.cornerOcclusion>0.0f)){std::fputs("RENDER_CONTRACTS_FAIL room occlusion identity\n",stderr);return 1;}
    for(int i=0;i<sterile.localLightCount;++i)if(!sterile.localLights[i].visibleFixture||sterile.localLights[i].fixtureSize.x<=0.0f||sterile.localLights[i].radius<=0.0f){std::fputs("RENDER_CONTRACTS_FAIL visible fixture pairing\n",stderr);return 1;}
    const auto repeat=roomLightingProfile(RoomSetting::Field,RoomForm::Open,12345,1,0.0f,0.0f);
    if(repeat.ambient.r!=opening.ambient.r||repeat.primaryDirection.x!=opening.primaryDirection.x||repeat.skyHorizon.g!=opening.skyHorizon.g){std::fputs("RENDER_CONTRACTS_FAIL deterministic lighting\n",stderr);return 1;}
    SceneResponseInputs calmInput{};
    calmInput.batteryFraction=1.0f;
    calmInput.latestShotAge=9999.0f;
    calmInput.grounded=true;
    const auto calm=sceneResponse(calmInput);
    SceneResponseInputs activeInput{};
    activeInput.horizontalSpeed=8.0f;activeInput.batteryFraction=0.1f;activeInput.vacuumPower=1.0f;activeInput.discharge=1.0f;
    activeInput.latestShotAge=0.0f;activeInput.criticalPulse=1.0f;activeInput.goalProgress=1.0f;activeInput.grounded=false;activeInput.roomClear=true;
    const auto active=sceneResponse(activeInput);
    if(!(active.movement>calm.movement&&active.shotLight>calm.shotLight&&active.actionLight>calm.actionLight&&active.criticalLight>calm.criticalLight&&active.phoneLight<calm.phoneLight&&active.exitGlow>calm.exitGlow&&active.contactShadowScale<calm.contactShadowScale&&active.wind>calm.wind)){std::fputs("RENDER_CONTRACTS_FAIL scene response\n",stderr);return 1;}
    const auto completedField=roomLightingProfile(RoomSetting::Field,RoomForm::Open,12345,1,0.0f,0.0f,1.0f);
    const auto completedSterile=roomLightingProfile(RoomSetting::Sterile,RoomForm::Corridor,12345,2,0.0f,0.0f,1.0f);
    if(!(active.goalProgress==1.0f&&completedField.skyHorizon.r>opening.skyHorizon.r&&completedField.fogDensity<opening.fogDensity&&completedSterile.localLights[0].intensity>sterile.localLights[0].intensity)){std::fputs("RENDER_CONTRACTS_FAIL physical goal response\n",stderr);return 1;}
    std::puts("RENDER_CONTRACTS_OK profiles=4 source-owned sky=GRADIENT fixtures=PAIRED shadows=PROFILE field_grass=LIT city_ground=TEXTURED");
    return 0;
}
