#pragma once

#include "Game.hpp"

class DesktopRenderer {
public:
    static void drawBox(const Vec3& position, const Vec3& scale, float pitch, float yaw, float roll, float r, float g, float b);
    static void drawBox(const Vec3& position, const Vec3& scale, const Quat& orientation, float r, float g, float b);
    void resize(int width, int height);
    void draw(const GameState& state) const;

private:
    int width_ = 1280;
    int height_ = 720;

    static void drawRoomTile(const GameState& state, int tileIndex);
    static void applyCamera(const GameState& state, float aspect);
};
