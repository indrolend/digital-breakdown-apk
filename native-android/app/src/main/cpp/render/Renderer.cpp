#include "Renderer.hpp"
#include "../game/HumanVisual.hpp"

#include <GLES2/gl2.h>
#include <android/log.h>
#include <array>
#include <cmath>
#include <cstring>

namespace {
constexpr float ROOM_WIDTH = 30.0f;
constexpr float ROOM_DEPTH = 42.0f;

constexpr int ROUNDED_SEGMENTS = 7;
constexpr int ROUNDED_RINGS = 5;
constexpr int ROUNDED_VERTEX_COUNT = ROUNDED_SEGMENTS * (ROUNDED_RINGS - 1) * 6;
constexpr int FX_SEGMENTS=24;
constexpr int FX_STRIP_VERTICES=(FX_SEGMENTS+1)*2;

std::array<float,FX_STRIP_VERTICES*3> makeRibbon(float inner,float outer,float sweep){std::array<float,FX_STRIP_VERTICES*3> out{}; int n=0;
    for(int i=0;i<=FX_SEGMENTS;++i){const float a=sweep*static_cast<float>(i)/FX_SEGMENTS,c=std::cos(a),s=std::sin(a); out[n++]=c*outer;out[n++]=s*outer;out[n++]=0;out[n++]=c*inner;out[n++]=s*inner;out[n++]=0;} return out;}
std::array<float,FX_STRIP_VERTICES*3> makeStreak(){std::array<float,FX_STRIP_VERTICES*3> out{}; int n=0;
    for(int i=0;i<=FX_SEGMENTS;++i){const float a=DB_PI*2*i/FX_SEGMENTS,c=std::cos(a),s=std::sin(a); out[n++]=c*0.11f;out[n++]=s*0.11f;out[n++]=-0.5f;out[n++]=c*0.035f;out[n++]=s*0.035f;out[n++]=0.5f;} return out;}
const auto FX_ARC=makeRibbon(0.494f,0.546f,DB_PI*1.35f);
const auto FX_RING=makeRibbon(0.318f,0.362f,DB_PI*2.0f);
const auto FX_STREAK=makeStreak();

Vec3 lowPolySpherePoint(int ring, int segment) {
    const float v = static_cast<float>(ring) / static_cast<float>(ROUNDED_RINGS);
    const float phi = -DB_PI * 0.5f + v * DB_PI;
    const float u = static_cast<float>(segment) / static_cast<float>(ROUNDED_SEGMENTS);
    const float theta = u * DB_PI * 2.0f;
    const float cp = std::cos(phi);
    return {std::cos(theta) * cp * 0.5f, std::sin(phi) * 0.5f, std::sin(theta) * cp * 0.5f};
}

std::array<float, ROUNDED_VERTEX_COUNT * 3> makeLowPolySphereVertices() {
    std::array<float, ROUNDED_VERTEX_COUNT * 3> vertices{};
    int out = 0;
    auto emit = [&](const Vec3& p) {
        vertices[out++] = p.x;
        vertices[out++] = p.y;
        vertices[out++] = p.z;
    };
    for (int ring = 0; ring < ROUNDED_RINGS; ++ring) {
        for (int seg = 0; seg < ROUNDED_SEGMENTS; ++seg) {
            const int nextSeg = (seg + 1) % ROUNDED_SEGMENTS;
            const Vec3 a = lowPolySpherePoint(ring, seg);
            const Vec3 b = lowPolySpherePoint(ring + 1, seg);
            const Vec3 c = lowPolySpherePoint(ring + 1, nextSeg);
            const Vec3 d = lowPolySpherePoint(ring, nextSeg);
            emit(a); emit(b); emit(c);
            emit(a); emit(c); emit(d);
        }
    }
    return vertices;
}


const char* VERT_SRC =
    "attribute vec3 aPos;\n"
    "uniform mat4 uMvp;\n"
    "varying float vLight;\n"
    "void main() {\n"
    "  vec3 n = normalize(aPos + vec3(0.0001));\n"
    "  float sun = max(dot(n, normalize(vec3(0.42, 0.84, 0.35))), 0.0);\n"
    "  float fill = max(dot(n, normalize(vec3(-0.46, 0.57, -0.68))), 0.0);\n"
    "  vLight = clamp(0.48 + sun * 0.42 + fill * 0.10, 0.0, 1.0);\n"
    "  gl_Position = uMvp * vec4(aPos, 1.0);\n"
    "}\n";

const char* FRAG_SRC =
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "varying float vLight;\n"
    "void main() { gl_FragColor = vec4(uColor.rgb * vLight, uColor.a); }\n";

void ident(float* m) {
    std::memset(m, 0, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void multiply(float* out, const float* a, const float* b) {
    float r[16]{};
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            r[c * 4 + row] =
                a[0 * 4 + row] * b[c * 4 + 0] +
                a[1 * 4 + row] * b[c * 4 + 1] +
                a[2 * 4 + row] * b[c * 4 + 2] +
                a[3 * 4 + row] * b[c * 4 + 3];
        }
    }
    std::memcpy(out, r, sizeof(r));
}

void perspective(float* m, float fovy, float aspect, float nearZ, float farZ) {
    std::memset(m, 0, sizeof(float) * 16);
    const float f = 1.0f / std::tan(fovy * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (farZ + nearZ) / (nearZ - farZ);
    m[11] = -1.0f;
    m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

void lookAt(float* m, Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 f = normalized(center - eye);
    const Vec3 s = normalized(cross(f, up));
    const Vec3 u = cross(s, f);
    ident(m);
    m[0] = s.x; m[4] = s.y; m[8] = s.z;
    m[1] = u.x; m[5] = u.y; m[9] = u.z;
    m[2] = -f.x; m[6] = -f.y; m[10] = -f.z;
    m[12] = -dot(s, eye);
    m[13] = -dot(u, eye);
    m[14] = dot(f, eye);
}

void modelBox(float* m, const Vec3& pos, const Vec3& scale, float yaw) {
    ident(m);
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    m[0] = c * scale.x;
    m[1] = 0.0f;
    m[2] = -s * scale.x;
    m[4] = 0.0f;
    m[5] = scale.y;
    m[6] = 0.0f;
    m[8] = s * scale.z;
    m[9] = 0.0f;
    m[10] = c * scale.z;
    m[12] = pos.x;
    m[13] = pos.y;
    m[14] = pos.z;
}

void modelBox(float* m, const Vec3& pos, const Vec3& scale, const Quat& q) {
    ident(m);
    m[0]=(1-2*(q.y*q.y+q.z*q.z))*scale.x; m[1]=(2*(q.x*q.y+q.z*q.w))*scale.x; m[2]=(2*(q.x*q.z-q.y*q.w))*scale.x;
    m[4]=(2*(q.x*q.y-q.z*q.w))*scale.y; m[5]=(1-2*(q.x*q.x+q.z*q.z))*scale.y; m[6]=(2*(q.y*q.z+q.x*q.w))*scale.y;
    m[8]=(2*(q.x*q.z+q.y*q.w))*scale.z; m[9]=(2*(q.y*q.z-q.x*q.w))*scale.z; m[10]=(1-2*(q.x*q.x+q.y*q.y))*scale.z;
    m[12]=pos.x; m[13]=pos.y; m[14]=pos.z;
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, "DBNATIVE", "shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
}

bool Renderer::initProgram() {
    const GLuint vs = compileShader(GL_VERTEX_SHADER, VERT_SRC);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAG_SRC);
    if (!vs || !fs) return false;

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glBindAttribLocation(program_, 0, "aPos");
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]{};
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, "DBNATIVE", "program link failed: %s", log);
        return false;
    }

    aPos_ = glGetAttribLocation(program_, "aPos");
    uMvp_ = glGetUniformLocation(program_, "uMvp");
    uColor_ = glGetUniformLocation(program_, "uColor");

    const float cube[] = {
        -0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f, -0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f, -0.5f,0.5f,-0.5f,
        -0.5f,-0.5f, 0.5f, 0.5f,0.5f, 0.5f, 0.5f,-0.5f, 0.5f, -0.5f,-0.5f,0.5f, -0.5f,0.5f,0.5f, 0.5f,0.5f,0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f,0.5f,-0.5f, -0.5f,0.5f,0.5f, -0.5f,-0.5f,-0.5f, -0.5f,0.5f,0.5f, -0.5f,-0.5f,0.5f,
        0.5f,-0.5f,-0.5f, 0.5f,-0.5f,0.5f, 0.5f,0.5f,0.5f, 0.5f,-0.5f,-0.5f, 0.5f,0.5f,0.5f, 0.5f,0.5f,-0.5f,
        -0.5f,0.5f,-0.5f, 0.5f,0.5f,-0.5f, 0.5f,0.5f,0.5f, -0.5f,0.5f,-0.5f, 0.5f,0.5f,0.5f, -0.5f,0.5f,0.5f,
        -0.5f,-0.5f,-0.5f, 0.5f,-0.5f,0.5f, 0.5f,-0.5f,-0.5f, -0.5f,-0.5f,-0.5f, -0.5f,-0.5f,0.5f, 0.5f,-0.5f,0.5f
    };
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);

    const auto rounded = makeLowPolySphereVertices();
    roundedVertexCount_ = ROUNDED_VERTEX_COUNT;
    glGenBuffers(1, &roundedVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, roundedVbo_);
    glBufferData(GL_ARRAY_BUFFER, rounded.size() * sizeof(float), rounded.data(), GL_STATIC_DRAW);
    return true;
}

void Renderer::surfaceCreated() {
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.02f, 0.04f, 0.02f, 1.0f);
    initProgram();
}

void Renderer::surfaceChanged(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
    glViewport(0, 0, width_, height_);
}

void Renderer::drawBox(const float* viewProj, const Vec3& pos, const Vec3& scale, float yaw, const float color[4]) {
    if (!program_) return;
    float model[16];
    float mvp[16];
    modelBox(model, pos, scale, yaw);
    multiply(mvp, viewProj, model);
    glUseProgram(program_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(static_cast<GLuint>(aPos_));
    glVertexAttribPointer(static_cast<GLuint>(aPos_), 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp);
    glUniform4fv(uColor_, 1, color);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void Renderer::drawBox(const float* viewProj, const Vec3& pos, const Vec3& scale, const Quat& orientation, const float color[4]) {
    if (!program_) return;
    float model[16], mvp[16]; modelBox(model,pos,scale,orientation); multiply(mvp,viewProj,model);
    glUseProgram(program_); glBindBuffer(GL_ARRAY_BUFFER,vbo_); glEnableVertexAttribArray(static_cast<GLuint>(aPos_));
    glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,nullptr);
    glUniformMatrix4fv(uMvp_,1,GL_FALSE,mvp); glUniform4fv(uColor_,1,color); glDrawArrays(GL_TRIANGLES,0,36);
}

void Renderer::drawFxStrip(const float* viewProj,const Vec3& pos,const Vec3& scale,const Quat& orientation,const float color[4],const float* vertices,int vertexCount){
    if(!program_) return; float model[16],mvp[16]; modelBox(model,pos,scale,orientation); multiply(mvp,viewProj,model);
    glUseProgram(program_); glBindBuffer(GL_ARRAY_BUFFER,0); glEnableVertexAttribArray(static_cast<GLuint>(aPos_));
    glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,vertices); glUniformMatrix4fv(uMvp_,1,GL_FALSE,mvp); glUniform4fv(uColor_,1,color); glDrawArrays(GL_TRIANGLE_STRIP,0,vertexCount);
}

void Renderer::drawRoundedEllipsoid(const float* viewProj, const Vec3& pos, const Vec3& scale, float yaw, const float color[4]) {
    if (!program_ || !roundedVbo_ || roundedVertexCount_ <= 0) return;
    float model[16];
    float mvp[16];
    modelBox(model, pos, scale, yaw);
    multiply(mvp, viewProj, model);
    glUseProgram(program_);
    glBindBuffer(GL_ARRAY_BUFFER, roundedVbo_);
    glEnableVertexAttribArray(static_cast<GLuint>(aPos_));
    glVertexAttribPointer(static_cast<GLuint>(aPos_), 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp);
    glUniform4fv(uColor_, 1, color);
    glDrawArrays(GL_TRIANGLES, 0, roundedVertexCount_);
}


void Renderer::drawProceduralHuman(const float* viewProj, const TargetState& target, float time, const float color[4]) {
    const HumanVisualSpec& spec = PASS7_HUMAN_VISUAL_SPEC;
    const bool aliveHuman = !target.slurpable;
    const HumanVisualPose pose = makeHumanVisualPose(target.visualYaw, target.scale, time, target.visualReaction, aliveHuman);
    if (pose.scale <= 0.001f) return;

    const float s = pose.scale;
    const float collapseScale = std::max(0.18f, 1.0f - pose.collapse * 0.62f);
    const Vec3 root = target.pos + Vec3{0.0f, spec.rootGroundOffset + pose.rootBob, 0.0f};
    const float yaw = pose.yaw;
    const Vec3 forward{-std::sin(yaw), 0.0f, -std::cos(yaw)};
    const Vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};

    const float footY = 0.03f * s;
    const float shinY = footY + spec.footHeight * s * 0.5f + spec.shinLength * s * 0.5f;
    const float thighY = footY + spec.footHeight * s + spec.shinLength * s + spec.thighLength * s * 0.5f;
    const float pelvisY = footY + spec.footHeight * s + spec.shinLength * s + spec.thighLength * s + spec.pelvisHeight * s * 0.5f;
    const float torsoY = pelvisY + (spec.pelvisHeight + spec.torsoHeight) * s * 0.5f;
    const float headY = spec.totalHeight * s - spec.headRadius * s;
    const float armY = torsoY + spec.torsoHeight * s * 0.18f;

    drawRoundedEllipsoid(viewProj, root + Vec3{0.0f, pelvisY * collapseScale, 0.0f}, {spec.pelvisWidth * s, spec.pelvisHeight * s * collapseScale, spec.pelvisDepth * s}, yaw, color);
    drawRoundedEllipsoid(viewProj, root + Vec3{0.0f, torsoY * collapseScale, 0.0f} + forward * ((pose.hitLean + pose.vacuumLean * 0.06f) * s), {spec.torsoWidth * s, spec.torsoHeight * s * collapseScale, spec.torsoDepth * s}, yaw + pose.torsoRoll, color);
    drawRoundedEllipsoid(viewProj, root + Vec3{0.0f, headY, 0.0f} + forward * (pose.headPitch * 0.03f), {spec.headRadius * 2.0f * s, spec.headRadius * 2.0f * s, spec.headRadius * 2.0f * s}, yaw, color);

    for (int side : {-1, 1}) {
        const float armSwing = side < 0 ? pose.leftArmSwing : pose.rightArmSwing;
        const float legSwing = side < 0 ? pose.leftLegSwing : pose.rightLegSwing;
        const Vec3 shoulder = root + right * (side * spec.shoulderWidth * 0.5f * s) + Vec3{0.0f, armY, 0.0f};
        drawRoundedEllipsoid(viewProj, shoulder + forward * (armSwing * 0.06f * s) + Vec3{0.0f, -spec.upperArmLength * 0.5f * s * collapseScale, 0.0f}, {0.055f * s, spec.upperArmLength * s * collapseScale, 0.065f * s}, yaw, color);
        drawRoundedEllipsoid(viewProj, shoulder + forward * (armSwing * 0.11f * s) + Vec3{0.0f, -(spec.upperArmLength + spec.forearmLength * 0.5f) * s * collapseScale, 0.0f}, {0.052f * s, spec.forearmLength * s * collapseScale, 0.060f * s}, yaw, color);
        drawRoundedEllipsoid(viewProj, shoulder + forward * (armSwing * 0.14f * s) + Vec3{0.0f, -(spec.upperArmLength + spec.forearmLength) * s, 0.0f}, {spec.handSize * s, spec.handSize * s, spec.handSize * 0.75f * s}, yaw, color);

        const Vec3 hip = root + right * (side * spec.pelvisWidth * 0.28f * s);
        drawRoundedEllipsoid(viewProj, hip + forward * (legSwing * 0.05f * s) + Vec3{0.0f, thighY * collapseScale, 0.0f}, {0.075f * s, spec.thighLength * s * collapseScale, 0.080f * s}, yaw, color);
        drawRoundedEllipsoid(viewProj, hip - forward * (legSwing * 0.05f * s) + Vec3{0.0f, shinY * collapseScale, 0.0f}, {0.070f * s, spec.shinLength * s * collapseScale, 0.075f * s}, yaw, color);
        drawRoundedEllipsoid(viewProj, hip + forward * (spec.footLength * 0.25f * s + legSwing * 0.04f * s) + Vec3{0.0f, footY, 0.0f}, {0.075f * s, spec.footHeight * s, spec.footLength * s}, yaw, color);
    }
}

void Renderer::drawGround(const float* viewProj) {
    const float groundColor[4] = {Pass7Visual::Floor.r, Pass7Visual::Floor.g, Pass7Visual::Floor.b, 1.0f};
    drawBox(viewProj, {0.0f, -0.04f, 0.0f}, {ROOM_WIDTH, 0.06f, ROOM_DEPTH}, 0.0f, groundColor);

    const float wallColor[4] = {Pass7Visual::Wall.r, Pass7Visual::Wall.g, Pass7Visual::Wall.b, 1.0f};
    drawBox(viewProj, {0.0f, 3.5f, -ROOM_DEPTH * 0.5f}, {ROOM_WIDTH, 7.0f, 0.16f}, 0.0f, wallColor);
    drawBox(viewProj, {-ROOM_WIDTH * 0.5f, 3.5f, 0.0f}, {0.16f, 7.0f, ROOM_DEPTH}, 0.0f, wallColor);
    drawBox(viewProj, {ROOM_WIDTH * 0.5f, 3.5f, 0.0f}, {0.16f, 7.0f, ROOM_DEPTH}, 0.0f, wallColor);
}

void Renderer::draw(const GameState& state) {
    glClearColor(Pass7Visual::Background.r, Pass7Visual::Background.g, Pass7Visual::Background.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float proj[16];
    float view[16];
    float viewProj[16];
    perspective(proj, 62.0f * DB_PI / 180.0f, static_cast<float>(width_) / static_cast<float>(height_), 0.05f, 90.0f);
    const Vec3 look = state.camera.firstPerson ? state.camera.pos + state.camera.forward * 10.0f : state.player.pos + state.camera.forward * 4.0f + Vec3{0.0f, 0.55f, 0.0f};
    lookAt(view, state.camera.pos, look, {0.0f, 1.0f, 0.0f});
    multiply(viewProj, proj, view);

    drawGround(viewProj);

    const float phoneBody[4] = {Pass7Visual::PhoneBody.r, Pass7Visual::PhoneBody.g, Pass7Visual::PhoneBody.b, 1.0f};
    const float screenBrightness = std::min(1.0f, 0.45f + state.phoneVisual.screenGlow * 0.36f);
    const float phoneScreen[4] = {Pass7Visual::PhoneEmission.r * screenBrightness, Pass7Visual::PhoneEmission.g * screenBrightness, Pass7Visual::PhoneEmission.b * screenBrightness, 1.0f};
    if (state.phoneVisual.visible) {
        const Vec3 forward{-std::sin(state.player.yaw), 0.0f, -std::cos(state.player.yaw)};
        const Vec3 right{std::cos(state.player.yaw), 0.0f, -std::sin(state.player.yaw)};
        const Vec3 phonePos = state.player.pos + Vec3{0.0f, state.phonePose.lift + state.phoneVisual.actionLift, 0.0f}
            + forward * (state.phonePose.forward + state.phoneVisual.actionForward) + right * state.phonePose.side;
        Quat phoneOrientation=state.phonePose.orientation * quatAxisAngle({1,0,0},state.phoneVisual.pitch) * quatAxisAngle({0,0,1},state.phoneVisual.roll);
        drawBox(viewProj, phonePos, {PHONE_BODY_WIDTH * state.phoneVisual.bodyScale.x, PHONE_BODY_HEIGHT * state.phoneVisual.bodyScale.y, PHONE_BODY_DEPTH}, phoneOrientation, phoneBody);
        drawBox(viewProj, phonePos + rotate(phoneOrientation,{0,0,PHONE_SCREEN_Z_OFFSET + state.phoneVisual.screenOffset}), {PHONE_SCREEN_WIDTH * state.phoneVisual.screenScale.x, PHONE_SCREEN_HEIGHT * state.phoneVisual.screenScale.y, PHONE_SCREEN_DEPTH}, phoneOrientation, phoneScreen);
    }

    const MeleeVisualState& melee=state.meleeVisual;
    if(melee.visualTimer>0.0f){
        const float t=1.0f-clampf(melee.visualTimer/std::max(0.001f,melee.visualDuration),0.0f,1.0f);
        const float hitBoost=melee.visualHit?1.25f:0.72f;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        const float cyan[4]={0.56f,0.97f,1.0f,(1.0f-t)*0.72f};
        const float slashScale=(0.75f+t*0.72f)*hitBoost;
        const Quat slashQ=quatAxisAngle({0,1,0},state.player.yaw)*quatAxisAngle({1,0,0},DB_PI*0.5f)*quatAxisAngle({0,0,1},-DB_PI*0.28f+t*DB_PI*1.15f);
        drawFxStrip(viewProj,melee.origin+melee.direction*(0.66f+0.22f*t),{slashScale,slashScale,1},slashQ,cyan,FX_ARC.data(),FX_STRIP_VERTICES);
        const Vec3 delta=melee.impact-melee.origin; const float len=std::max(0.3f,length(delta));
        const float yaw=std::atan2(delta.x,delta.z); const float pitch=-std::asin(clampf(delta.y/std::max(len,0.001f),-1.0f,1.0f));
        const Quat lineQ=quatAxisAngle({0,1,0},yaw)*quatAxisAngle({1,0,0},pitch); const float streak[4]={0.33f,0.84f,1.0f,(1.0f-t)*0.42f};
        const float strike=std::sin(t*DB_PI); drawFxStrip(viewProj,melee.origin+delta*0.46f,{1+strike*1.2f,1+strike*1.2f,len*(0.70f+strike*0.18f)},lineQ,streak,FX_STREAK.data(),FX_STRIP_VERTICES);
        const float white[4]={1,1,1,melee.visualHit?(1.0f-t)*0.9f:(1.0f-t)*0.26f};
        const float ringScale=(0.45f+t*1.45f)*hitBoost; drawFxStrip(viewProj,melee.impact,{ringScale,ringScale,1},{},white,FX_RING.data(),FX_STRIP_VERTICES);
        glDisable(GL_BLEND);
    }

    const float targetColor[4] = {0.14f, 1.0f, 0.32f, 1.0f};
    for (const auto& target : state.targets) {
        if (!target.alive) continue;
        if (target.slurpable && target.soulCubeAmount >= 0.995f) {
            if (!target.soulVisual.visible) continue;
            const float soulColor[4] = {target.soulVisual.color.r, target.soulVisual.color.g, target.soulVisual.color.b, 1.0f};
            drawBox(viewProj, target.pos + Vec3{0.0f, 0.62f, 0.0f}, {0.52f * target.scale * target.soulVisual.scale.x, 0.52f * target.scale * target.soulVisual.scale.y, 0.52f * target.scale * target.soulVisual.scale.z}, state.time * 1.7f, soulColor);
            const float shell = 1.025f + target.soulVisual.emission * 0.012f;
            const float glowColor[4] = {Pass7Visual::SoulEmission.r * target.soulVisual.emission, Pass7Visual::SoulEmission.g * std::min(1.0f, target.soulVisual.emission), Pass7Visual::SoulEmission.b * std::min(1.0f, target.soulVisual.emission), 1.0f};
            drawBox(viewProj, target.pos + Vec3{0.0f, 0.62f, 0.0f}, {0.52f * target.scale * target.soulVisual.scale.x * shell, 0.52f * target.scale * target.soulVisual.scale.y * shell, 0.52f * target.scale * target.soulVisual.scale.z * shell}, state.time * 1.7f, glowColor);
        } else {
            drawProceduralHuman(viewProj, target, state.time, targetColor);
        }
    }

    for (const auto& capture : state.captures) {
        const float filledColor[4] = {0.95f, 0.95f, 1.0f, 1.0f};
        const float openColor[4] = {0.18f, 0.34f, 1.0f, 1.0f};
        drawBox(viewProj, capture.pos, {1.55f, 1.55f, 0.18f}, 0.0f, capture.filled ? filledColor : openColor);
    }

    const float bulletColor[4] = {1.0f, 0.92f, 0.45f, 1.0f};
    for (const auto& bullet : state.bullets) {
        if (!bullet.alive) continue;
        drawBox(viewProj, bullet.pos, {0.28f, 0.28f, 0.28f}, state.time * 8.0f, bulletColor);
    }
}
