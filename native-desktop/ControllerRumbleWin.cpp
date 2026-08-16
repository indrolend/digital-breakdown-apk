#include "ControllerRumble.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>

#include <algorithm>
#include <chrono>

namespace {
int activeController = -1;
std::chrono::steady_clock::time_point stopAt;

int connectedController() {
    XINPUT_STATE state{};
    if (activeController >= 0 && XInputGetState(activeController, &state) == ERROR_SUCCESS) return activeController;
    for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
        if (XInputGetState(index, &state) == ERROR_SUCCESS) return static_cast<int>(index);
    }
    return -1;
}
}

void controllerRumblePulse(float lowFrequency, float highFrequency, int durationMilliseconds) {
    activeController = connectedController();
    if (activeController < 0) return;
    XINPUT_VIBRATION vibration{};
    vibration.wLeftMotorSpeed = static_cast<WORD>(std::clamp(lowFrequency, 0.0f, 1.0f) * 65535.0f);
    vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(highFrequency, 0.0f, 1.0f) * 65535.0f);
    XInputSetState(activeController, &vibration);
    stopAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(1, durationMilliseconds));
}

void controllerRumbleUpdate() {
    if (activeController < 0 || std::chrono::steady_clock::now() < stopAt) return;
    controllerRumbleStop();
}

void controllerRumbleStop() {
    if (activeController >= 0) {
        XINPUT_VIBRATION vibration{};
        XInputSetState(activeController, &vibration);
    }
    activeController = -1;
}

// Windows does not expose a general-purpose Precision Touchpad haptics API.
void touchpadHapticPulse(TouchpadHapticEffect, int) {}
