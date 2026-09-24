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
    const auto opening=sceneAtmosphere(0.0f,1,0.0f);
    const auto deepRoom=sceneAtmosphere(0.0f,19,1.0f);
    if(!(deepRoom.fogDensity>opening.fogDensity&&deepRoom.fog.r>opening.fog.r&&deepRoom.sun.b>opening.sun.b)){
        std::fputs("RENDER_CONTRACTS_FAIL atmosphere progression\n",stderr);return 1;
    }
    if(opening.phone.r!=0.0f||deepRoom.phone.b!=1.32f){
        std::fputs("RENDER_CONTRACTS_FAIL phone light response\n",stderr);return 1;
    }
    std::puts("RENDER_CONTRACTS_OK profiles=2 shading_models=3 shadow_qualities=3 atmosphere=PROGRESSIVE field_grass=TEXTURED city_ground=TEXTURED");
    return 0;
}
