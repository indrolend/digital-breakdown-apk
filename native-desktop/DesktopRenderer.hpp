#pragma once

#include "Game.hpp"

class DesktopRenderer {
public:
    void resize(int width, int height);
    void draw(const GameState& state) const;

private:
    int width_ = 1280;
    int height_ = 720;

    static void drawBox(const Vec3& position, const Vec3& scale, float yaw, float r, float g, float b);
    static void drawGround();
    static void applyCamera(const GameState& state, float aspect);
};
