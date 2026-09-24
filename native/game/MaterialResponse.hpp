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

struct FloorMaterialResponse {
    VisualColor color{};
    float wetness=0.0f;
    float specular=0.0f;
    float shininess=0.0f;
    float traction=1.0f;
    float movementCueTransmission=1.0f;
};

inline FloorMaterialResponse floorMaterialResponse(early_browser_visuals::RoomSetting setting,float goalProgress,float wetness,bool exposed) {
    using early_browser_visuals::RoomSetting;
    const bool field=setting==RoomSetting::Field,sterile=setting==RoomSetting::Sterile,coastal=setting==RoomSetting::Coastal;
    const VisualColor base=field?Pass7Visual::FieldGround:(sterile?VisualColor{0.58f,0.61f,0.63f}:(coastal?VisualColor{0.24f,0.43f,0.50f}:Pass7Visual::RoomFloor));
    const VisualColor awakened=field?VisualColor{0.42f,0.58f,0.34f}:(sterile?VisualColor{0.61f,0.65f,0.67f}:(coastal?VisualColor{0.31f,0.50f,0.55f}:VisualColor{0.31f,0.30f,0.28f}));
    const float progress=clampf(goalProgress,0.0f,1.0f)*0.14f;
    const float wet=exposed?clampf(wetness,0.0f,1.0f):0.0f;
    const float darken=1.0f-wet*(field?0.20f:(coastal?0.10f:0.15f));
    FloorMaterialResponse response{};
    response.color={(base.r+(awakened.r-base.r)*progress)*darken,(base.g+(awakened.g-base.g)*progress)*darken,(base.b+(awakened.b-base.b)*progress)*darken};
    response.wetness=wet;
    response.specular=wet*0.34f;
    response.shininess=wet>0.04f?10.0f+wet*22.0f:0.0f;
    // The same wet material authority that changes appearance also changes the
    // ground reaction available to physical bodies. Different substrates lose
    // different amounts of grip; shelter removes both effects together.
    const float wetGripLoss=field?0.28f:(coastal?0.34f:(sterile?0.16f:0.20f));
    response.traction=1.0f-wet*wetGripLoss;
    // Surface character also shapes how much body motion carries as a cue. Hard
    // floors report movement more clearly than vegetation/sand, while the same
    // exposed wetness that changes color and grip slightly damps that report.
    const float dryMovementCue=field?0.76f:(coastal?0.82f:(sterile?0.94f:1.0f));
    response.movementCueTransmission=dryMovementCue*(1.0f-wet*0.18f);
    return response;
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
