#include "DesktopRenderer.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr float ROOM_WIDTH = 30.0f;
constexpr float ROOM_DEPTH = 42.0f;
constexpr float PI = 3.14159265358979323846f;

Vec3 cross3(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float dot3(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

void perspective(float fovyDegrees, float aspect, float nearPlane, float farPlane) {
    const float top = nearPlane * std::tan(fovyDegrees * PI / 360.0f);
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
}

void lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 forward = normalized(center - eye);
    const Vec3 side = normalized(cross3(forward, up));
    const Vec3 correctedUp = cross3(side, forward);

    const float matrix[16] = {
        side.x, correctedUp.x, -forward.x, 0.0f,
        side.y, correctedUp.y, -forward.y, 0.0f,
        side.z, correctedUp.z, -forward.z, 0.0f,
        -dot3(side, eye), -dot3(correctedUp, eye), dot3(forward, eye), 1.0f
    };
    glMultMatrixf(matrix);
}

void cube() {
    glBegin(GL_QUADS);

    glVertex3f(-0.5f, -0.5f,  0.5f); glVertex3f( 0.5f, -0.5f,  0.5f); glVertex3f( 0.5f,  0.5f,  0.5f); glVertex3f(-0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f,  0.5f, -0.5f); glVertex3f( 0.5f,  0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, -0.5f,  0.5f); glVertex3f(-0.5f,  0.5f,  0.5f); glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f); glVertex3f( 0.5f, -0.5f, -0.5f); glVertex3f( 0.5f,  0.5f, -0.5f); glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f); glVertex3f( 0.5f,  0.5f,  0.5f); glVertex3f( 0.5f,  0.5f, -0.5f); glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f( 0.5f, -0.5f, -0.5f); glVertex3f( 0.5f, -0.5f,  0.5f); glVertex3f(-0.5f, -0.5f,  0.5f);

    glEnd();
}
}

void DesktopRenderer::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    glViewport(0, 0, width_, height_);
}

void DesktopRenderer::drawBox(const Vec3& position, const Vec3& scale, float yaw, float r, float g, float b) {
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);
    glRotatef(yaw * 180.0f / PI, 0.0f, 1.0f, 0.0f);
    glScalef(scale.x, scale.y, scale.z);
    glColor3f(r, g, b);
    cube();
    glPopMatrix();
}

void DesktopRenderer::drawGround() {
    drawBox({0.0f, -0.04f, 0.0f}, {ROOM_WIDTH, 0.06f, ROOM_DEPTH}, 0.0f, 0.02f, 0.12f, 0.035f);
    drawBox({0.0f, 3.5f, -ROOM_DEPTH * 0.5f}, {ROOM_WIDTH, 7.0f, 0.16f}, 0.0f, 0.03f, 0.22f, 0.06f);
    drawBox({-ROOM_WIDTH * 0.5f, 3.5f, 0.0f}, {0.16f, 7.0f, ROOM_DEPTH}, 0.0f, 0.03f, 0.22f, 0.06f);
    drawBox({ ROOM_WIDTH * 0.5f, 3.5f, 0.0f}, {0.16f, 7.0f, ROOM_DEPTH}, 0.0f, 0.03f, 0.22f, 0.06f);
}

void DesktopRenderer::applyCamera(const GameState& state, float aspect) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    perspective(62.0f, aspect, 0.05f, 90.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const Vec3 look = state.camera.firstPerson
        ? state.camera.pos + state.camera.forward * 10.0f
        : state.player.pos + state.camera.forward * 4.0f + Vec3{0.0f, 0.55f, 0.0f};
    lookAt(state.camera.pos, look, {0.0f, 1.0f, 0.0f});
}

void DesktopRenderer::draw(const GameState& state) const {
    const float pulse = 0.5f + 0.5f * std::sin(state.time * 2.0f);
    glClearColor(0.01f, 0.025f + pulse * 0.02f, 0.015f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);

    applyCamera(state, static_cast<float>(width_) / static_cast<float>(height_));
    drawGround();

    if (!state.camera.firstPerson) {
        drawBox(state.player.pos + Vec3{0.0f, 0.34f, 0.0f}, {0.85f, 1.35f, 0.16f}, state.player.yaw, 0.06f, 0.09f, 0.06f);
        drawBox(state.player.pos + Vec3{0.0f, 0.35f, -0.10f}, {0.62f, 0.92f, 0.035f}, state.player.yaw, 0.33f, 1.0f, 0.45f);
    }

    for (const auto& target : state.targets) {
        if (!target.alive) continue;
        const float wobble = 1.0f + std::sin(state.time * 7.0f + target.phase) * 0.04f;
        if (target.slurpable) {
            drawBox(target.pos + Vec3{0.0f, 0.62f, 0.0f}, {0.65f * target.scale * wobble, 1.25f * target.scale, 0.42f * target.scale * wobble}, 0.0f, 0.70f, 1.0f, 0.78f);
        } else {
            drawBox(target.pos + Vec3{0.0f, 0.62f, 0.0f}, {0.65f * target.scale * wobble, 1.25f * target.scale, 0.42f * target.scale * wobble}, 0.0f, 0.14f, 1.0f, 0.32f);
        }
    }

    for (const auto& capture : state.captures) {
        if (capture.filled) {
            drawBox(capture.pos, {1.55f, 1.55f, 0.18f}, 0.0f, 0.95f, 0.95f, 1.0f);
        } else {
            drawBox(capture.pos, {1.55f, 1.55f, 0.18f}, 0.0f, 0.18f, 0.34f, 1.0f);
        }
    }

    for (const auto& bullet : state.bullets) {
        if (!bullet.alive) continue;
        drawBox(bullet.pos, {0.18f, 0.18f, 0.18f}, 0.0f, 0.85f, 1.0f, 0.90f);
    }

    glFlush();
}
