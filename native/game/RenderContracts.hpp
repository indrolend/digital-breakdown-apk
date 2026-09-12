#pragma once

#include <cmath>
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
struct DirectionalLightDefinition { Vec3 direction{};VisualColor color{1,1,1};float intensity=1.0f; };
struct FogDefinition { VisualColor color{};float density=0.0f; };
struct SceneLightingDefinition {
    VisualColor ambient{};
    DirectionalLightDefinition sun{};
    DirectionalLightDefinition fill{};
    FogDefinition fog{};
};

inline const SceneLightingDefinition DesktopSceneLighting{
    {0.32f,0.43f,0.34f},{{30.0f,60.0f,25.0f},{1,1,1},1.0f},
    {{-20.0f,25.0f,-30.0f},{0.20f,0.28f,0.35f},1.0f},{Pass7Visual::Background,0.018f}};

struct SceneAtmosphere {
    VisualColor background{};
    VisualColor ambient{};
    VisualColor sun{};
    VisualColor fill{};
    VisualColor phone{};
    VisualColor fog{};
    float fogDensity=0.0f;
};

inline SceneAtmosphere sceneAtmosphere(float time,int roomIndex,float phonePower){
    const float omenPulse=0.5f+0.5f*std::sin(time*0.73f+static_cast<float>(roomIndex)*0.41f);
    const float roomThreat=clampf((static_cast<float>(roomIndex)-1.0f)/18.0f,0.0f,1.0f);
    const float phonePulse=clampf(phonePower,0.0f,1.0f);
    return {
        {0.003f+omenPulse*0.004f,0.002f,0.009f+roomThreat*0.008f},
        {0.018f+omenPulse*0.012f,0.014f,0.030f+roomThreat*0.018f},
        {0.48f+roomThreat*0.12f,0.055f+omenPulse*0.035f,0.13f+roomThreat*0.16f},
        {0.04f,0.30f+omenPulse*0.10f,0.52f+roomThreat*0.18f},
        {0.18f*phonePulse,1.05f*phonePulse,1.32f*phonePulse},
        {0.010f+roomThreat*0.018f,0.002f,0.024f+omenPulse*0.012f},
        0.020f+roomThreat*0.010f+omenPulse*0.003f
    };
}

} // namespace render_contract
