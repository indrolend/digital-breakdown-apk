#pragma once

#include <array>
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
enum class PrimaryLightSource : unsigned char { OutdoorSun, UrbanSky, CeilingFixtures };
struct LocalLightDefinition {
    Vec3 localPosition{};
    Vec3 fixtureSize{};
    VisualColor color{};
    float intensity=0.0f;
    float radius=1.0f;
    bool visibleFixture=false;
};
struct RoomLightingProfile {
    VisualColor skyTop{};
    VisualColor skyHorizon{};
    VisualColor ambient{};
    VisualColor primary{};
    Vec3 primaryDirection{};
    VisualColor fill{};
    Vec3 fillDirection{};
    VisualColor phone{};
    VisualColor fog{};
    float fogDensity=0.0f;
    PrimaryLightSource primarySource=PrimaryLightSource::OutdoorSun;
    std::array<LocalLightDefinition,3> localLights{};
    int localLightCount=0;
    float contactOcclusion=0.10f;
    float cornerOcclusion=0.08f;
};

inline RoomLightingProfile roomLightingProfile(early_browser_visuals::RoomSetting setting,early_browser_visuals::RoomForm form,int roomSeed,int roomIndex,float time,float phonePower){
    const float pulse=0.98f+0.02f*(0.5f+0.5f*std::sin(time*0.73f+static_cast<float>(roomIndex)*0.41f));
    const float roomThreat=clampf((static_cast<float>(roomIndex)-1.0f)/18.0f,0.0f,1.0f);
    const float variation=0.96f+0.08f*(0.5f+0.5f*std::sin(static_cast<float>(roomSeed)*0.0173f+static_cast<float>(roomIndex)*1.91f));
    const float phonePulse=clampf(phonePower,0.0f,1.0f);
    RoomLightingProfile profile{};
    using early_browser_visuals::RoomSetting;
    switch(setting){
        case RoomSetting::Field:
            profile={{0.42f,0.55f,0.61f},{0.76f,0.75f,0.66f},{0.24f,0.27f,0.23f},{0.96f,0.84f,0.68f},{-18.0f,50.0f,12.0f},{0.16f,0.22f,0.25f},{20.0f,25.0f,-30.0f},{},{0.60f,0.63f,0.58f},0.010f,PrimaryLightSource::OutdoorSun};profile.contactOcclusion=0.09f;profile.cornerOcclusion=0.055f;break;
        case RoomSetting::Sterile:
            profile={{0.008f,0.012f,0.014f},{0.012f,0.019f,0.022f},{0.080f,0.100f,0.105f},{},{0.0f,1.0f,0.0f},{},{},{},{0.025f,0.034f,0.037f},0.018f,PrimaryLightSource::CeilingFixtures};
            profile.contactOcclusion=0.16f;profile.cornerOcclusion=0.14f;
            profile.localLightCount=form==early_browser_visuals::RoomForm::Chamber?3:2;
            for(int i=0;i<profile.localLightCount;++i){const float z=profile.localLightCount==3?(-10.0f+10.0f*static_cast<float>(i)):(i==0?-8.0f:8.0f);profile.localLights[i]={{0.0f,7.02f,z},{5.8f,0.055f,0.72f},{0.68f,0.88f,0.94f},i==profile.localLightCount-1?1.05f:1.24f,12.5f,true};}
            break;
        case RoomSetting::City:
            profile={{0.16f,0.18f,0.20f},{0.50f,0.47f,0.40f},{0.12f,0.12f,0.13f},{0.74f,0.69f,0.58f},{-12.0f,50.0f,-8.0f},{0.11f,0.14f,0.18f},{18.0f,24.0f,-28.0f},{},{0.30f,0.29f,0.27f},form==early_browser_visuals::RoomForm::Canyon?0.022f:0.017f,PrimaryLightSource::UrbanSky};profile.contactOcclusion=0.13f;profile.cornerOcclusion=0.11f;break;
        case RoomSetting::Coastal:
            profile={{0.40f,0.57f,0.63f},{0.82f,0.78f,0.65f},{0.22f,0.26f,0.25f},{0.98f,0.86f,0.68f},{-16.0f,48.0f,10.0f},{0.15f,0.24f,0.28f},{20.0f,25.0f,-30.0f},{},{0.65f,0.68f,0.61f},0.014f,PrimaryLightSource::OutdoorSun};profile.contactOcclusion=0.08f;profile.cornerOcclusion=0.05f;break;
    }
    const auto scaled=[](VisualColor color,float scale){return VisualColor{color.r*scale,color.g*scale,color.b*scale};};
    profile.skyTop=scaled(profile.skyTop,variation);profile.skyHorizon=scaled(profile.skyHorizon,variation);
    profile.ambient=scaled(profile.ambient,variation);profile.primary=scaled(profile.primary,pulse);profile.fill=scaled(profile.fill,variation);
    profile.phone={0.18f*phonePulse,1.05f*phonePulse,1.32f*phonePulse};profile.fogDensity+=roomThreat*0.004f;
    return profile;
}

} // namespace render_contract
