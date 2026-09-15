#pragma once

#include <cmath>
#include "EarlyBrowserVisuals.hpp"
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

enum class PrimaryLightSource : unsigned char { OutdoorSun, UrbanSky, CeilingFixtures };
constexpr PrimaryLightSource primaryLightSourceFor(early_browser_visuals::RoomSetting setting){
    using early_browser_visuals::RoomSetting;
    return setting==RoomSetting::Sterile?PrimaryLightSource::CeilingFixtures:
        (setting==RoomSetting::City?PrimaryLightSource::UrbanSky:PrimaryLightSource::OutdoorSun);
}

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

inline SceneAtmosphere sceneAtmosphere(float time,int roomIndex,int roomSeed,float phonePower,early_browser_visuals::RoomSetting setting){
    const float omenPulse=0.5f+0.5f*std::sin(time*0.73f+static_cast<float>(roomIndex)*0.41f);
    const float roomThreat=clampf((static_cast<float>(roomIndex)-1.0f)/18.0f,0.0f,1.0f);
    const float variation=0.92f+0.16f*(0.5f+0.5f*std::sin(static_cast<float>(roomSeed)*0.0173f+static_cast<float>(roomIndex)*1.91f));
    const float phonePulse=clampf(phonePower,0.0f,1.0f);
    SceneAtmosphere atmosphere{};
    using early_browser_visuals::RoomSetting;
    switch(setting){
        case RoomSetting::Field:
            atmosphere={{0.025f,0.055f,0.075f},{0.20f,0.31f,0.22f},{0.92f,0.72f,0.38f},{0.12f,0.38f,0.55f},{},{0.035f,0.075f,0.085f},0.010f};break;
        case RoomSetting::Sterile:
            atmosphere={{0.003f,0.007f,0.011f},{0.035f,0.060f,0.075f},{0.38f,0.56f,0.68f},{0.055f,0.16f,0.23f},{},{0.018f,0.032f,0.043f},0.025f};break;
        case RoomSetting::City:
            atmosphere={{0.006f,0.005f,0.014f},{0.060f,0.055f,0.085f},{0.62f,0.24f,0.20f},{0.08f,0.28f,0.46f},{},{0.020f,0.014f,0.036f},0.021f};break;
        case RoomSetting::Coastal:
            atmosphere={{0.018f,0.045f,0.065f},{0.15f,0.25f,0.27f},{0.86f,0.62f,0.36f},{0.10f,0.42f,0.58f},{},{0.035f,0.080f,0.095f},0.014f};break;
    }
    atmosphere.background={atmosphere.background.r*variation,atmosphere.background.g*variation,atmosphere.background.b*variation};
    atmosphere.ambient={atmosphere.ambient.r*variation,atmosphere.ambient.g*variation,atmosphere.ambient.b*variation};
    atmosphere.sun={atmosphere.sun.r*(0.94f+omenPulse*0.06f),atmosphere.sun.g*(0.94f+omenPulse*0.06f),atmosphere.sun.b*(0.94f+omenPulse*0.06f)};
    atmosphere.fill={atmosphere.fill.r*variation,atmosphere.fill.g*variation,atmosphere.fill.b*variation};
    atmosphere.phone={0.18f*phonePulse,1.05f*phonePulse,1.32f*phonePulse};
    atmosphere.fogDensity+=roomThreat*0.004f;
    return atmosphere;
}

} // namespace render_contract
