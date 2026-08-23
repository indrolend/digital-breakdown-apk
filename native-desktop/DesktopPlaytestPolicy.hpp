#pragma once

struct DesktopPlaytestPolicy {
    bool automation = false;

    constexpr bool clearInputOnFocusChange(bool focused) const noexcept {
        // Losing focus always releases held input. Automation activation may
        // race the first injected key, so its focus-gain callback must not
        // erase a newly latched press.
        return !focused || !automation;
    }
    constexpr bool releaseCaptureOnFocusLoss() const noexcept { return !automation; }
    constexpr bool acceptsRelativeMouseLook() const noexcept { return !automation; }
    constexpr bool allowsNetworkMode(bool networkRequested) const noexcept {
        return !automation || !networkRequested;
    }
};
