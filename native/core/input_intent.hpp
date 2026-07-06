#pragma once

namespace db {

struct InputIntent {
    float moveX = 0.0f;
    float moveZ = 0.0f;
    float lookX = 0.0f;
    float lookY = 0.0f;

    bool jump = false;
    bool sprint = false;
    bool vacuum = false;
    bool attack = false;
    bool discharge = false;
    bool switchMode = false;
    bool toggleCamera = false;
};

} // namespace db
