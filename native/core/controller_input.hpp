#pragma once

#include <cmath>

#include "input_intent.hpp"

namespace db {

constexpr int CONTROLLER_AXIS_COUNT = 8;
constexpr int CONTROLLER_BUTTON_COUNT = 16;

enum ControllerAxis {
    ControllerAxis_LeftX = 0,
    ControllerAxis_LeftY = 1,
    ControllerAxis_RightX = 2,
    ControllerAxis_RightY = 3,
    ControllerAxis_LeftTrigger = 4,
    ControllerAxis_RightTrigger = 5
};

enum ControllerButton {
    ControllerButton_South = 0,      // Xbox A / PlayStation Cross: jump
    ControllerButton_East = 1,       // Xbox B / PlayStation Circle: reserved/back
    ControllerButton_West = 2,       // Xbox X / PlayStation Square: melee attack
    ControllerButton_North = 3,      // Xbox Y / PlayStation Triangle: switch mode
    ControllerButton_LeftBumper = 4, // sprint
    ControllerButton_RightBumper = 5,// discharge / shoot
    ControllerButton_Back = 6,       // toggle camera
    ControllerButton_Start = 7,
    ControllerButton_LeftStick = 8,  // sprint alternate
    ControllerButton_RightStick = 9
};

struct ControllerSnapshot {
    bool connected = false;
    float axes[CONTROLLER_AXIS_COUNT] = {};
    bool buttons[CONTROLLER_BUTTON_COUNT] = {};
};

struct ControllerMapSettings {
    float moveDeadzone = 0.18f;
    float lookDeadzone = 0.12f;
    float triggerThreshold = 0.35f;
    float lookScale = 3.2f;
    bool invertLookY = false;
};

struct ControllerMapperState {
    bool previousButtons[CONTROLLER_BUTTON_COUNT] = {};
};

inline float clampInput(float v) {
    if (v < -1.0f) return -1.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

inline float applySignedDeadzone(float v, float deadzone) {
    v = clampInput(v);
    if (std::fabs(v) <= deadzone) return 0.0f;

    const float sign = v < 0.0f ? -1.0f : 1.0f;
    const float mag = (std::fabs(v) - deadzone) / (1.0f - deadzone);
    return sign * clampInput(mag);
}

inline bool pressedThisFrame(const ControllerSnapshot& pad, ControllerMapperState& state, int button) {
    const bool now = button >= 0 && button < CONTROLLER_BUTTON_COUNT && pad.buttons[button];
    const bool before = button >= 0 && button < CONTROLLER_BUTTON_COUNT && state.previousButtons[button];
    if (button >= 0 && button < CONTROLLER_BUTTON_COUNT) state.previousButtons[button] = now;
    return now && !before;
}

inline InputIntent mapControllerToInputIntent(
    const ControllerSnapshot& pad,
    ControllerMapperState& state,
    const ControllerMapSettings& settings = ControllerMapSettings{}
) {
    InputIntent input;

    if (!pad.connected) {
        for (int i = 0; i < CONTROLLER_BUTTON_COUNT; ++i) state.previousButtons[i] = false;
        return input;
    }

    input.moveX = applySignedDeadzone(pad.axes[ControllerAxis_LeftX], settings.moveDeadzone);
    input.moveZ = -applySignedDeadzone(pad.axes[ControllerAxis_LeftY], settings.moveDeadzone);

    input.lookX = applySignedDeadzone(pad.axes[ControllerAxis_RightX], settings.lookDeadzone) * settings.lookScale;
    const float lookY = applySignedDeadzone(pad.axes[ControllerAxis_RightY], settings.lookDeadzone) * settings.lookScale;
    input.lookY = settings.invertLookY ? -lookY : lookY;

    const bool leftTrigger = pad.axes[ControllerAxis_LeftTrigger] >= settings.triggerThreshold;
    const bool rightTrigger = pad.axes[ControllerAxis_RightTrigger] >= settings.triggerThreshold;

    input.jump = pressedThisFrame(pad, state, ControllerButton_South);
    input.attack = pad.buttons[ControllerButton_West];
    input.sprint = pad.buttons[ControllerButton_LeftBumper] || pad.buttons[ControllerButton_LeftStick];
    input.vacuum = leftTrigger;
    input.discharge = rightTrigger || pad.buttons[ControllerButton_RightBumper];
    input.switchMode = pressedThisFrame(pad, state, ControllerButton_North);
    input.toggleCamera = pressedThisFrame(pad, state, ControllerButton_Back);

    for (int i = 0; i < CONTROLLER_BUTTON_COUNT; ++i) {
        if (i == ControllerButton_South || i == ControllerButton_North || i == ControllerButton_Back) continue;
        state.previousButtons[i] = pad.buttons[i];
    }

    return input;
}

} // namespace db
