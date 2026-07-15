#pragma once

#include "../game/Game.hpp"

class Renderer {
public:
    void surfaceCreated();
    void surfaceChanged(int width, int height);
    void draw(const GameState& state);

private:
    int width_ = 1;
    int height_ = 1;
    unsigned int program_ = 0;
    unsigned int vbo_ = 0;
    unsigned int roundedVbo_ = 0;
    int roundedVertexCount_ = 0;
    int uMvp_ = -1;
    int uColor_ = -1;
    int aPos_ = -1;

    bool initProgram();
    void drawBox(const float* viewProj, const Vec3& pos, const Vec3& scale, float yaw, const float color[4]);
    void drawRoundedEllipsoid(const float* viewProj, const Vec3& pos, const Vec3& scale, float yaw, const float color[4]);
    void drawGround(const float* viewProj);
    void drawProceduralHuman(const float* viewProj, const TargetState& target, float time, const float color[4]);
};
