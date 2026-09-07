#include "ControllerInput.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>

#pragma comment(lib, "xinput9_1_0.lib")

namespace {
float signedAxis(SHORT value) {
    return value < 0 ? static_cast<float>(value) / 32768.0f : static_cast<float>(value) / 32767.0f;
}

bool pressed(WORD buttons, WORD mask) {
    return (buttons & mask) != 0;
}
}

bool readNativeController(NativeControllerSnapshot& snapshot) {
    snapshot = NativeControllerSnapshot{};
    for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
        XINPUT_STATE state{};
        if (XInputGetState(index, &state) != ERROR_SUCCESS) continue;
        const XINPUT_GAMEPAD& pad = state.Gamepad;
        snapshot.id = static_cast<int>(index);
        snapshot.leftX = signedAxis(pad.sThumbLX);
        snapshot.leftY = -signedAxis(pad.sThumbLY);
        snapshot.rightX = signedAxis(pad.sThumbRX);
        snapshot.rightY = -signedAxis(pad.sThumbRY);
        snapshot.leftTrigger = static_cast<float>(pad.bLeftTrigger) / 127.5f - 1.0f;
        snapshot.rightTrigger = static_cast<float>(pad.bRightTrigger) / 127.5f - 1.0f;
        snapshot.a = pressed(pad.wButtons, XINPUT_GAMEPAD_A);
        snapshot.b = pressed(pad.wButtons, XINPUT_GAMEPAD_B);
        snapshot.x = pressed(pad.wButtons, XINPUT_GAMEPAD_X);
        snapshot.y = pressed(pad.wButtons, XINPUT_GAMEPAD_Y);
        snapshot.leftBumper = pressed(pad.wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER);
        snapshot.rightBumper = pressed(pad.wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER);
        snapshot.back = pressed(pad.wButtons, XINPUT_GAMEPAD_BACK);
        snapshot.start = pressed(pad.wButtons, XINPUT_GAMEPAD_START);
        snapshot.leftThumb = pressed(pad.wButtons, XINPUT_GAMEPAD_LEFT_THUMB);
        snapshot.rightThumb = pressed(pad.wButtons, XINPUT_GAMEPAD_RIGHT_THUMB);
        snapshot.dpadUp = pressed(pad.wButtons, XINPUT_GAMEPAD_DPAD_UP);
        snapshot.dpadRight = pressed(pad.wButtons, XINPUT_GAMEPAD_DPAD_RIGHT);
        snapshot.dpadDown = pressed(pad.wButtons, XINPUT_GAMEPAD_DPAD_DOWN);
        snapshot.dpadLeft = pressed(pad.wButtons, XINPUT_GAMEPAD_DPAD_LEFT);
        return true;
    }
    return false;
}
