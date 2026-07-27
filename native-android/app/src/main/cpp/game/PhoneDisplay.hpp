#pragma once

#include "Math.hpp"

enum class PhoneDisplayMode : unsigned char {
    Off,
    Boot,
    MainMenu,
    Online,
    JoinCode,
    Settings,
    Controls,
    Audio,
    Graphics,
    Gameplay,
    Pause,
    Upgrade,
    Warning,
    Death,
    Restarting,
    Transition
};

struct PhoneDisplayMaterialState {
    float backgroundEmission = 0.0f;
    float glassEmission = 0.0f;
    float rimEmission = 0.0f;
    float displayBrightness = 0.0f;
    Vec3 emissionColor{0.07f, 0.19f, 0.29f};
    float emissionIntensity = 0.0f;
    float glassOpacity = 0.18f;
    float glassRoughness = 0.42f;
    float blackLevel = 0.92f;
};

struct PhoneDisplayLightingState {
    Vec3 color{0.36f, 0.84f, 1.0f};
    float intensity = 0.0f;
    float radius = 0.18f;
    float forwardOffset = 0.024f;
    float pulse = 0.0f;
};

struct PhoneDisplayState {
    static constexpr int LogicalWidth = 720;
    static constexpr int LogicalHeight = 1280;

    PhoneDisplayMode mode = PhoneDisplayMode::Off;
    PhoneDisplayMode previousMode = PhoneDisplayMode::Off;
    float transitionProgress = 1.0f;
    float brightness = 0.0f;
    float contentOpacity = 0.0f;
    Vec3 screenTint{0.07f, 0.19f, 0.29f};
    Vec3 emissionColor{0.07f, 0.19f, 0.29f};
    float emissionStrength = 0.0f;
    float localLightIntensity = 0.0f;
    float localLightRadius = 0.18f;
    float glassResponse = 0.18f;
    float blackLevel = 0.92f;
    float damagePulse = 0.0f;
    float capturePulse = 0.0f;
    float powerPulse = 0.0f;
    float lowBatteryPulse = 0.0f;
    float warningPulse = 0.0f;
    float screenNoisePhase = 0.0f;
    bool interactive = false;
    PhoneDisplayMaterialState material;
    PhoneDisplayLightingState lighting;
};

inline const char* phoneDisplayModeName(PhoneDisplayMode mode) {
    switch (mode) {
        case PhoneDisplayMode::Off: return "Off";
        case PhoneDisplayMode::Boot: return "Boot";
        case PhoneDisplayMode::MainMenu: return "MainMenu";
        case PhoneDisplayMode::Online: return "Online";
        case PhoneDisplayMode::JoinCode: return "JoinCode";
        case PhoneDisplayMode::Settings: return "Settings";
        case PhoneDisplayMode::Controls: return "Controls";
        case PhoneDisplayMode::Audio: return "Audio";
        case PhoneDisplayMode::Graphics: return "Graphics";
        case PhoneDisplayMode::Gameplay: return "Gameplay";
        case PhoneDisplayMode::Pause: return "Pause";
        case PhoneDisplayMode::Upgrade: return "Upgrade";
        case PhoneDisplayMode::Warning: return "Warning";
        case PhoneDisplayMode::Death: return "Death";
        case PhoneDisplayMode::Restarting: return "Restarting";
        case PhoneDisplayMode::Transition: return "Transition";
    }
    return "Off";
}
