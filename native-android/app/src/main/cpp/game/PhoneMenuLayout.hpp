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
    Defaults,
    NextControls,
    PreviousControls,
    MusicVolume,
    SfxVolume,
    MusicMute,
    SfxMute,
    GraphicsPreset,
    ToggleShadows,
    ToggleParticles,
    ToggleFps,
    Restart
};

struct PhoneMenuTokens {
    float horizontalSafeMargin = 0.11f;
    float topSafeMargin = 0.095f;
    float bottomSafeMargin = 0.095f;
    float titleZone = 0.15f;
    float footerZone = 0.12f;
    float panelInset = 0.04f;
    float titleScale = 0.0225f;
    float itemScale = 0.0148f;
    float metadataScale = 0.0098f;
    float sectionScale = 0.0082f;
    float majorSpacing = 0.030f;
    float rowSpacing = 0.092f;
    float denseRowSpacing = 0.066f;
    float rowHeight = 0.062f;
    float denseRowHeight = 0.044f;
    float dividerThickness = 0.004f;
    float selectedFillOpacity = 0.0f;
    float inactiveFillOpacity = 0.0f;
    float inactiveTextIntensity = 0.72f;
    float activeTextIntensity = 0.96f;
};

struct PhoneMenuRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct PhoneMenuRow {
    PhoneMenuRowKind kind = PhoneMenuRowKind::Item;
    PhoneMenuAction action = PhoneMenuAction::None;
    int selectableIndex = -1;
    int bindingAction = -1;
    std::string label;
    std::string value;
    PhoneMenuRect visual;
    PhoneMenuRect hit;
    float labelX = 0.0f;
    float valueRightX = 0.0f;
    float textY = 0.0f;
    float scale = 0.0f;
    bool selectable = false;
};

struct PhoneMenuLayout {
    static constexpr int MaxRows = 16;
    PhoneMenuTokens tokens;
    float screenW = PHONE_SCREEN_WIDTH;
    float screenH = PHONE_SCREEN_HEIGHT;
    PhoneMenuRect panel;
    PhoneMenuRect safe;
    PhoneMenuRect header;
    PhoneMenuRect content;
    PhoneMenuRect footer;
    std::string title;
    float titleY = 0.0f;
    float titleScale = 0.0f;
    float labelX = 0.0f;
    float valueRightX = 0.0f;
    std::array<PhoneMenuRow, MaxRows> rows{};
    int rowCount = 0;
    int selectableCount = 0;
    bool paletteTitle = false;
    bool joinCode = false;
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

inline float phoneMenuFitScale(float preferred, float maxWidth, const std::string& text) {
    if (text.empty()) return preferred;
    return std::min(preferred, maxWidth / (static_cast<float>(text.size()) * 6.0f));
}

inline void addPhoneMenuRow(PhoneMenuLayout& layout, PhoneMenuRow row) {
    if (layout.rowCount >= PhoneMenuLayout::MaxRows) return;
    if (row.selectable) row.selectableIndex = layout.selectableCount++;
    layout.rows[layout.rowCount++] = row;
}

inline void addPhoneMenuSection(PhoneMenuLayout& layout, const std::string& label) {
    PhoneMenuRow row;
    row.kind = PhoneMenuRowKind::Section;
    row.label = label;
    row.scale = layout.screenW * layout.tokens.sectionScale;
    addPhoneMenuRow(layout, row);
}

inline void addPhoneMenuItem(PhoneMenuLayout& layout, const std::string& label, PhoneMenuAction action, int bindingAction = -1) {
    PhoneMenuRow row;
    row.kind = PhoneMenuRowKind::Item;
    row.action = action;
    row.bindingAction = bindingAction;
    row.label = label;
    row.scale = layout.screenW * layout.tokens.itemScale;
    row.selectable = true;
    addPhoneMenuRow(layout, row);
}

inline void addPhoneMenuValue(PhoneMenuLayout& layout, const std::string& label, const std::string& value, PhoneMenuAction action, int bindingAction = -1) {
    PhoneMenuRow row;
    row.kind = PhoneMenuRowKind::TwoColumn;
    row.action = action;
    row.bindingAction = bindingAction;
    row.label = label;
    row.value = value;
    row.scale = layout.screenW * layout.tokens.metadataScale;
    row.selectable = true;
    addPhoneMenuRow(layout, row);
}

inline void finishPhoneMenuRows(PhoneMenuLayout& layout, bool tablePage) {
    if (layout.rowCount <= 0) return;
    const float rowStep = layout.screenH * (tablePage ? layout.tokens.denseRowSpacing : layout.tokens.rowSpacing);
    const float rowH = layout.screenH * (tablePage ? layout.tokens.denseRowHeight : layout.tokens.rowHeight);
    const float maxBlockH = layout.content.h;
    const float usableStep = layout.rowCount > 1 ? std::min(rowStep, (maxBlockH - rowH) / static_cast<float>(layout.rowCount - 1)) : rowStep;
    const float usedBlockH = usableStep * static_cast<float>(layout.rowCount - 1) + rowH;
    const float firstY = layout.content.y + usedBlockH * 0.5f - rowH * 0.50f;
    for (int i = 0; i < layout.rowCount; ++i) {
        PhoneMenuRow& row = layout.rows[i];
        const float y = firstY - static_cast<float>(i) * usableStep;
        row.visual = {0.0f, y - rowH * 0.34f, layout.safe.w, rowH};
        row.hit = row.visual;
        row.textY = y;
        row.labelX = tablePage ? layout.labelX : 0.0f;
        row.valueRightX = layout.valueRightX;
        if (tablePage && row.kind == PhoneMenuRowKind::Item) {
            row.scale = phoneMenuFitScale(layout.screenW * layout.tokens.metadataScale, layout.safe.w * 0.74f, row.label);
        } else if (row.kind == PhoneMenuRowKind::TwoColumn) {
            const int totalChars = static_cast<int>(row.label.size() + row.value.size() + 1u);
            row.scale = phoneMenuFitScale(row.scale, layout.safe.w * 0.92f, std::string(static_cast<std::size_t>(std::max(1, totalChars)), 'X'));
        }
        if (row.kind == PhoneMenuRowKind::Section) {
            row.visual.w = layout.safe.w * 0.92f;
            row.hit.w = 0.0f;
            row.hit.h = 0.0f;
        }
    }
}

inline PhoneMenuLayout makePhoneMenuLayout(const GameState& state) {
    PhoneMenuLayout layout;
    layout.screenW = PHONE_SCREEN_WIDTH * std::max(0.65f, state.phoneVisual.screenScale.x);
    layout.screenH = PHONE_SCREEN_HEIGHT * std::max(0.65f, state.phoneVisual.screenScale.y);
    layout.panel = {0.0f, 0.0f, layout.screenW * (1.0f - layout.tokens.panelInset), layout.screenH * (1.0f - layout.tokens.panelInset)};
    layout.safe = {
        0.0f,
        (layout.tokens.bottomSafeMargin - layout.tokens.topSafeMargin) * layout.screenH * 0.5f,
        layout.screenW * (1.0f - layout.tokens.horizontalSafeMargin * 2.0f),
        layout.screenH * (1.0f - layout.tokens.topSafeMargin - layout.tokens.bottomSafeMargin)
    };
    const float safeTop = layout.safe.y + layout.safe.h * 0.5f;
    const float headerH = layout.screenH * layout.tokens.titleZone;
    const float footerH = layout.screenH * layout.tokens.footerZone;
    layout.header = {0.0f, safeTop - headerH * 0.5f, layout.safe.w, headerH};
    layout.footer = {0.0f, layout.safe.y - layout.safe.h * 0.5f + footerH * 0.5f, layout.safe.w, footerH};
    layout.content = {0.0f, layout.safe.y - (headerH - footerH) * 0.5f, layout.safe.w, layout.safe.h - headerH - footerH};
    layout.titleY = layout.header.y - layout.header.h * 0.14f;
    layout.titleScale = layout.screenW * layout.tokens.titleScale;
    layout.labelX = -layout.safe.w * 0.46f;
    layout.valueRightX = layout.safe.w * 0.46f;

    const bool pausedSolo = state.started && state.uiPaused && !state.multiplayer.enabled && !state.upgradeMenu.active;
    if (state.cinematic.introActive) {
        layout.title = "DATA"; layout.paletteTitle = true; addPhoneMenuItem(layout, "Start", PhoneMenuAction::Start);
    } else if (state.dead) {
        layout.title = ""; addPhoneMenuItem(layout, "Restart", PhoneMenuAction::Restart); addPhoneMenuItem(layout, "Exit", PhoneMenuAction::Exit);
    } else if (pausedSolo && state.localSettings.menuPage == LocalMenuPage::Main) {
        layout.title = "PAUSED";
        addPhoneMenuItem(layout, "Resume", PhoneMenuAction::Resume);
        addPhoneMenuItem(layout, "Controls", PhoneMenuAction::Controls);
        addPhoneMenuItem(layout, "Audio", PhoneMenuAction::Audio);
        addPhoneMenuItem(layout, "Graphics", PhoneMenuAction::Graphics);
        addPhoneMenuItem(layout, "Exit Run", PhoneMenuAction::ExitRun);
    } else if (state.localSettings.menuPage == LocalMenuPage::Main) {
        layout.title = "DATA"; layout.paletteTitle = true;
        addPhoneMenuItem(layout, "Solo", PhoneMenuAction::Solo);
        addPhoneMenuItem(layout, "Online", PhoneMenuAction::Online);
        addPhoneMenuItem(layout, "Settings", PhoneMenuAction::Settings);
        addPhoneMenuItem(layout, "Exit", PhoneMenuAction::Exit);
    } else if (state.localSettings.menuPage == LocalMenuPage::Online) {
        layout.title = "Online";
        addPhoneMenuItem(layout, "Host", PhoneMenuAction::Host);
        addPhoneMenuItem(layout, "Join", PhoneMenuAction::Join);
        addPhoneMenuItem(layout, "Back", PhoneMenuAction::Back);
    } else if (state.localSettings.menuPage == LocalMenuPage::JoinCode) {
        layout.title = "Enter Code"; layout.joinCode = true;
    } else if (state.localSettings.menuPage == LocalMenuPage::Settings) {
        layout.title = "Settings";
        addPhoneMenuItem(layout, "Controls", PhoneMenuAction::Controls);
        addPhoneMenuItem(layout, "Audio", PhoneMenuAction::Audio);
        addPhoneMenuItem(layout, "Graphics", PhoneMenuAction::Graphics);
        addPhoneMenuItem(layout, "Back", PhoneMenuAction::Back);
    } else if (state.localSettings.menuPage == LocalMenuPage::Controls) {
        const bool pageTwo = state.localSettings.controlsPage != 0;
        layout.title = pageTwo ? "Controls 2/2" : "Controls 1/2";
        if (!pageTwo) {
            addPhoneMenuSection(layout, "Move");
            addPhoneMenuValue(layout, "Forward", phoneMenuKeyName(state.localSettings.keyboardBindings[0]), PhoneMenuAction::Rebind, 0);
            addPhoneMenuValue(layout, "Back", phoneMenuKeyName(state.localSettings.keyboardBindings[1]), PhoneMenuAction::Rebind, 1);
            addPhoneMenuValue(layout, "Left", phoneMenuKeyName(state.localSettings.keyboardBindings[2]), PhoneMenuAction::Rebind, 2);
            addPhoneMenuValue(layout, "Right", phoneMenuKeyName(state.localSettings.keyboardBindings[3]), PhoneMenuAction::Rebind, 3);
            addPhoneMenuValue(layout, "Run", phoneMenuKeyName(state.localSettings.keyboardBindings[4]), PhoneMenuAction::Rebind, 4);
            addPhoneMenuValue(layout, "Jump", phoneMenuKeyName(state.localSettings.keyboardBindings[5]), PhoneMenuAction::Rebind, 5);
            addPhoneMenuItem(layout, "Next", PhoneMenuAction::NextControls);
            addPhoneMenuItem(layout, "Back", PhoneMenuAction::Back);
        } else {
            addPhoneMenuSection(layout, "Action");
            addPhoneMenuValue(layout, "Lunge", phoneMenuKeyName(state.localSettings.keyboardBindings[6]), PhoneMenuAction::Rebind, 6);
            addPhoneMenuValue(layout, "Shoot", phoneMenuKeyName(state.localSettings.keyboardBindings[7]), PhoneMenuAction::Rebind, 7);
            addPhoneMenuValue(layout, "Camera", phoneMenuKeyName(state.localSettings.keyboardBindings[8]), PhoneMenuAction::Rebind, 8);
            addPhoneMenuValue(layout, "Alternate", phoneMenuKeyName(state.localSettings.keyboardBindings[9]), PhoneMenuAction::Rebind, 9);
            addPhoneMenuSection(layout, "Look");
            addPhoneMenuValue(layout, "Mouse", std::to_string(phoneMenuPercent(state.localSettings.mouseLookSensitivity)) + "%", PhoneMenuAction::AdjustMouse);
            addPhoneMenuValue(layout, "Controller", std::to_string(phoneMenuPercent(state.localSettings.controllerLookSensitivity)) + "%", PhoneMenuAction::AdjustController);
            addPhoneMenuItem(layout, "Previous", PhoneMenuAction::PreviousControls);
            addPhoneMenuItem(layout, "Defaults", PhoneMenuAction::Defaults);
            addPhoneMenuItem(layout, "Back", PhoneMenuAction::Back);
        }
    } else if (state.localSettings.menuPage == LocalMenuPage::Audio) {
        layout.title = "Audio";
        addPhoneMenuValue(layout, "Music", std::to_string(phoneMenuPercent(state.localSettings.musicVolume)) + "%", PhoneMenuAction::MusicVolume);
        addPhoneMenuValue(layout, "SFX", std::to_string(phoneMenuPercent(state.localSettings.sfxVolume)) + "%", PhoneMenuAction::SfxVolume);
        addPhoneMenuItem(layout, state.localSettings.musicMuted ? "Music On" : "Music Mute", PhoneMenuAction::MusicMute);
        addPhoneMenuItem(layout, state.localSettings.sfxMuted ? "SFX On" : "SFX Mute", PhoneMenuAction::SfxMute);
        addPhoneMenuItem(layout, "Back", PhoneMenuAction::Back);
    } else {
        const char* presets[] = {"Legacy", "Normal", "Pretty"};
        layout.title = "Graphics";
        addPhoneMenuValue(layout, "Preset", presets[std::max(0, std::min(2, state.localSettings.graphicsPreset))], PhoneMenuAction::GraphicsPreset);
        addPhoneMenuItem(layout, state.localSettings.shadows ? "Shadows On" : "Shadows Off", PhoneMenuAction::ToggleShadows);
        addPhoneMenuItem(layout, state.localSettings.particles ? "Particles On" : "Particles Off", PhoneMenuAction::ToggleParticles);
        addPhoneMenuItem(layout, state.localSettings.fpsCounter ? "FPS On" : "FPS Off", PhoneMenuAction::ToggleFps);
        addPhoneMenuItem(layout, "Back", PhoneMenuAction::Back);
    }

    layout.titleScale = phoneMenuFitScale(layout.titleScale, layout.safe.w * 0.94f, layout.title);
    const bool tablePage = state.localSettings.menuPage == LocalMenuPage::Controls ||
        state.localSettings.menuPage == LocalMenuPage::Audio ||
        state.localSettings.menuPage == LocalMenuPage::Graphics;
    finishPhoneMenuRows(layout, tablePage);
    return layout;
}

inline const PhoneMenuRow* phoneMenuRowForSelection(const PhoneMenuLayout& layout, int selection) {
    for (int i = 0; i < layout.rowCount; ++i) {
        if (layout.rows[i].selectable && layout.rows[i].selectableIndex == selection) return &layout.rows[i];
    }
    return nullptr;
}
