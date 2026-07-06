#pragma once

#include "math.hpp"
#include "sim_constants.hpp"

namespace db {

struct PlayerState {
    Vec3 pos = {0.0f, 0.55f, 12.0f};
    Vec3 vel = {0.0f, 0.0f, 0.0f};
    float yaw = 3.14159265358979323846f;
    float targetYaw = 3.14159265358979323846f;
    float battery = 100.0f;
    int souls = 0;
    bool grounded = true;
    bool alive = true;
};

enum class TargetKind {
    Basic
};

enum class TargetLifecycle {
    Alive,
    Attracted,
    Latched,
    Captured,
    Stored,
    Dead
};

struct TargetState {
    Vec3 pos = {0.0f, 0.0f, 0.0f};
    Vec3 vel = {0.0f, 0.0f, 0.0f};
    TargetKind kind = TargetKind::Basic;
    TargetLifecycle lifecycle = TargetLifecycle::Alive;
    float captureProgress = 0.0f;
    float phase = 0.0f;
    bool alive = true;
};

struct CapturePointState {
    Vec3 pos = {0.0f, 0.0f, 0.0f};
    bool filled = false;
};

struct RoomState {
    int roomIndex = 1;
    bool clear = false;
    int requiredCaptures = 3;
};

struct WorldState {
    PlayerState player;
    TargetState targets[MAX_TARGETS];
    CapturePointState captures[MAX_CAPTURE_POINTS];
    int targetCount = 0;
    int captureCount = 0;
    RoomState room;
};

} // namespace db
