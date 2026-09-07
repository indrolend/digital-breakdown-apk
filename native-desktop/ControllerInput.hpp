#pragma once

struct NativeControllerSnapshot {
    int id = -1;
    float leftX = 0.0f;
    float leftY = 0.0f;
    float rightX = 0.0f;
    float rightY = 0.0f;
    float leftTrigger = -1.0f;
    float rightTrigger = -1.0f;
    bool a = false;
    bool b = false;
    bool x = false;
    bool y = false;
    bool leftBumper = false;
    bool rightBumper = false;
    bool back = false;
    bool start = false;
    bool leftThumb = false;
    bool rightThumb = false;
    bool dpadUp = false;
    bool dpadRight = false;
    bool dpadDown = false;
    bool dpadLeft = false;
};

bool readNativeController(NativeControllerSnapshot& snapshot);
