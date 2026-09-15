#include "RenderContracts.hpp"
#include <cstdio>

int main(){
    using namespace render_contract;
    if(DesktopSceneLighting.sun.direction.x!=30.0f||DesktopSceneLighting.fog.density!=0.018f){
        std::fputs("RENDER_CONTRACTS_FAIL desktop profile\n",stderr);return 1;
    }
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
    const auto opening=sceneAtmosphere(0.0f,1,12345,0.0f,early_browser_visuals::RoomSetting::Field);
    const auto deepRoom=sceneAtmosphere(0.0f,19,12345,1.0f,early_browser_visuals::RoomSetting::Field);
    if(!(deepRoom.fogDensity>opening.fogDensity)){
        std::fputs("RENDER_CONTRACTS_FAIL atmosphere progression\n",stderr);return 1;
    }
    if(opening.phone.r!=0.0f||deepRoom.phone.b!=1.32f){
        std::fputs("RENDER_CONTRACTS_FAIL phone light response\n",stderr);return 1;
    }
    const auto sterile=sceneAtmosphere(0.0f,4,12345,0.0f,early_browser_visuals::RoomSetting::Sterile);
    const auto city=sceneAtmosphere(0.0f,4,12345,0.0f,early_browser_visuals::RoomSetting::City);
    const auto coastal=sceneAtmosphere(0.0f,4,12345,0.0f,early_browser_visuals::RoomSetting::Coastal);
    const auto alternateField=sceneAtmosphere(0.0f,1,54321,0.0f,early_browser_visuals::RoomSetting::Field);
    if(!(opening.ambient.r>sterile.ambient.r&&opening.sun.r>sterile.sun.r&&sterile.fogDensity>opening.fogDensity&&city.sun.r>city.sun.b&&coastal.fill.b>coastal.fill.r&&alternateField.ambient.r!=opening.ambient.r)){
        std::fputs("RENDER_CONTRACTS_FAIL room lighting identities\n",stderr);return 1;
    }
    const auto repeat=sceneAtmosphere(0.0f,1,12345,0.0f,early_browser_visuals::RoomSetting::Field);
    if(repeat.ambient.r!=opening.ambient.r||repeat.sun.g!=opening.sun.g){std::fputs("RENDER_CONTRACTS_FAIL deterministic lighting\n",stderr);return 1;}
    std::puts("RENDER_CONTRACTS_OK profiles=4 deterministic-room-variation atmosphere=PROGRESSIVE field_grass=TEXTURED city_ground=TEXTURED");
    return 0;
}
