#include "RenderContracts.hpp"
#include <cstdio>

int main(){
    using namespace render_contract;
    static_assert(androidShadingSelector(ShadingModel::Unlit)==-1.0f);
    static_assert(androidShadingSelector(ShadingModel::ColorGraded)==0.0f);
    static_assert(androidShadingSelector(ShadingModel::NormalLit)==1.0f);
    if(DesktopSceneLighting.sun.direction.x!=30.0f||DesktopSceneLighting.fog.density!=0.018f){
        std::fputs("RENDER_CONTRACTS_FAIL desktop profile\n",stderr);return 1;
    }
    if(AndroidSceneLighting.sun.intensity!=0.42f){
        std::fputs("RENDER_CONTRACTS_FAIL android profile\n",stderr);return 1;
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
    std::puts("RENDER_CONTRACTS_OK profiles=2 shading_models=3 shadow_qualities=3 field_grass=TEXTURED city_ground=TEXTURED");
    return 0;
}
