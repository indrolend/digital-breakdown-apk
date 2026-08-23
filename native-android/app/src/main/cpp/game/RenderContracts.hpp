#pragma once

#include "Math.hpp"
#include "VisualIdentity.hpp"

namespace render_contract {

enum class ShadingModel : unsigned char { Unlit, ColorGraded, NormalLit };
enum class TextureId : unsigned char { None, FieldGrass, CityAsphalt };
enum class ShadowQuality : unsigned char { Off, Cheap, Directional };

constexpr ShadowQuality shadowQualityFor(int graphicsPreset,bool shadowsEnabled,bool directionalSupported){
    if(!shadowsEnabled)return ShadowQuality::Off;
    return graphicsPreset>=2&&directionalSupported?ShadowQuality::Directional:ShadowQuality::Cheap;
}

struct MaterialDefinition {
    VisualColor baseColor{1.0f,1.0f,1.0f};
    ShadingModel shading=ShadingModel::ColorGraded;
    float opacity=1.0f;
    bool fog=true;
    TextureId texture=TextureId::None;
    float textureWorldScale=1.0f;
};

constexpr MaterialDefinition sceneMatte(VisualColor color,float opacity=1.0f){return {color,ShadingModel::ColorGraded,opacity,true,TextureId::None,1.0f};}
constexpr MaterialDefinition normalLit(VisualColor color,float opacity=1.0f){return {color,ShadingModel::NormalLit,opacity,true,TextureId::None,1.0f};}
constexpr MaterialDefinition unlit(VisualColor color,float opacity=1.0f){return {color,ShadingModel::Unlit,opacity,false,TextureId::None,1.0f};}
inline constexpr MaterialDefinition FieldOpenGround{Pass7Visual::FieldGround,ShadingModel::ColorGraded,1.0f,true,TextureId::FieldGrass,2.4f};
inline constexpr MaterialDefinition CityGround{{0.24f,0.26f,0.28f},ShadingModel::ColorGraded,1.0f,true,TextureId::CityAsphalt,3.2f};
constexpr float androidShadingSelector(ShadingModel model){return model==ShadingModel::Unlit?-1.0f:(model==ShadingModel::NormalLit?1.0f:0.0f);}

struct DirectionalLightDefinition { Vec3 direction{};VisualColor color{1,1,1};float intensity=1.0f; };
struct FogDefinition { VisualColor color{};float density=0.0f; };
struct SceneLightingDefinition {
    VisualColor ambient{};
    DirectionalLightDefinition sun{};
    DirectionalLightDefinition fill{};
    FogDefinition fog{};
};

// These profiles formalize the deliberately different implementations that
// existed before the contract. A later art-direction pass may choose convergence.
inline const SceneLightingDefinition DesktopSceneLighting{
    {0.32f,0.43f,0.34f},{{30.0f,60.0f,25.0f},{1,1,1},1.0f},
    {{-20.0f,25.0f,-30.0f},{0.20f,0.28f,0.35f},1.0f},{Pass7Visual::Background,0.018f}};
inline const SceneLightingDefinition AndroidSceneLighting{
    {0.48f,0.48f,0.48f},{{0.42f,0.84f,0.35f},{1,1,1},0.42f},
    {{-0.46f,0.57f,-0.68f},{1,1,1},0.10f},{Pass7Visual::Background,0.0f}};

} // namespace render_contract
