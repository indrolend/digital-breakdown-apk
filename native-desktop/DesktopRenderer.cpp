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
constexpr float ROOM_WALL_HEIGHT = 7.2f;
constexpr float GROUND_Y = 0.08f;
constexpr float PI = 3.14159265358979323846f;

Vec3 cross3(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float dot3(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
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
        side.x, correctedUp.x, -forward.x, 0,
        side.y, correctedUp.y, -forward.y, 0,
        side.z, correctedUp.z, -forward.z, 0,
        -dot3(side, eye), -dot3(correctedUp, eye), dot3(forward, eye), 1
    };
    glMultMatrixf(matrix);
}
void cube() {
    glBegin(GL_QUADS);
    glVertex3f(-.5f,-.5f,.5f); glVertex3f(.5f,-.5f,.5f); glVertex3f(.5f,.5f,.5f); glVertex3f(-.5f,.5f,.5f);
    glVertex3f(.5f,-.5f,-.5f); glVertex3f(-.5f,-.5f,-.5f); glVertex3f(-.5f,.5f,-.5f); glVertex3f(.5f,.5f,-.5f);
    glVertex3f(-.5f,-.5f,-.5f); glVertex3f(-.5f,-.5f,.5f); glVertex3f(-.5f,.5f,.5f); glVertex3f(-.5f,.5f,-.5f);
    glVertex3f(.5f,-.5f,.5f); glVertex3f(.5f,-.5f,-.5f); glVertex3f(.5f,.5f,-.5f); glVertex3f(.5f,.5f,.5f);
    glVertex3f(-.5f,.5f,.5f); glVertex3f(.5f,.5f,.5f); glVertex3f(.5f,.5f,-.5f); glVertex3f(-.5f,.5f,-.5f);
    glVertex3f(-.5f,-.5f,-.5f); glVertex3f(.5f,-.5f,-.5f); glVertex3f(.5f,-.5f,.5f); glVertex3f(-.5f,-.5f,.5f);
    glEnd();
}
}

void DesktopRenderer::resize(int width, int height) {
    width_ = std::max(1, width); height_ = std::max(1, height); glViewport(0, 0, width_, height_);
}

void DesktopRenderer::drawBox(const Vec3& p, const Vec3& s, float pitch, float yaw, float roll, float r, float g, float b) {
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(yaw * 180.0f / PI, 0, 1, 0);
    glRotatef(pitch * 180.0f / PI, 1, 0, 0);
    glRotatef(roll * 180.0f / PI, 0, 0, 1);
    glScalef(s.x, s.y, s.z); glColor3f(r,g,b); cube(); glPopMatrix();
}

void DesktopRenderer::drawRoomTile(const GameState& state, int tileIndex) {
    const float z0 = static_cast<float>(tileIndex) * ROOM_DEPTH;
    const float doorWidth = 5.35f;
    const float doorHeight = 3.95f;
    const float sideW = (ROOM_WIDTH - doorWidth) * 0.5f;
    const float sideX = doorWidth * 0.5f + sideW * 0.5f;
    const float topH = ROOM_WALL_HEIGHT - doorHeight;
    const float topY = doorHeight + topH * 0.5f;
    const float wallR = 0.39f, wallG = 0.43f, wallB = 0.46f;
    drawBox({0,-0.04f,z0},{ROOM_WIDTH,0.08f,ROOM_DEPTH},0,0,0,0.50f,0.55f,0.57f);
    drawBox({0,ROOM_WALL_HEIGHT+0.08f,z0},{ROOM_WIDTH,0.16f,ROOM_DEPTH},0,0,0,wallR,wallG,wallB);
    for (float seam : {-ROOM_DEPTH*0.5f, ROOM_DEPTH*0.5f}) {
        drawBox({-sideX,ROOM_WALL_HEIGHT*0.5f,z0+seam},{sideW,ROOM_WALL_HEIGHT,0.5f},0,0,0,wallR,wallG,wallB);
        drawBox({ sideX,ROOM_WALL_HEIGHT*0.5f,z0+seam},{sideW,ROOM_WALL_HEIGHT,0.5f},0,0,0,wallR,wallG,wallB);
        drawBox({0,topY,z0+seam},{doorWidth,topH,0.5f},0,0,0,wallR,wallG,wallB);
    }
    drawBox({-ROOM_WIDTH*0.5f,ROOM_WALL_HEIGHT*0.5f,z0},{0.5f,ROOM_WALL_HEIGHT,ROOM_DEPTH},0,0,0,wallR,wallG,wallB);
    drawBox({ ROOM_WIDTH*0.5f,ROOM_WALL_HEIGHT*0.5f,z0},{0.5f,ROOM_WALL_HEIGHT,ROOM_DEPTH},0,0,0,wallR,wallG,wallB);
    for (int i=0;i<state.debug.colliderCount;++i) {
        const RoomCollider& c=state.roomColliders[i];
        drawBox({c.center.x,c.center.y,z0+c.center.z},{c.width,c.height,c.depth},0,0,0,0.43f,0.49f,0.53f);
    }
}

void DesktopRenderer::applyCamera(const GameState& state, float aspect) {
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); perspective(75.0f, aspect, 0.1f, 1000.0f);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); lookAt(state.camera.pos, state.camera.lookTarget, {0,1,0});
}

void DesktopRenderer::draw(const GameState& state) const {
    glClearColor(0.031f,0.063f,0.094f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_LIGHTING);
    applyCamera(state, static_cast<float>(width_)/static_cast<float>(height_));
    for (int tile=state.topology.currentTileIndex-1; tile<=state.topology.currentTileIndex+1; ++tile) drawRoomTile(state,tile);

    if (!state.camera.firstPerson) {
        const Vec3 forward={-std::sin(state.player.yaw),0,-std::cos(state.player.yaw)};
        const Vec3 right={std::cos(state.player.yaw),0,-std::sin(state.player.yaw)};
        Vec3 phonePos=state.player.pos + Vec3{0,0.34f+state.phonePose.lift,0} + forward*state.phonePose.forward + right*state.phonePose.side;
        const float phoneYaw=state.player.yaw+state.phonePose.yaw;
        drawBox(phonePos,{0.85f,1.35f,0.16f},state.phonePose.pitch,phoneYaw,state.phonePose.roll,0.04f,0.05f,0.05f);
        drawBox(phonePos+forward*0.095f,{0.62f,0.92f,0.035f},state.phonePose.pitch,phoneYaw,state.phonePose.roll,0.16f,0.92f,0.35f);
    }

    const float tileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for (const auto& target:state.targets) if (target.alive) {
        Vec3 p=target.pos; p.z=tileOrigin + (target.pos.z - std::floor((target.pos.z+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);
        const float wobble=1.0f+std::sin(state.time*7.0f+target.phase)*0.04f;
        drawBox(p+Vec3{0,0.62f,0},{0.65f*wobble,1.25f,0.42f*wobble},0,0,0,target.slurpable?0.70f:0.14f,1.0f,target.slurpable?0.78f:0.32f);
    }
    for (const auto& capture:state.captures) {
        Vec3 p=capture.pos; p.z+=tileOrigin;
        drawBox(p,{0.72f,0.72f,0.06f},0,0,0,capture.filled?0.56f:0.18f,capture.filled?0.95f:0.34f,1.0f);
    }
    for (const auto& bullet:state.bullets) if (bullet.alive) drawBox(bullet.pos,{0.18f,0.18f,0.18f},0,0,0,0.85f,1.0f,0.90f);
    glFlush();
}
