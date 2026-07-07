#include "render_state.hpp"

namespace db {
namespace {

constexpr unsigned COLOR_FLOOR = 0xff9aa7ad;
constexpr unsigned COLOR_WALL = 0xffaeb8bd;
constexpr unsigned COLOR_DOOR = 0xff010203;
constexpr unsigned COLOR_PHONE = 0xffd0d0d0;
constexpr unsigned COLOR_SCREEN = 0xff12304a;
constexpr unsigned COLOR_TARGET = 0xffffe2b8;
constexpr unsigned COLOR_CAPTURE = 0xff8ff7ff;
constexpr unsigned COLOR_CAPTURE_HOLE = 0xff000000;
constexpr unsigned COLOR_SOUL = 0xff8ff7ff;

void addRoomBox(RenderFrame& frame, RenderKind kind, float width, float height, float depth, float x, float y, float z, unsigned color) {
    pushBox(frame, kind, Vec3{x, y, z}, Vec3{width, height, depth}, color);
}

void addRoomShell(RenderFrame& frame, const SimConstants& c) {
    const float floorTopY = c.groundY;
    addRoomBox(frame, RenderKind::Floor, c.roomWidth, 0.08f, c.roomDepth, 0.0f, floorTopY - 0.04f, 0.0f, COLOR_FLOOR);
    addRoomBox(frame, RenderKind::Ceiling, c.roomWidth, 0.16f, c.roomDepth, 0.0f, c.wallHeight + 0.08f, 0.0f, COLOR_WALL);

    const float doorOpeningWidth = 5.35f;
    const float doorOpeningHeight = 3.95f;
    const float sideWallWidth = (c.roomWidth - doorOpeningWidth) * 0.5f;
    const float sideWallX = doorOpeningWidth * 0.5f + sideWallWidth * 0.5f;
    const float topWallHeight = c.wallHeight - doorOpeningHeight;
    const float topWallY = doorOpeningHeight + topWallHeight * 0.5f;
    const float frontZ = c.roomDepth * 0.5f;
    const float backZ = -c.roomDepth * 0.5f;

    addRoomBox(frame, RenderKind::Wall, sideWallWidth, c.wallHeight, 0.5f, -sideWallX, c.wallHeight * 0.5f, frontZ, COLOR_WALL);
    addRoomBox(frame, RenderKind::Wall, sideWallWidth, c.wallHeight, 0.5f, sideWallX, c.wallHeight * 0.5f, frontZ, COLOR_WALL);
    addRoomBox(frame, RenderKind::DoorFrame, doorOpeningWidth, topWallHeight, 0.5f, 0.0f, topWallY, frontZ, COLOR_WALL);

    addRoomBox(frame, RenderKind::Wall, sideWallWidth, c.wallHeight, 0.5f, -sideWallX, c.wallHeight * 0.5f, backZ, COLOR_WALL);
    addRoomBox(frame, RenderKind::Wall, sideWallWidth, c.wallHeight, 0.5f, sideWallX, c.wallHeight * 0.5f, backZ, COLOR_WALL);
    addRoomBox(frame, RenderKind::DoorFrame, doorOpeningWidth, topWallHeight, 0.5f, 0.0f, topWallY, backZ, COLOR_WALL);

    addRoomBox(frame, RenderKind::Wall, 0.5f, c.wallHeight, c.roomDepth, -c.roomWidth * 0.5f, c.wallHeight * 0.5f, 0.0f, COLOR_WALL);
    addRoomBox(frame, RenderKind::Wall, 0.5f, c.wallHeight, c.roomDepth, c.roomWidth * 0.5f, c.wallHeight * 0.5f, 0.0f, COLOR_WALL);
}

void addPlayerPhone(RenderFrame& frame, const WorldState& world) {
    const Vec3 p = world.player.pos;
    pushBox(frame, RenderKind::PlayerBody, p, Vec3{0.80f, 1.60f, 0.12f}, COLOR_PHONE, world.player.yaw);
    pushBox(frame, RenderKind::PlayerScreen, Vec3{p.x, p.y + 0.01f, p.z - 0.07f}, Vec3{0.70f, 1.25f, 0.035f}, COLOR_SCREEN, world.player.yaw);
}

void addTargets(RenderFrame& frame, const WorldState& world) {
    for (int i = 0; i < world.targetCount; ++i) {
        const TargetState& t = world.targets[i];
        if (!t.alive) continue;
        const float pulse = t.lifecycle == TargetLifecycle::Latched ? 1.18f : 1.0f;
        pushBox(frame, RenderKind::TargetBody, Vec3{t.pos.x, 0.42f, t.pos.z}, Vec3{0.36f * pulse, 0.64f * pulse, 0.24f * pulse}, COLOR_TARGET);
        pushBox(frame, RenderKind::TargetHead, Vec3{t.pos.x, 0.86f, t.pos.z}, Vec3{0.28f * pulse, 0.28f * pulse, 0.28f * pulse}, COLOR_TARGET);
    }
}

void addCapturePoints(RenderFrame& frame, const WorldState& world) {
    for (int i = 0; i < world.captureCount; ++i) {
        const CapturePointState& cp = world.captures[i];
        const Vec3 base = {cp.pos.x, 3.05f, cp.pos.z};
        pushBox(frame, RenderKind::CaptureFrame, base, Vec3{0.72f, 0.72f, 0.06f}, COLOR_CAPTURE);
        pushBox(frame, RenderKind::CaptureHole, Vec3{base.x, base.y, base.z + 0.01f}, Vec3{0.52f, 0.52f, 0.08f}, COLOR_CAPTURE_HOLE);
        if (cp.filled) {
            pushBox(frame, RenderKind::DepositedSoul, Vec3{base.x, base.y, base.z + 0.12f}, Vec3{0.36f, 0.36f, 0.36f}, COLOR_SOUL);
        }
    }
}

} // namespace

void clearRenderFrame(RenderFrame& frame) {
    frame.boxCount = 0;
}

bool pushBox(RenderFrame& frame, RenderKind kind, Vec3 pos, Vec3 size, unsigned color, float yaw) {
    if (frame.boxCount >= RenderFrame::MAX_BOXES) return false;
    RenderBox& box = frame.boxes[frame.boxCount++];
    box.kind = kind;
    box.pos = pos;
    box.size = size;
    box.color = color;
    box.yaw = yaw;
    return true;
}

void buildRenderFrame(RenderFrame& frame, const WorldState& world, const SimConstants& c) {
    clearRenderFrame(frame);
    addRoomShell(frame, c);
    addCapturePoints(frame, world);
    addTargets(frame, world);
    addPlayerPhone(frame, world);
}

} // namespace db
