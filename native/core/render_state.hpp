#pragma once

#include "math.hpp"
#include "sim_constants.hpp"
#include "world_state.hpp"

namespace db {

enum class RenderKind {
    Floor,
    Ceiling,
    Wall,
    DoorFrame,
    PlayerBody,
    PlayerScreen,
    TargetBody,
    TargetHead,
    CaptureFrame,
    CaptureHole,
    DepositedSoul
};

struct RenderBox {
    RenderKind kind = RenderKind::Wall;
    Vec3 pos = {0.0f, 0.0f, 0.0f};
    Vec3 size = {1.0f, 1.0f, 1.0f};
    float yaw = 0.0f;
    unsigned color = 0xffffffffu;
};

struct RenderFrame {
    static constexpr int MAX_BOXES = 96;
    RenderBox boxes[MAX_BOXES];
    int boxCount = 0;
};

void clearRenderFrame(RenderFrame& frame);
bool pushBox(RenderFrame& frame, RenderKind kind, Vec3 pos, Vec3 size, unsigned color, float yaw = 0.0f);
void buildRenderFrame(RenderFrame& frame, const WorldState& world, const SimConstants& c);

} // namespace db
