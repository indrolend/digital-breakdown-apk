#pragma once

#include "Game.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

enum class PhoneMenuRowKind : unsigned char { Section, Item, TwoColumn };
enum class PhoneMenuAction : unsigned char {
    None,
    Start,
    Solo,
    Online,
    Settings,
    Exit,
    Resume,
    Controls,
    Audio,
    Graphics,
    ExitRun,
    Host,
    Join,
    Back,
    Rebind,
    AdjustMouse,
    AdjustController,
    AdjustTriggers,
    AdjustVibration,
    Defaults,
    MusicVolume,
    SfxVolume,
    MusicMute,
    SfxMute,
    GraphicsPreset,
    ToggleShadows,
    ToggleParticles,
    ToggleFps,
    CheckUpdates,
    Restart
};

enum class PhoneMenuHorizontal : unsigned char { None, Adjust, Toggle };

inline void applyPhoneGraphicsPreset(LocalSettingsState& settings, int preset) {
    settings.graphicsPreset = std::max(0, std::min(2, preset));
    settings.shadows = settings.graphicsPreset >= 2;
    settings.portalWindow = settings.graphicsPreset >= 1;
    settings.particles = settings.graphicsPreset >= 1;
}

struct PhoneMenuElement {
    PhoneMenuRowKind kind = PhoneMenuRowKind::Item;
    PhoneMenuAction action = PhoneMenuAction::None;
    int bindingAction = -1;
    std::string label;
    std::string value;
    bool selectable = false;
    PhoneMenuHorizontal horizontal = PhoneMenuHorizontal::None;
};

struct PhoneMenuPageViewModel {
    static constexpr int MaxElements = 24;
    std::string title;
    std::array<PhoneMenuElement, MaxElements> elements{};
    int elementCount = 0;
    int selectableCount = 0;
    bool paletteTitle = false;
    bool joinCode = false;
    bool tablePage = false;
};

inline std::string phoneMenuKeyName(int key) {
    if (key >= 65 && key <= 90) return std::string(1, static_cast<char>(key));
    if (key >= 48 && key <= 57) return std::string(1, static_cast<char>(key));
    if (key == 32) return "SPACE";
    if (key == 340 || key == 344) return "SHIFT";
    return "KEY " + std::to_string(key);
}

inline int phoneMenuPercent(float value) {
    return static_cast<int>(std::round(value * 100.0f));
}

inline int phoneMenuCycleIndex(int value, int direction, int count) {
    if (count <= 0 || direction == 0) return value;
    const int normalized = (value % count + count) % count;
    return (normalized + (direction > 0 ? 1 : count - 1)) % count;
}

inline float phoneMenuCycleFloat(float value, int direction, float minimum, float maximum, float step) {
    if (direction == 0 || maximum <= minimum || step <= 0.0f) return clampf(value, minimum, maximum);
    constexpr float epsilon = 0.0001f;
    if (direction > 0 && value >= maximum - epsilon) return minimum;
    if (direction < 0 && value <= minimum + epsilon) return maximum;
    return clampf(value + (direction > 0 ? step : -step), minimum, maximum);
}

inline const char* phoneMenuTriggerSensitivityName(int value) {
    static constexpr const char* Names[] = {"Deep", "Balanced", "Hair"};
    return Names[std::max(0, std::min(2, value))];
}

inline const char* phoneMenuVibrationName(int value) {
    static constexpr const char* Names[] = {"Off", "Standard", "Strong"};
    return Names[std::max(0, std::min(2, value))];
}

inline void addPhoneMenuElement(PhoneMenuPageViewModel& page, PhoneMenuElement element) {
    if (page.elementCount >= PhoneMenuPageViewModel::MaxElements) return;
    if (element.selectable) ++page.selectableCount;
    page.elements[page.elementCount++] = element;
}

inline void addPhoneMenuSection(PhoneMenuPageViewModel& page, const std::string& label) {
    PhoneMenuElement element;
    element.kind = PhoneMenuRowKind::Section;
    element.label = label;
    addPhoneMenuElement(page, element);
}

inline void addPhoneMenuItem(PhoneMenuPageViewModel& page, const std::string& label, PhoneMenuAction action, int bindingAction = -1) {
    PhoneMenuElement element;
    element.kind = PhoneMenuRowKind::Item;
    element.action = action;
    element.bindingAction = bindingAction;
    element.label = label;
    element.selectable = true;
    addPhoneMenuElement(page, element);
}

inline void addPhoneMenuValue(PhoneMenuPageViewModel& page, const std::string& label, const std::string& value, PhoneMenuAction action, int bindingAction = -1) {
    PhoneMenuElement element;
    element.kind = PhoneMenuRowKind::TwoColumn;
    element.action = action;
    element.bindingAction = bindingAction;
    element.label = label;
    element.value = value;
    element.selectable = true;
    element.horizontal = action == PhoneMenuAction::Rebind
        ? PhoneMenuHorizontal::None
        : PhoneMenuHorizontal::Adjust;
    addPhoneMenuElement(page, element);
}

inline void addPhoneMenuToggle(PhoneMenuPageViewModel& page, const std::string& label, PhoneMenuAction action) {
    PhoneMenuElement element;
    element.kind = PhoneMenuRowKind::Item;
    element.action = action;
    element.label = label;
    element.selectable = true;
    element.horizontal = PhoneMenuHorizontal::Toggle;
    addPhoneMenuElement(page, element);
}

inline bool phoneMenuPausedSolo(const GameState& state) {
    return state.started && state.uiPaused && !state.multiplayer.enabled && !state.upgradeMenu.active;
}

inline PhoneMenuPageViewModel makePhoneMenuPageModel(const GameState& state) {
    PhoneMenuPageViewModel page;
    const bool pausedSolo = phoneMenuPausedSolo(state);
    if (state.cinematic.introActive) {
        addPhoneMenuItem(page, "Start", PhoneMenuAction::Start);
    } else if (state.dead) {
        page.title = "";
    } else if (pausedSolo && state.localSettings.menuPage == LocalMenuPage::Main) {
        page.title = "PAUSED";
        addPhoneMenuItem(page, "Resume", PhoneMenuAction::Resume);
        addPhoneMenuItem(page, "Controls", PhoneMenuAction::Controls);
        addPhoneMenuItem(page, "Audio", PhoneMenuAction::Audio);
        addPhoneMenuItem(page, "Graphics", PhoneMenuAction::Graphics);
        addPhoneMenuItem(page, "Exit Run", PhoneMenuAction::ExitRun);
    } else if (state.localSettings.menuPage == LocalMenuPage::Main) {
        addPhoneMenuItem(page, "Solo", PhoneMenuAction::Solo);
        addPhoneMenuItem(page, "Online", PhoneMenuAction::Online);
        addPhoneMenuItem(page, "Settings", PhoneMenuAction::Settings);
        addPhoneMenuItem(page, "Exit", PhoneMenuAction::Exit);
    } else if (state.localSettings.menuPage == LocalMenuPage::Online) {
        page.title = "Online";
        const std::string networkStatus=state.multiplayer.status.data();
        const std::string roomCode=state.multiplayer.roomCode.data();
        const bool inRoom=!roomCode.empty()&&
            (networkStatus.find("WAITING")!=std::string::npos||
             networkStatus.find("READY")!=std::string::npos||
             networkStatus.find("CONNECTED")!=std::string::npos||
             networkStatus.find("STARTING")!=std::string::npos||
             networkStatus.find("SYNCHRONIZING")!=std::string::npos);
        if(inRoom){
            addPhoneMenuSection(page,"Room "+roomCode);
            addPhoneMenuSection(page,networkStatus);
            if(networkStatus.find("READY 2/2")!=std::string::npos)
                addPhoneMenuItem(page,"Start Game",PhoneMenuAction::Start);
        }else{
            addPhoneMenuItem(page, "Host", PhoneMenuAction::Host);
            addPhoneMenuItem(page, "Join", PhoneMenuAction::Join);
        }
        addPhoneMenuItem(page, "Back", PhoneMenuAction::Back);
    } else if (state.localSettings.menuPage == LocalMenuPage::JoinCode) {
        page.title = "Enter Code";
        page.joinCode = true;
        addPhoneMenuItem(page, "Back", PhoneMenuAction::Back);
    } else if (state.localSettings.menuPage == LocalMenuPage::Settings) {
        page.title = "Settings";
        addPhoneMenuItem(page, "Controls", PhoneMenuAction::Controls);
        addPhoneMenuItem(page, "Audio", PhoneMenuAction::Audio);
        addPhoneMenuItem(page, "Graphics", PhoneMenuAction::Graphics);
        addPhoneMenuItem(page, "Check Updates", PhoneMenuAction::CheckUpdates);
        addPhoneMenuItem(page, "Back", PhoneMenuAction::Back);
    } else if (state.localSettings.menuPage == LocalMenuPage::Controls) {
        page.title = "Controls";
        page.tablePage = true;
        addPhoneMenuSection(page, "Movement");
        addPhoneMenuValue(page, "Forward", phoneMenuKeyName(state.localSettings.keyboardBindings[0]), PhoneMenuAction::Rebind, 0);
        addPhoneMenuValue(page, "Back", phoneMenuKeyName(state.localSettings.keyboardBindings[1]), PhoneMenuAction::Rebind, 1);
        addPhoneMenuValue(page, "Left", phoneMenuKeyName(state.localSettings.keyboardBindings[2]), PhoneMenuAction::Rebind, 2);
        addPhoneMenuValue(page, "Right", phoneMenuKeyName(state.localSettings.keyboardBindings[3]), PhoneMenuAction::Rebind, 3);
        addPhoneMenuValue(page, "Run", phoneMenuKeyName(state.localSettings.keyboardBindings[4]), PhoneMenuAction::Rebind, 4);
        addPhoneMenuValue(page, "Jump", phoneMenuKeyName(state.localSettings.keyboardBindings[5]), PhoneMenuAction::Rebind, 5);
        addPhoneMenuSection(page, "Actions");
        addPhoneMenuValue(page, "Attack", phoneMenuKeyName(state.localSettings.keyboardBindings[6]), PhoneMenuAction::Rebind, 6);
        addPhoneMenuValue(page, "Shoot", phoneMenuKeyName(state.localSettings.keyboardBindings[7]), PhoneMenuAction::Rebind, 7);
        addPhoneMenuValue(page, "Camera", phoneMenuKeyName(state.localSettings.keyboardBindings[8]), PhoneMenuAction::Rebind, 8);
        addPhoneMenuSection(page, "Look");
        addPhoneMenuValue(page, "Mouse", std::to_string(phoneMenuPercent(state.localSettings.mouseLookSensitivity)) + "%", PhoneMenuAction::AdjustMouse);
        addPhoneMenuValue(page, "Controller", std::to_string(phoneMenuPercent(state.localSettings.controllerLookSensitivity)) + "%", PhoneMenuAction::AdjustController);
        addPhoneMenuValue(page, "Triggers", phoneMenuTriggerSensitivityName(state.localSettings.controllerTriggerSensitivity), PhoneMenuAction::AdjustTriggers);
        addPhoneMenuValue(page, "Vibration", phoneMenuVibrationName(state.localSettings.controllerVibration), PhoneMenuAction::AdjustVibration);
        addPhoneMenuItem(page, "Defaults", PhoneMenuAction::Defaults);
        addPhoneMenuItem(page, "Back", PhoneMenuAction::Back);
    } else if (state.localSettings.menuPage == LocalMenuPage::Audio) {
        page.title = "Audio";
        page.tablePage = true;
        addPhoneMenuValue(page, "Music", std::to_string(phoneMenuPercent(state.localSettings.musicVolume)) + "%", PhoneMenuAction::MusicVolume);
        addPhoneMenuValue(page, "SFX", std::to_string(phoneMenuPercent(state.localSettings.sfxVolume)) + "%", PhoneMenuAction::SfxVolume);
        addPhoneMenuToggle(page, state.localSettings.musicMuted ? "Music On" : "Music Mute", PhoneMenuAction::MusicMute);
        addPhoneMenuToggle(page, state.localSettings.sfxMuted ? "SFX On" : "SFX Mute", PhoneMenuAction::SfxMute);
        addPhoneMenuItem(page, "Back", PhoneMenuAction::Back);
    } else {
        const char* presets[] = {"Legacy", "Normal", "Pretty"};
        page.title = "Graphics";
        page.tablePage = true;
        addPhoneMenuValue(page, "Preset", presets[std::max(0, std::min(2, state.localSettings.graphicsPreset))], PhoneMenuAction::GraphicsPreset);
        addPhoneMenuToggle(page, state.localSettings.shadows ? "Shadows On" : "Shadows Off", PhoneMenuAction::ToggleShadows);
        addPhoneMenuToggle(page, state.localSettings.particles ? "Particles On" : "Particles Off", PhoneMenuAction::ToggleParticles);
        addPhoneMenuToggle(page, state.localSettings.fpsCounter ? "FPS On" : "FPS Off", PhoneMenuAction::ToggleFps);
        addPhoneMenuItem(page, "Back", PhoneMenuAction::Back);
    }
    return page;
}

inline const PhoneMenuElement* phoneMenuElementForSelection(const PhoneMenuPageViewModel& page, int selection) {
    int selectableIndex = 0;
    for (int i = 0; i < page.elementCount; ++i) {
        const PhoneMenuElement& element = page.elements[i];
        if (!element.selectable) continue;
        if (selectableIndex == selection) return &element;
        ++selectableIndex;
    }
    return nullptr;
}
