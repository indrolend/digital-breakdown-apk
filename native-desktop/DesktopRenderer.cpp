#include "DesktopRenderer.hpp"
#include "HumanVisual.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

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
    glNormal3f(0,0,1);
    glVertex3f(-.5f,-.5f,.5f); glVertex3f(.5f,-.5f,.5f); glVertex3f(.5f,.5f,.5f); glVertex3f(-.5f,.5f,.5f);
    glNormal3f(0,0,-1);
    glVertex3f(.5f,-.5f,-.5f); glVertex3f(-.5f,-.5f,-.5f); glVertex3f(-.5f,.5f,-.5f); glVertex3f(.5f,.5f,-.5f);
    glNormal3f(-1,0,0);
    glVertex3f(-.5f,-.5f,-.5f); glVertex3f(-.5f,-.5f,.5f); glVertex3f(-.5f,.5f,.5f); glVertex3f(-.5f,.5f,-.5f);
    glNormal3f(1,0,0);
    glVertex3f(.5f,-.5f,.5f); glVertex3f(.5f,-.5f,-.5f); glVertex3f(.5f,.5f,-.5f); glVertex3f(.5f,.5f,.5f);
    glNormal3f(0,1,0);
    glVertex3f(-.5f,.5f,.5f); glVertex3f(.5f,.5f,.5f); glVertex3f(.5f,.5f,-.5f); glVertex3f(-.5f,.5f,-.5f);
    glNormal3f(0,-1,0);
    glVertex3f(-.5f,-.5f,-.5f); glVertex3f(.5f,-.5f,-.5f); glVertex3f(.5f,-.5f,.5f); glVertex3f(-.5f,-.5f,.5f);
    glEnd();
}

void roundedEllipsoid(const Vec3& p, const Vec3& scale, float pitch, float yaw, float roll, float r, float g, float b) {
    constexpr int segments = 7;
    constexpr int rings = 5;
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(yaw * 180.0f / PI, 0, 1, 0);
    glRotatef(pitch * 180.0f / PI, 1, 0, 0);
    glRotatef(roll * 180.0f / PI, 0, 0, 1);
    glScalef(scale.x, scale.y, scale.z);
    glColor3f(r, g, b);
    auto vertex = [](int ring, int segment) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float phi = -PI * 0.5f + v * PI;
        const float u = static_cast<float>(segment) / static_cast<float>(segments);
        const float theta = u * PI * 2.0f;
        const float cp = std::cos(phi);
        const float x=std::cos(theta)*cp, y=std::sin(phi), z=std::sin(theta)*cp;
        glNormal3f(x,y,z); glVertex3f(x*0.5f,y*0.5f,z*0.5f);
    };
    glBegin(GL_TRIANGLES);
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            const int nextSeg = (seg + 1) % segments;
            vertex(ring, seg); vertex(ring + 1, seg); vertex(ring + 1, nextSeg);
            vertex(ring, seg); vertex(ring + 1, nextSeg); vertex(ring, nextSeg);
        }
    }
    glEnd();
    glPopMatrix();
}

void drawProceduralHumanDesktop(const TargetState& target, float time, float r, float g, float b) {
    const HumanVisualSpec& spec = PASS7_HUMAN_VISUAL_SPEC;
    const bool aliveHuman = !target.slurpable;
    const HumanVisualPose pose = makeHumanVisualPose(target.visualYaw, target.scale, time, target.visualReaction, aliveHuman);
    if (pose.scale <= 0.001f) return;
    const float s = pose.scale;
    const float collapseScale = std::max(0.18f, 1.0f - pose.collapse * 0.62f);
    const Vec3 root = target.pos + Vec3{0.0f, spec.rootGroundOffset + pose.rootBob, 0.0f};
    const float yaw = pose.yaw;
    const Vec3 forward{-std::sin(yaw),0,-std::cos(yaw)};
    const Vec3 right{std::cos(yaw),0,-std::sin(yaw)};
    const float footY=0.03f*s;
    const float shinY=footY+spec.footHeight*s*0.5f+spec.shinLength*s*0.5f;
    const float thighY=footY+spec.footHeight*s+spec.shinLength*s+spec.thighLength*s*0.5f;
    const float pelvisY=footY+spec.footHeight*s+spec.shinLength*s+spec.thighLength*s+spec.pelvisHeight*s*0.5f;
    const float torsoY=pelvisY+(spec.pelvisHeight+spec.torsoHeight)*s*0.5f;
    const float headY=spec.totalHeight*s-spec.headRadius*s;
    const float armY=torsoY+spec.torsoHeight*s*0.18f;
    roundedEllipsoid(root+Vec3{0,pelvisY*collapseScale,0},{spec.pelvisWidth*s,spec.pelvisHeight*s*collapseScale,spec.pelvisDepth*s},0,yaw,0,r,g,b);
    roundedEllipsoid(root+Vec3{0,torsoY*collapseScale,0}+forward*((pose.hitLean + pose.vacuumLean * 0.06f)*s),{spec.torsoWidth*s,spec.torsoHeight*s*collapseScale,spec.torsoDepth*s},pose.torsoPitch,yaw,pose.torsoRoll,r,g,b);
    roundedEllipsoid(root+Vec3{0,headY,0}+forward*(pose.headPitch*0.03f),{spec.headRadius*2*s,spec.headRadius*2*s,spec.headRadius*2*s},pose.headPitch,yaw,0,r,g,b);
    for (int side : {-1,1}) {
        const float armSwing=side<0?pose.leftArmSwing:pose.rightArmSwing;
        const float legSwing=side<0?pose.leftLegSwing:pose.rightLegSwing;
        const Vec3 shoulder=root+right*(side*spec.shoulderWidth*0.5f*s)+Vec3{0,armY,0};
        roundedEllipsoid(shoulder+forward*(armSwing*0.06f*s)+Vec3{0,-spec.upperArmLength*0.5f*s*collapseScale,0},{0.055f*s,spec.upperArmLength*s*collapseScale,0.065f*s},armSwing,yaw,0,r,g,b);
        roundedEllipsoid(shoulder+forward*(armSwing*0.11f*s)+Vec3{0,-(spec.upperArmLength+spec.forearmLength*0.5f)*s*collapseScale,0},{0.052f*s,spec.forearmLength*s*collapseScale,0.060f*s},armSwing*0.7f,yaw,0,r,g,b);
        roundedEllipsoid(shoulder+forward*(armSwing*0.14f*s)+Vec3{0,-(spec.upperArmLength+spec.forearmLength)*s,0},{spec.handSize*s,spec.handSize*s,spec.handSize*0.75f*s},0,yaw,0,r,g,b);
        const Vec3 hip=root+right*(side*spec.pelvisWidth*0.28f*s);
        roundedEllipsoid(hip+forward*(legSwing*0.05f*s)+Vec3{0,thighY*collapseScale,0},{0.075f*s,spec.thighLength*s*collapseScale,0.080f*s},legSwing,yaw,0,r,g,b);
        roundedEllipsoid(hip-forward*(legSwing*0.05f*s)+Vec3{0,shinY*collapseScale,0},{0.070f*s,spec.shinLength*s*collapseScale,0.075f*s},-legSwing*0.65f,yaw,0,r,g,b);
        roundedEllipsoid(hip+forward*(spec.footLength*0.25f*s+legSwing*0.04f*s)+Vec3{0,footY,0},{0.075f*s,spec.footHeight*s,spec.footLength*s},0,yaw,0,r,g,b);
    }
}

}

void DesktopRenderer::resize(int width, int height) {
    width_ = std::max(1, width); height_ = std::max(1, height); glViewport(0, 0, width_, height_);
}

void DesktopRenderer::drawBox(const Vec3& p, const Vec3& s, float pitch, float yaw, float roll, float r, float g, float b, float a) {
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(yaw * 180.0f / PI, 0, 1, 0);
    glRotatef(pitch * 180.0f / PI, 1, 0, 0);
    glRotatef(roll * 180.0f / PI, 0, 0, 1);
    glScalef(s.x, s.y, s.z); glColor4f(r,g,b,a); cube(); glPopMatrix();
}

void DesktopRenderer::drawBox(const Vec3& p, const Vec3& s, const Quat& q, float r, float g, float b) {
    const float matrix[16] = {
        1-2*(q.y*q.y+q.z*q.z), 2*(q.x*q.y+q.z*q.w), 2*(q.x*q.z-q.y*q.w), 0,
        2*(q.x*q.y-q.z*q.w), 1-2*(q.x*q.x+q.z*q.z), 2*(q.y*q.z+q.x*q.w), 0,
        2*(q.x*q.z+q.y*q.w), 2*(q.y*q.z-q.x*q.w), 1-2*(q.x*q.x+q.y*q.y), 0,
        0,0,0,1
    };
    glPushMatrix(); glTranslatef(p.x,p.y,p.z); glMultMatrixf(matrix); glScalef(s.x,s.y,s.z);
    glColor3f(r,g,b); cube(); glPopMatrix();
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
    glClearColor(Pass7Visual::Background.r,Pass7Visual::Background.g,Pass7Visual::Background.b,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_LIGHT1); glEnable(GL_COLOR_MATERIAL);
    const GLfloat ambient[]={0.30f,0.37f,0.40f,1.0f}; glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);
    const GLfloat sunDiffuse[]={1.0f,1.0f,1.0f,1.0f}, sunPos[]={30.0f,60.0f,25.0f,0.0f};
    glLightfv(GL_LIGHT0,GL_DIFFUSE,sunDiffuse); glLightfv(GL_LIGHT0,GL_POSITION,sunPos);
    const GLfloat fillDiffuse[]={0.20f,0.28f,0.35f,1.0f}, fillPos[]={-20.0f,25.0f,-30.0f,0.0f};
    glLightfv(GL_LIGHT1,GL_DIFFUSE,fillDiffuse); glLightfv(GL_LIGHT1,GL_POSITION,fillPos);
    glEnable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_LIGHTING); glEnable(GL_NORMALIZE);
    applyCamera(state, static_cast<float>(width_)/static_cast<float>(height_));
    for (int tile=state.topology.currentTileIndex-1; tile<=state.topology.currentTileIndex+1; ++tile) drawRoomTile(state,tile);

    if (!state.camera.firstPerson) {
        const Vec3 forward={-std::sin(state.player.yaw),0,-std::cos(state.player.yaw)};
        const Vec3 right={std::cos(state.player.yaw),0,-std::sin(state.player.yaw)};
        Vec3 phonePos=state.player.pos + Vec3{0,state.phonePose.lift+state.phoneVisual.actionLift,0} + forward*(state.phonePose.forward+state.phoneVisual.actionForward) + right*state.phonePose.side;
        const auto& pv=state.phoneVisual;
        Quat phoneOrientation=state.phonePose.orientation * quatAxisAngle({1,0,0},pv.pitch) * quatAxisAngle({0,0,1},pv.roll);
        drawBox(phonePos,{PHONE_BODY_WIDTH*pv.bodyScale.x,PHONE_BODY_HEIGHT*pv.bodyScale.y,PHONE_BODY_DEPTH},phoneOrientation,Pass7Visual::PhoneBody.r,Pass7Visual::PhoneBody.g,Pass7Visual::PhoneBody.b);
        const float glow=std::min(1.0f,0.45f+pv.screenGlow*0.36f);
        drawBox(phonePos+rotate(phoneOrientation,{0,0,PHONE_SCREEN_Z_OFFSET+pv.screenOffset}),{PHONE_SCREEN_WIDTH*pv.screenScale.x,PHONE_SCREEN_HEIGHT*pv.screenScale.y,PHONE_SCREEN_DEPTH},phoneOrientation,Pass7Visual::PhoneEmission.r*glow,Pass7Visual::PhoneEmission.g*glow,Pass7Visual::PhoneEmission.b*glow);
    }

    const MeleeVisualState& melee=state.meleeVisual;
    if(melee.visualTimer>0.0f){
        const float t=1.0f-clampf(melee.visualTimer/std::max(0.001f,melee.visualDuration),0.0f,1.0f);
        const float fade=1.0f-t, hitBoost=melee.visualHit?1.25f:0.72f;
        const Vec3 side{melee.direction.z,0,-melee.direction.x};
        glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        for(int segment=0;segment<9;++segment){
            const float a0=(-0.28f+t*1.15f)*PI + static_cast<float>(segment)*PI*1.35f/9.0f;
            const Vec3 radial=side*std::cos(a0)+Vec3{0,1,0}*std::sin(a0);
            const Vec3 p=melee.origin+melee.direction*(0.66f+0.22f*t)+radial*(0.52f*(0.75f+t*0.72f)*hitBoost);
            drawBox(p,{0.09f,0.035f,0.035f},0,state.player.yaw,a0,0.56f,0.97f,1.0f,fade*0.72f);
        }
        const Vec3 delta=melee.impact-melee.origin; const float len=std::max(0.3f,length(delta));
        const Vec3 mid=melee.origin+delta*0.46f; const float yaw=std::atan2(delta.x,delta.z); const float pitch=-std::asin(clampf(delta.y/std::max(len,0.001f),-1.0f,1.0f));
        drawBox(mid,{0.07f,0.07f,len*(0.70f+std::sin(t*PI)*0.18f)},pitch,yaw,0,0.33f,0.84f,1.0f,fade*0.42f);
        for(int segment=0;segment<12;++segment){const float a=segment*PI*2.0f/12.0f; const float radius=(0.45f+t*1.45f)*0.34f*hitBoost;
            drawBox(melee.impact+Vec3{std::cos(a)*radius,std::sin(a)*radius,0},{0.055f,0.025f,0.025f},0,0,a,1,1,1,melee.visualHit?fade*0.9f:fade*0.26f);}
        glDisable(GL_BLEND); glEnable(GL_LIGHTING);
    }

    const float tileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for (auto target:state.targets) if (target.alive) {
        Vec3 p=target.pos; p.z=tileOrigin + (target.pos.z - std::floor((target.pos.z+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);
        target.pos = p;
        if (target.slurpable && target.soulCubeAmount >= 0.995f) {
            const auto& sv=target.soulVisual;
            if (!sv.visible) continue;
            drawBox(p+Vec3{0,0.62f,0},{0.52f*target.scale*sv.scale.x,0.52f*target.scale*sv.scale.y,0.52f*target.scale*sv.scale.z},state.time*1.2f,state.time*1.7f,state.time*0.9f,sv.color.r,sv.color.g,sv.color.b);
            glDisable(GL_LIGHTING);
            const float shell=1.025f+sv.emission*0.012f;
            drawBox(p+Vec3{0,0.62f,0},{0.52f*target.scale*sv.scale.x*shell,0.52f*target.scale*sv.scale.y*shell,0.52f*target.scale*sv.scale.z*shell},state.time*1.2f,state.time*1.7f,state.time*0.9f,Pass7Visual::SoulEmission.r*sv.emission,Pass7Visual::SoulEmission.g*std::min(1.0f,sv.emission),Pass7Visual::SoulEmission.b*std::min(1.0f,sv.emission));
            glEnable(GL_LIGHTING);
        } else {
            drawProceduralHumanDesktop(target,state.time,target.slurpable?0.70f:0.14f,1.0f,target.slurpable?0.78f:0.32f);
        }
    }
    for (const auto& capture:state.captures) {
        Vec3 p=capture.pos; p.z+=tileOrigin;
        drawBox(p,{0.72f,0.72f,0.06f},0,0,0,capture.filled?0.56f:0.18f,capture.filled?0.95f:0.34f,1.0f);
    }
    for (const auto& bullet:state.bullets) if (bullet.alive) drawBox(bullet.pos,{0.18f,0.18f,0.18f},0,0,0,0.85f,1.0f,0.90f);
    glFlush();
}
