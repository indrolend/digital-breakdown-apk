#pragma once

#include "Math.hpp"

#include <algorithm>
#include <cmath>

struct VisualColor {
    float r, g, b;
};

namespace Pass7Visual {
constexpr VisualColor Background{0x08 / 255.0f, 0x10 / 255.0f, 0x18 / 255.0f};
constexpr VisualColor Floor{0x9a / 255.0f, 0xa7 / 255.0f, 0xad / 255.0f};
constexpr VisualColor Wall{0x8f / 255.0f, 0x98 / 255.0f, 0xa3 / 255.0f};
constexpr VisualColor PhoneBody{0xd0 / 255.0f, 0xd0 / 255.0f, 0xd0 / 255.0f};
constexpr VisualColor PhoneScreen{0x16 / 255.0f, 0x20 / 255.0f, 0x2a / 255.0f};
constexpr VisualColor PhoneEmission{0x12 / 255.0f, 0x30 / 255.0f, 0x4a / 255.0f};
constexpr VisualColor SoulBase{0x8f / 255.0f, 0xf7 / 255.0f, 1.0f};
constexpr VisualColor SoulEmission{0x1d / 255.0f, 0x9c / 255.0f, 1.0f};
constexpr VisualColor HitFlash{1.0f, 1.0f, 1.0f};
constexpr VisualColor NormalEnemy{0x22 / 255.0f, 0x2a / 255.0f, 0x30 / 255.0f};
constexpr VisualColor BruteEnemy{0x43 / 255.0f, 0x2b / 255.0f, 0x2b / 255.0f};
constexpr float HemisphereIntensity = 1.1f;
constexpr float SunIntensity = 1.0f;
constexpr float FillIntensity = 0.35f;
}

struct PhoneVisualState {
    float vacuumPose = 0.0f;
    float slurpStretch = 0.0f;
    float screenGlow = 0.75f;
    float screenPulse = 0.0f;
    float captureContactAmount = 0.0f;
    float actionLift = 0.0f;
    float actionForward = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    Vec3 bodyScale{1.0f, 1.0f, 1.0f};
    Vec3 screenScale{1.0f, 1.0f, 1.0f};
    float screenOffset = 0.0f;
    bool visible = true;
};

struct SoulVisualState {
    VisualColor color = Pass7Visual::SoulBase;
    float emission = 0.42f;
    float hitAmount = 0.0f;
    float pullAmount = 0.0f;
    float latchAmount = 0.0f;
    float ingestAmount = 0.0f;
    float elasticity = 0.0f;
    float phase = 0.0f;
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Vec3 deformation{0.0f, 0.0f, 0.0f};
    bool visible = true;
};

inline float visualSmooth01(float value) {
    const float t = std::max(0.0f, std::min(1.0f, value));
    return t * t * (3.0f - 2.0f * t);
}

inline PhoneVisualState makePhoneVisualState(float vacuumPose, float vacuumPower, float contact, float time, bool firstPerson) {
    PhoneVisualState visual;
    visual.vacuumPose = std::max(0.0f, std::min(1.0f, vacuumPose));
    visual.captureContactAmount = std::max(0.0f, std::min(1.0f, contact));
    visual.slurpStretch = std::max(visual.vacuumPose * vacuumPower, visual.captureContactAmount);
    visual.screenPulse = (0.5f + 0.5f * std::sin(time * 18.0f)) * visual.slurpStretch;
    visual.screenGlow = std::min(2.8f, 0.75f + visual.slurpStretch * 0.72f + visual.screenPulse * 0.20f);
    visual.actionLift = visual.vacuumPose * 0.65f;
    visual.actionForward = visual.vacuumPose * 0.25f;
    visual.pitch = -0.28f * visualSmooth01(visual.vacuumPose);
    visual.roll = std::sin(time * 18.0f) * 0.035f * visual.vacuumPose;
    visual.bodyScale = {1.0f - visual.slurpStretch * 0.035f, 1.0f + visual.slurpStretch * 0.10f, 1.0f};
    visual.screenScale = {1.0f + visual.captureContactAmount * 0.12f, 1.0f - visual.captureContactAmount * 0.08f, 1.0f};
    visual.screenOffset = visual.captureContactAmount * 0.012f;
    visual.visible = !firstPerson;
    return visual;
}

// soulState follows SoulState's Free, Attracted, Latched, Ingesting, Recoiling order.
inline SoulVisualState makeSoulVisualState(int soulState, float vacuumPull, float ingest, float hit, float time, float seed, bool alive) {
    SoulVisualState visual;
    visual.hitAmount = std::max(0.0f, std::min(1.0f, hit));
    visual.pullAmount = soulState == 1 ? std::max(0.0f, std::min(1.0f, vacuumPull)) : 0.0f;
    visual.latchAmount = (soulState == 2 || soulState == 3) ? 1.0f : 0.0f;
    visual.ingestAmount = std::max(0.0f, std::min(1.0f, ingest));
    visual.phase = time * 7.0f + seed;
    const float active = std::max(visual.pullAmount, std::max(visual.latchAmount * 0.72f, visual.ingestAmount));
    visual.elasticity = std::max(0.0f, std::min(1.0f, active + visual.hitAmount * 0.18f));
    const float wave = std::sin(visual.phase) * 0.04f * (0.35f + visual.elasticity * 0.65f);
    const float stretch = visual.pullAmount * 0.18f + visual.latchAmount * 0.12f;
    const float squash = visual.ingestAmount * 0.42f;
    visual.scale = {
        std::max(0.12f, 1.0f - stretch * 0.45f - squash * 0.38f + wave),
        std::max(0.12f, 1.0f + stretch - squash * 0.28f - wave * 0.5f),
        std::max(0.12f, 1.0f - stretch * 0.45f - squash * 0.38f + wave)
    };
    visual.deformation = {wave, stretch, -squash};
    visual.emission = std::max(0.0f, std::min(1.5f, 0.42f + visual.pullAmount * 0.18f + visual.latchAmount * 0.25f + visual.ingestAmount * 0.48f + visual.hitAmount * 0.38f));
    const float flash = visual.hitAmount;
    visual.color = {
        Pass7Visual::SoulBase.r + (1.0f - Pass7Visual::SoulBase.r) * flash,
        Pass7Visual::SoulBase.g + (1.0f - Pass7Visual::SoulBase.g) * flash,
        Pass7Visual::SoulBase.b
    };
    visual.visible = alive && visual.ingestAmount < 0.92f;
    return visual;
}
