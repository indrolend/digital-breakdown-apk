#pragma once

#include "EarlyBrowserVisuals.hpp"
#include "VisualIdentity.hpp"

enum class ParticleMaterial : unsigned char { Impact, Flesh, Environment, Soul, Data };

inline constexpr VisualColor roomSubstrateColor(early_browser_visuals::RoomSetting setting) {
    using early_browser_visuals::RoomSetting;
    switch(setting) {
        case RoomSetting::Field: return {0.25f,0.45f,0.29f};
        case RoomSetting::City: return {0.43f,0.49f,0.53f};
        case RoomSetting::Sterile: return {0.62f,0.66f,0.69f};
        case RoomSetting::Coastal: return {0.54f,0.48f,0.36f};
    }
    return Pass7Visual::RoomObstacle;
}

inline VisualColor mixVisualColor(const VisualColor& from,const VisualColor& to,float amount) {
    const float t=visualSmooth01(clampf(amount,0.0f,1.0f));
    return {from.r+(to.r-from.r)*t,from.g+(to.g-from.g)*t,from.b+(to.b-from.b)*t};
}

inline float humanSubstrateAmount(float armor,float armorMax,bool slurpable) {
    if(slurpable||armorMax<=0.001f)return 1.0f;
    const float damage=1.0f-clampf(armor/armorMax,0.0f,1.0f);
    return visualSmooth01(clampf((damage-0.28f)/0.72f,0.0f,1.0f))*0.68f;
}

inline VisualColor humanDamageSurfaceColor(const VisualColor& base,
    early_browser_visuals::RoomSetting setting,float armor,float armorMax,bool slurpable,float hitFlash) {
    VisualColor color=mixVisualColor(base,roomSubstrateColor(setting),humanSubstrateAmount(armor,armorMax,slurpable));
    const float flash=clampf(hitFlash,0.0f,1.0f)*0.34f;
    return mixVisualColor(color,Pass7Visual::HitFlash,flash);
}

inline VisualColor particleMaterialColor(ParticleMaterial material,
    early_browser_visuals::RoomSetting setting,float remainingLife) {
    const VisualColor substrate=roomSubstrateColor(setting);
    switch(material) {
        case ParticleMaterial::Flesh: return Pass7Visual::SoulFlesh;
        case ParticleMaterial::Environment:
            return mixVisualColor(Pass7Visual::SoulFlesh,substrate,1.0f-clampf(remainingLife,0.0f,1.0f));
        case ParticleMaterial::Soul: return Pass7Visual::SoulBase;
        case ParticleMaterial::Data: return Pass7Visual::ElectricCyan;
        case ParticleMaterial::Impact: return {1.0f,0.267f,0.267f};
    }
    return Pass7Visual::HitFlash;
}
