#pragma once

struct DesktopPlaytestPolicy {
    bool automation = false;

    constexpr bool clearInputOnFocusChange() const noexcept { return true; }
    constexpr bool releaseCaptureOnFocusLoss() const noexcept { return !automation; }
    constexpr bool allowsNetworkMode(bool networkRequested) const noexcept {
        return !automation || !networkRequested;
    }
};
