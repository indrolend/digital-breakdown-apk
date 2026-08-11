#pragma once

#include "Math.hpp"

#include <algorithm>
#include <cmath>

struct VisualColor {
    float r, g, b;
};

namespace Pass7Visual {
constexpr float CameraVerticalFovDegrees = 75.0f;
constexpr float CameraNearPlane = 0.1f;
constexpr float CameraFarPlane = 1000.0f;
constexpr VisualColor GalaxyViolet{0x4f / 255.0f, 0x4c / 255.0f, 0xb1 / 255.0f};
constexpr VisualColor ElectricMagenta{0x98 / 255.0f, 0x1e / 255.0f, 0x97 / 255.0f};
constexpr VisualColor ElectricCyan{0x78 / 255.0f, 0xd5 / 255.0f, 0xe1 / 255.0f};
constexpr VisualColor MetallicTeal{0x7f / 255.0f, 0xa9 / 255.0f, 0xae / 255.0f};
constexpr VisualColor SignalGreen{0x4d / 255.0f, 0xe8 / 255.0f, 0x8e / 255.0f};
constexpr VisualColor AcidChartreuse{0xd4 / 255.0f, 0xec / 255.0f, 0x8e / 255.0f};
constexpr VisualColor DeepPlum{0x60 / 255.0f, 0x3f / 255.0f, 0x5b / 255.0f};
constexpr VisualColor Copper{0xaa / 255.0f, 0x80 / 255.0f, 0x66 / 255.0f};
constexpr VisualColor WarmGold{0xe1 / 255.0f, 0xb8 / 255.0f, 0x7f / 255.0f};
constexpr VisualColor Background{0x08 / 255.0f, 0x10 / 255.0f, 0x18 / 255.0f};
constexpr VisualColor Floor{0x7f / 255.0f, 0xa9 / 255.0f, 0xae / 255.0f};
constexpr VisualColor Wall{0x86 / 255.0f, 0x8b / 255.0f, 0xb2 / 255.0f};
constexpr VisualColor RoomFloor{0.50f, 0.55f, 0.57f};
constexpr VisualColor RoomWall{0.39f, 0.43f, 0.46f};
constexpr VisualColor RoomObstacle{0.43f, 0.49f, 0.53f};
constexpr VisualColor SecretFloor{0.055f, 0.065f, 0.068f};
constexpr VisualColor SecretWall{0.075f, 0.082f, 0.085f};
constexpr VisualColor SecretBlack{0.018f, 0.020f, 0.021f};
constexpr VisualColor SecretCable{0.025f, 0.028f, 0.030f};
constexpr VisualColor TvMembrane{0.45f, 0.86f, 0.91f};
constexpr VisualColor SoulFlesh{224.0f / 255.0f, 160.0f / 255.0f, 143.0f / 255.0f};
constexpr VisualColor Tether{1.0f, 183.0f / 255.0f, 166.0f / 255.0f};
constexpr VisualColor PhoneBody{0xd0 / 255.0f, 0xd0 / 255.0f, 0xd0 / 255.0f};
constexpr VisualColor PhoneScreen{0x16 / 255.0f, 0x20 / 255.0f, 0x2a / 255.0f};
constexpr VisualColor PhoneEmission{0x78 / 255.0f, 0xd5 / 255.0f, 0xe1 / 255.0f};
constexpr VisualColor SoulBase{0xd4 / 255.0f, 0xec / 255.0f, 0x8e / 255.0f};
constexpr VisualColor SoulEmission{0x78 / 255.0f, 0xd5 / 255.0f, 0xe1 / 255.0f};
constexpr VisualColor HitFlash{1.0f, 1.0f, 1.0f};
constexpr VisualColor NormalEnemy{0x22 / 255.0f, 0x2a / 255.0f, 0x30 / 255.0f};
constexpr VisualColor BruteEnemy{0x43 / 255.0f, 0x2b / 255.0f, 0x2b / 255.0f};
constexpr VisualColor Flower{0xd4 / 255.0f, 0xec / 255.0f, 0x8e / 255.0f};
constexpr VisualColor FlowerCore{0x33 / 255.0f, 0x55 / 255.0f, 0x25 / 255.0f};
constexpr float HemisphereIntensity = 1.1f;
constexpr float SunIntensity = 1.0f;
constexpr float FillIntensity = 0.35f;

constexpr VisualColor DataMosaicPalette[25] = {
    {0x66 / 255.0f, 0x6b / 255.0f, 0xb2 / 255.0f},
    {0x4f / 255.0f, 0x4c / 255.0f, 0xb1 / 255.0f},
    {0x98 / 255.0f, 0x1e / 255.0f, 0x97 / 255.0f},
    {0x74 / 255.0f, 0x53 / 255.0f, 0x71 / 255.0f},
    {0x60 / 255.0f, 0x3f / 255.0f, 0x5b / 255.0f},
    {0x78 / 255.0f, 0xd5 / 255.0f, 0xe1 / 255.0f},
    {0x7f / 255.0f, 0xa9 / 255.0f, 0xae / 255.0f},
    {0xc4 / 255.0f, 0xd6 / 255.0f, 0xa4 / 255.0f},
    {0xd4 / 255.0f, 0xec / 255.0f, 0x8e / 255.0f},
    {0xd0 / 255.0f, 0xde / 255.0f, 0xbb / 255.0f},
    {0x93 / 255.0f, 0x78 / 255.0f, 0x91 / 255.0f},
    {0x67 / 255.0f, 0x45 / 255.0f, 0x64 / 255.0f},
    {0x33 / 255.0f, 0x55 / 255.0f, 0x25 / 255.0f},
    {0xaa / 255.0f, 0x80 / 255.0f, 0x66 / 255.0f},
    {0xae / 255.0f, 0x8a / 255.0f, 0x79 / 255.0f},
    {0xf1 / 255.0f, 0xc6 / 255.0f, 0xa6 / 255.0f},
    {0xe1 / 255.0f, 0xb8 / 255.0f, 0x7f / 255.0f},
    {0xb9 / 255.0f, 0x97 / 255.0f, 0x5b / 255.0f},
    {0x99 / 255.0f, 0x55 / 255.0f, 0x2b / 255.0f},
    {0x8b / 255.0f, 0x5b / 255.0f, 0x29 / 255.0f},
    {0x4e / 255.0f, 0x36 / 255.0f, 0x29 / 255.0f},
    {0x5b / 255.0f, 0x34 / 255.0f, 0x27 / 255.0f},
    {0x78 / 255.0f, 0xd5 / 255.0f, 0xe1 / 255.0f},
    {0xd4 / 255.0f, 0xec / 255.0f, 0x8e / 255.0f},
    {0xf1 / 255.0f, 0xc6 / 255.0f, 0xa6 / 255.0f}
};
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
    float verticalOffset = 0.0f;
    float rotationY = 0.0f;
    float morphScale = 0.0f;
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
inline SoulVisualState makeSoulVisualState(int soulState, float vacuumPull, float ingest, float hit, float time, float seed, bool alive,
    float morph = 1.0f, float floatOffset = 0.0f, float spinSpeed = 0.8f) {
    SoulVisualState visual;
    visual.hitAmount = std::max(0.0f, std::min(1.0f, hit));
    visual.pullAmount = soulState == 1 ? std::max(0.0f, std::min(1.0f, vacuumPull)) : 0.0f;
    visual.latchAmount = (soulState == 2 || soulState == 3) ? 1.0f : 0.0f;
    visual.ingestAmount = std::max(0.0f, std::min(1.0f, ingest));
    visual.phase = time * 7.0f + seed;
    visual.verticalOffset = std::sin(time * 2.0f + floatOffset) * 0.18f;
    visual.rotationY = time * spinSpeed;
    // Once latched, the soul moves into the phone at close camera range. Contract
    // the translucent shell with that motion instead of leaving it full-sized
    // until the final visibility cutoff, where it can obscure the capture itself.
    const float ingestContraction = 1.0f - visualSmooth01(visual.ingestAmount);
    visual.morphScale = visualSmooth01(morph) * ingestContraction;
    const float active = std::max(visual.pullAmount, std::max(visual.latchAmount * 0.72f, visual.ingestAmount));
    visual.elasticity = std::max(0.0f, std::min(1.0f, active + visual.hitAmount * 0.18f));
    const float baseBreath = 1.0f + std::sin(time * 3.0f + seed * 1.7f) * 0.035f;
    const float readyPulse = 1.0f + std::sin(time * 18.0f + seed) * 0.055f;
    const float hitPulse = 1.0f + visual.hitAmount * 0.16f;
    const float uniformScale = baseBreath * readyPulse * hitPulse;
    visual.scale = {uniformScale, uniformScale, uniformScale};
    visual.deformation = {0.0f, 0.0f, 0.0f};
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
