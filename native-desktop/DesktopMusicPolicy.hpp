#pragma once

enum class DesktopMusicMode {
    Silent,
    Menu,
    Gameplay,
    Flower,
    GameOver
};

constexpr DesktopMusicMode desktopMusicMode(bool started, bool attractMode, bool dead, bool flowerPower) noexcept {
    if (dead && !attractMode) return DesktopMusicMode::GameOver;
    if (!started || attractMode) return DesktopMusicMode::Menu;
    if (flowerPower) return DesktopMusicMode::Flower;
    return DesktopMusicMode::Gameplay;
}
