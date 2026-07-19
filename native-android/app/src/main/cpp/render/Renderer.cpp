#include "Renderer.hpp"
#include "../game/HumanVisual.hpp"
#include "../game/BitmapFont.hpp"

#include <GLES2/gl2.h>
#include <android/log.h>
#include <array>
#include <cmath>
#include <cstring>
#include <string>

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

Quat quaternionFromEulerXYZ(float x,float y,float z){const float c1=std::cos(x*0.5f),c2=std::cos(y*0.5f),c3=std::cos(z*0.5f),s1=std::sin(x*0.5f),s2=std::sin(y*0.5f),s3=std::sin(z*0.5f);return{s1*c2*c3+c1*s2*s3,c1*s2*c3-s1*c2*s3,c1*c2*s3+s1*s2*c3,c1*c2*c3-s1*s2*s3};}

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
    "attribute vec3 aNormal;\n"
    "uniform mat4 uMvp;\n"
    "uniform mat4 uModel;\n"
    "uniform float uUseNormal;\n"
    "varying float vLight;\n"
    "void main() {\n"
    "  vec3 radial = normalize(aPos + vec3(0.0001));\n"
    "  vec3 modelNormal = normalize(mat3(uModel) * aNormal + vec3(0.0001));\n"
    "  vec3 n = normalize(mix(radial, modelNormal, uUseNormal));\n"
    "  float sun = max(dot(n, normalize(vec3(0.42, 0.84, 0.35))), 0.0);\n"
    "  float fill = max(dot(n, normalize(vec3(-0.46, 0.57, -0.68))), 0.0);\n"
    "  vLight = uUseNormal < -0.5 ? 1.0 : clamp(0.48 + sun * 0.42 + fill * 0.10, 0.0, 1.0);\n"
    "  gl_Position = uMvp * vec4(aPos, 1.0);\n"
    "}\n";

const char* FRAG_SRC =
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "varying float vLight;\n"
    "void main() { gl_FragColor = vec4(uColor.rgb * vLight, uColor.a); }\n";

const char* DATAMOSH_VERT="attribute vec2 aPos;attribute vec2 aUv;varying vec2 vUv;void main(){vUv=aUv;gl_Position=vec4(aPos,0.0,1.0);}";
const char* DATAMOSH_FRAG="precision mediump float;uniform sampler2D uFrame;uniform float uAlpha;varying vec2 vUv;void main(){vec4 c=texture2D(uFrame,vUv);gl_FragColor=vec4(c.rgb,uAlpha);}";

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
    aNormal_ = glGetAttribLocation(program_, "aNormal");
    uMvp_ = glGetUniformLocation(program_, "uMvp");
    uModel_ = glGetUniformLocation(program_, "uModel");
    uUseNormal_ = glGetUniformLocation(program_, "uUseNormal");
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
    glGenBuffers(1, &soulSurfaceVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, soulSurfaceVbo_);
    glBufferData(GL_ARRAY_BUFFER, 144u * 3u * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    return true;
}

bool Renderer::initDatamoshProgram(){
    const GLuint vs=compileShader(GL_VERTEX_SHADER,DATAMOSH_VERT),fs=compileShader(GL_FRAGMENT_SHADER,DATAMOSH_FRAG);if(!vs||!fs)return false;
    datamoshProgram_=glCreateProgram();glAttachShader(datamoshProgram_,vs);glAttachShader(datamoshProgram_,fs);glLinkProgram(datamoshProgram_);glDeleteShader(vs);glDeleteShader(fs);
    GLint ok=0;glGetProgramiv(datamoshProgram_,GL_LINK_STATUS,&ok);if(!ok)return false;
    datamoshPos_=glGetAttribLocation(datamoshProgram_,"aPos");datamoshUv_=glGetAttribLocation(datamoshProgram_,"aUv");datamoshSampler_=glGetUniformLocation(datamoshProgram_,"uFrame");datamoshAlpha_=glGetUniformLocation(datamoshProgram_,"uAlpha");
    glGenTextures(1,&datamoshTexture_);glBindTexture(GL_TEXTURE_2D,datamoshTexture_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glGenBuffers(1,&datamoshVbo_);return true;
}

void Renderer::surfaceCreated() {
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.02f, 0.04f, 0.02f, 1.0f);
    initProgram();
    initDatamoshProgram();
    if(!assetRoot_.empty()) {
        phoneModel_.load(assetRoot_+"/phone.dbmesh"); flowerModel_.load(assetRoot_+"/flower.dbmesh"); humanModel_.load(assetRoot_+"/human.dbhuman");
        if(phoneModel_.valid()){glGenBuffers(1,&phoneVbo_);glBindBuffer(GL_ARRAY_BUFFER,phoneVbo_);glBufferData(GL_ARRAY_BUFFER,phoneModel_.vertices.size()*sizeof(float),phoneModel_.vertices.data(),GL_STATIC_DRAW);glGenBuffers(1,&phoneNormalVbo_);glBindBuffer(GL_ARRAY_BUFFER,phoneNormalVbo_);glBufferData(GL_ARRAY_BUFFER,phoneModel_.normals.size()*sizeof(float),phoneModel_.normals.data(),GL_STATIC_DRAW);}
        if(flowerModel_.valid()){glGenBuffers(1,&flowerVbo_);glBindBuffer(GL_ARRAY_BUFFER,flowerVbo_);glBufferData(GL_ARRAY_BUFFER,flowerModel_.vertices.size()*sizeof(float),flowerModel_.vertices.data(),GL_STATIC_DRAW);glGenBuffers(1,&flowerNormalVbo_);glBindBuffer(GL_ARRAY_BUFFER,flowerNormalVbo_);glBufferData(GL_ARRAY_BUFFER,flowerModel_.normals.size()*sizeof(float),flowerModel_.normals.data(),GL_STATIC_DRAW);}
        if(humanModel_.valid()){glGenBuffers(1,&humanVbo_);glBindBuffer(GL_ARRAY_BUFFER,humanVbo_);glBufferData(GL_ARRAY_BUFFER,humanModel_.vertices.size()*3u*sizeof(float),nullptr,GL_DYNAMIC_DRAW);glGenBuffers(1,&humanNormalVbo_);glBindBuffer(GL_ARRAY_BUFFER,humanNormalVbo_);glBufferData(GL_ARRAY_BUFFER,humanModel_.vertices.size()*3u*sizeof(float),nullptr,GL_DYNAMIC_DRAW);}
        __android_log_print(ANDROID_LOG_INFO,"DBNATIVE","models phone=%d flower=%d human=%d",phoneModel_.valid()?1:0,flowerModel_.valid()?1:0,humanModel_.valid()?1:0);
    }
}

void Renderer::surfaceChanged(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
    glViewport(0, 0, width_, height_);
    datamoshFrameReady_=false;
}

void Renderer::drawBox(const float* viewProj, const Vec3& pos, const Vec3& scale, float yaw, const float color[4]) {
    if (!program_) return;
    float model[16];
    float mvp[16];
    modelBox(model, pos, scale, yaw);
    multiply(mvp, viewProj, model);
    glUseProgram(program_);glUniform1f(uUseNormal_,0.0f);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(static_cast<GLuint>(aPos_));
    glVertexAttribPointer(static_cast<GLuint>(aPos_), 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp);
    glUniform4fv(uColor_, 1, color);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void Renderer::drawStaticModel(const float* viewProj,const StaticModelData& model,unsigned int vbo,unsigned int normalVbo,const Vec3& pos,const Vec3& scale,const Quat& orientation,bool shadow) {
    if(!program_ || !vbo || !normalVbo || !model.valid()) return;
    float matrix[16],mvp[16];modelBox(matrix,pos,scale,orientation);multiply(mvp,viewProj,matrix);
    glUseProgram(program_);glUniform1f(uUseNormal_,shadow?-1.0f:1.0f);glUniformMatrix4fv(uModel_,1,GL_FALSE,matrix);glBindBuffer(GL_ARRAY_BUFFER,vbo);glEnableVertexAttribArray(static_cast<GLuint>(aPos_));glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,nullptr);glBindBuffer(GL_ARRAY_BUFFER,normalVbo);glEnableVertexAttribArray(static_cast<GLuint>(aNormal_));glVertexAttribPointer(static_cast<GLuint>(aNormal_),3,GL_FLOAT,GL_FALSE,0,nullptr);glUniformMatrix4fv(uMvp_,1,GL_FALSE,mvp);
    const float shadowColor[4]={0.012f,0.018f,0.022f,0.28f};
    for(const auto& batch:model.batches){const bool translucent=!shadow&&batch.color[3]<0.995f;if(translucent){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);}glUniform4fv(uColor_,1,shadow?shadowColor:batch.color);glDrawArrays(GL_TRIANGLES,static_cast<GLint>(batch.start),static_cast<GLsizei>(batch.count));if(translucent){glDepthMask(GL_TRUE);glDisable(GL_BLEND);}}
}

void Renderer::drawBox(const float* viewProj, const Vec3& pos, const Vec3& scale, const Quat& orientation, const float color[4]) {
    if (!program_) return;
    float model[16], mvp[16]; modelBox(model,pos,scale,orientation); multiply(mvp,viewProj,model);
    glUseProgram(program_); glUniform1f(uUseNormal_,0.0f); glBindBuffer(GL_ARRAY_BUFFER,vbo_); glEnableVertexAttribArray(static_cast<GLuint>(aPos_));
    glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,nullptr);
    glUniformMatrix4fv(uMvp_,1,GL_FALSE,mvp); glUniform4fv(uColor_,1,color); glDrawArrays(GL_TRIANGLES,0,36);
}

void Renderer::drawFxStrip(const float* viewProj,const Vec3& pos,const Vec3& scale,const Quat& orientation,const float color[4],const float* vertices,int vertexCount){
    if(!program_) return; float model[16],mvp[16]; modelBox(model,pos,scale,orientation); multiply(mvp,viewProj,model);
    glUseProgram(program_); glUniform1f(uUseNormal_,0.0f); glBindBuffer(GL_ARRAY_BUFFER,0); glEnableVertexAttribArray(static_cast<GLuint>(aPos_));
    glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,vertices); glUniformMatrix4fv(uMvp_,1,GL_FALSE,mvp); glUniform4fv(uColor_,1,color); glDrawArrays(GL_TRIANGLE_STRIP,0,vertexCount);
}

void Renderer::drawRoundedEllipsoid(const float* viewProj, const Vec3& pos, const Vec3& scale, float yaw, const float color[4]) {
    if (!program_ || !roundedVbo_ || roundedVertexCount_ <= 0) return;
    float model[16];
    float mvp[16];
    modelBox(model, pos, scale, yaw);
    multiply(mvp, viewProj, model);
    glUseProgram(program_);glUniform1f(uUseNormal_,0.0f);
    glBindBuffer(GL_ARRAY_BUFFER, roundedVbo_);
    glEnableVertexAttribArray(static_cast<GLuint>(aPos_));
    glVertexAttribPointer(static_cast<GLuint>(aPos_), 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp);
    glUniform4fv(uColor_, 1, color);
    glDrawArrays(GL_TRIANGLES, 0, roundedVertexCount_);
}


void Renderer::drawProceduralHuman(const float* viewProj, const TargetState& target, float time, const float color[4]) {
    const HumanVisualSpec& spec = PASS7_HUMAN_VISUAL_SPEC;
    const bool aliveHuman = !target.slurpable || target.soulCubeAmount < 0.995f;
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

void Renderer::drawRoomTile(const float* viewProj, const GameState& state, int tileIndex) {
    const float z0=static_cast<float>(tileIndex)*ROOM_DEPTH;
    const float groundColor[4] = {Pass7Visual::Floor.r, Pass7Visual::Floor.g, Pass7Visual::Floor.b, 1.0f};
    drawBox(viewProj, {0.0f, -0.04f, z0}, {ROOM_WIDTH, 0.08f, ROOM_DEPTH}, 0.0f, groundColor);

    const float wallColor[4] = {Pass7Visual::Wall.r, Pass7Visual::Wall.g, Pass7Visual::Wall.b, 1.0f};
    const float doorWidth=5.35f,doorHeight=3.95f,wallHeight=7.2f;
    const float sideWidth=(ROOM_WIDTH-doorWidth)*0.5f;
    const float sideX=doorWidth*0.5f+sideWidth*0.5f;
    const float topHeight=wallHeight-doorHeight;
    const float topY=doorHeight+topHeight*0.5f;
    drawBox(viewProj,{0,wallHeight+0.08f,z0},{ROOM_WIDTH,0.16f,ROOM_DEPTH},0,wallColor);
    for(float seam:{-ROOM_DEPTH*0.5f,ROOM_DEPTH*0.5f}) {
        drawBox(viewProj,{-sideX,wallHeight*0.5f,z0+seam},{sideWidth,wallHeight,0.5f},0,wallColor);
        drawBox(viewProj,{sideX,wallHeight*0.5f,z0+seam},{sideWidth,wallHeight,0.5f},0,wallColor);
        drawBox(viewProj,{0,topY,z0+seam},{doorWidth,topHeight,0.5f},0,wallColor);
    }
    drawBox(viewProj,{-ROOM_WIDTH*0.5f,wallHeight*0.5f,z0},{0.5f,wallHeight,ROOM_DEPTH},0,wallColor);
    drawBox(viewProj,{ROOM_WIDTH*0.5f,wallHeight*0.5f,z0},{0.5f,wallHeight,ROOM_DEPTH},0,wallColor);
    const float obstacleColor[4]={0.43f,0.49f,0.53f,1.0f};
    for(int i=0;i<state.debug.colliderCount;++i){const RoomCollider& collider=state.roomColliders[i]; drawBox(viewProj,{collider.center.x,collider.center.y,z0+collider.center.z},{collider.width,collider.height,collider.depth},0,obstacleColor);}
}

void Renderer::drawHud(const GameState& state) {
    float identity[16]; ident(identity);
    glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    const auto quad=[&](float x,float y,float w,float h,const float color[4],float angle=0.0f){
        const Vec3 center{-1.0f+(x+w*0.5f)*2.0f/static_cast<float>(width_),1.0f-(y+h*0.5f)*2.0f/static_cast<float>(height_),0.0f};
        const Vec3 scale{w*2.0f/static_cast<float>(width_),h*2.0f/static_cast<float>(height_),0.01f};
        drawBox(identity,center,scale,quatAxisAngle({0,0,1},-angle),color);
    };
    const auto pixelRotatedQuad=[&](float cx,float cy,float w,float h,float angle,const float color[4]){
        const float c=std::cos(angle),s=std::sin(angle),hx=w*0.5f,hy=h*0.5f;
        const float corners[8]={-hx,-hy,hx,-hy,hx,hy,-hx,hy};
        const int order[6]={0,1,2,0,2,3};float vertices[18]{};
        for(int i=0;i<6;++i){const int k=order[i]*2;const float px=cx+corners[k]*c-corners[k+1]*s,py=cy+corners[k]*s+corners[k+1]*c;vertices[i*3]=-1.0f+px*2.0f/width_;vertices[i*3+1]=1.0f-py*2.0f/height_;}
        glUseProgram(program_);glUniform1f(uUseNormal_,-1.0f);glUniformMatrix4fv(uMvp_,1,GL_FALSE,identity);glUniformMatrix4fv(uModel_,1,GL_FALSE,identity);glUniform4fv(uColor_,1,color);glBindBuffer(GL_ARRAY_BUFFER,0);glEnableVertexAttribArray(static_cast<GLuint>(aPos_));glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,vertices);glDrawArrays(GL_TRIANGLES,0,6);
    };
    const auto text=[&](const std::string& value,float x,float y,float scale,const float color[4]){float pen=x;for(char c:value){if(c==' '){pen+=6*scale;continue;}const auto rows=bitmapGlyph(c);for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1u<<(4-col)))quad(pen+col*scale,y+row*scale,scale,scale,color);pen+=6*scale;}};
    const float panel[4]={0.005f,0.012f,0.016f,0.72f}, cyan[4]={0.50f,0.91f,1.0f,0.95f};
    if(state.cinematic.introActive){glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);return;}
    if(!state.started) {
        const float black[4]={0,0,0,state.dead?0.46f:0.88f}; quad(0,0,static_cast<float>(width_),static_cast<float>(height_),black);
        const float pw=static_cast<float>(width_)*0.72f,ph=static_cast<float>(height_)*0.25f,px=(width_-pw)*0.5f,py=(height_-ph)*0.5f;
        quad(px,py,pw,ph,panel); quad(px,py,pw,3,cyan); quad(px,py+ph-3,pw,3,cyan); quad(px,py,3,ph,cyan); quad(px+pw-3,py,3,ph,cyan);
        const float white[4]={0.92f,0.97f,1,0.94f};const std::string status=state.dead?"BATTERY EMPTY":"READY";const float ss=std::max(2.0f,std::min(width_,height_)/240.0f);text(status,px+(pw-status.size()*6*ss)*0.5f,py+ph*0.30f,ss,white);quad(px+pw*0.24f,py+ph*0.60f,pw*0.52f,38,panel);const std::string action=state.dead?"TAP TO RESTART":"START";const float startScale=ss*0.8f;text(action,px+(pw-action.size()*6*startScale)*0.5f,py+ph*0.60f+10,startScale,white);
        glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); return;
    }
    const int goals=std::max(1,state.hud.requiredGoals); const float gs=22,gap=8,total=goals*gs+(goals-1)*gap,start=(width_-total)*0.5f;
    for(int i=0;i<goals;++i){const float empty[4]={0.01f,0.02f,0.025f,0.90f}; const float filled[4]={0.18f,0.88f,1,0.95f}; quad(start+i*(gs+gap),18,gs,gs,cyan); quad(start+i*(gs+gap)+2,20,gs-4,gs-4,i<state.hud.filledGoals?filled:empty);}
    quad(12,74,120,82,panel);
    const int filledSoulPixels=state.hud.storedSouls<=0?0:std::max(1,static_cast<int>(std::ceil(state.hud.storedSouls/30.0f*18.0f)));
    const Vec3 hudRight{std::cos(state.camera.yaw),0,-std::sin(state.camera.yaw)},hudForward{-std::sin(state.camera.yaw),0,-std::cos(state.camera.yaw)};
    const float lateral=state.player.vel.x*hudRight.x+state.player.vel.z*hudRight.z,forward=state.player.vel.x*hudForward.x+state.player.vel.z*hudForward.z;
    const float inertiaX=clampf(-lateral*1.35f,-7.0f,7.0f),inertiaY=clampf(forward*0.75f-state.player.jumpVel*0.12f,-5.0f,5.0f);
    for(int i=0;i<18;++i){const float angle=i/18.0f*DB_PI*2.0f,ring=0.38f+((i*7)%10)/26.0f,sx=34.0f*ring*std::cos(angle)+((i*13)%7-3),sy=14.0f*ring*std::sin(angle)+((i*17)%5-2),dx=((i*11)%9-4)*0.9f,dy=((i*19)%7-3)*0.8f,drift=0.5f+0.5f*std::sin(state.time*(1.35f+i%6*0.08f)-i*0.41f);const bool filled=i<filledSoulPixels;const float empty[4]={0.50f,0.90f,1,0.10f},full[4]={0.80f,1,1,0.92f};quad(67+sx+dx*drift+inertiaX,119+sy+dy*drift+inertiaY,filled?6:5,filled?6:5,filled?full:empty);}
    const float white[4]={1,1,1,0.94f},soft[4]={0.78f,0.94f,1,0.82f},green[4]={0.72f,1,0.74f,0.94f};text("SOULS "+std::to_string(state.hud.storedSouls),20,80,1.2f,soft);text("ROOM: "+std::to_string(state.roomIndex),12,170,1.5f,white);text("GOALS: "+std::to_string(state.hud.filledGoals)+"/"+std::to_string(state.hud.requiredGoals),12,187,1.5f,white);text(state.roomClear?"DOOR: OPEN":"DOOR: LOOP",12,204,1.5f,state.roomClear?green:white);
    quad(12,236,148,30,panel); const float track[4]={0.025f,0.035f,0.04f,0.95f}; quad(21,250,132,6,track);
    const float battery[4]={state.hud.lowBattery?1.0f:0.92f,state.hud.lowBattery?0.18f:0.97f,state.hud.lowBattery?0.12f:1.0f,0.98f}; quad(21,250,132*clampf(state.hud.batteryFill,0,1),6,battery);
    text("BATTERY",20,240,1.1f,state.hud.lowBattery?battery:white);
    if(state.energy.comboHits>0&&state.time-state.energy.lastComboHitTime<=1.8f){char multiplier[16]{};std::snprintf(multiplier,sizeof(multiplier),"X%.2F",state.energy.comboMultiplier);text(multiplier,112,240,1.0f,green);}
    if(state.hud.flowerStacks>0 || state.hud.supplementalFill>0.001f){const float flower[4]={0.35f,1,0.68f,0.98f};quad(12,274,148,24,panel);quad(21,283,132,6,track);quad(21,283,132*clampf(state.hud.supplementalFill,0,1),6,flower);text("POWER X"+std::to_string(state.hud.flowerStacks),20,276,1.0f,flower);}
    if(state.hud.energyTicker[0]&&state.time<state.hud.energyTickerUntil){const std::string ticker=state.hud.energyTicker.data();const float scale=1.35f,tw=ticker.size()*6*scale,pw=std::max(118.0f,tw+16.0f),px=(width_-pw)*0.5f;const float gain[4]={0.81f,1,0.91f,0.94f},cost[4]={1,0.69f,0.62f,0.94f};quad(px,72,pw,18,panel);text(ticker,(width_-tw)*0.5f,77,scale,state.hud.energyTickerType==1?cost:gain);}
    if(!state.uiPaused){const float minSide=static_cast<float>(std::min(width_,height_)),r=std::max(44.0f,minSide*0.070f),moveR=minSide*0.18f;const float control[4]={0.50f,0.91f,1.0f,0.22f},label[4]={0.88f,0.98f,1.0f,0.82f};const auto circle=[&](float cx,float cy,float radius){const Vec3 c{-1+cx*2.0f/width_,1-cy*2.0f/height_,0};drawRoundedEllipsoid(identity,c,{radius*4.0f/width_,radius*4.0f/height_,0.01f},0,control);};const auto button=[&](float cx,float cy,float radius,const char* name){circle(cx,cy,radius);const float s=1.5f,w=std::strlen(name)*6*s;text(name,cx-w*0.5f,cy-5*s,s,label);};circle(minSide*0.18f,height_-minSide*0.20f,moveR);button(width_-r*1.40f,height_-r*3.70f,r,"J");button(width_-r*3.70f,height_-r*1.40f,r,"M");button(width_-r*3.70f,height_-r*3.70f,r,"S");button(width_-r*6.00f,height_-r*1.40f,r*0.82f,"C");button(width_-r*1.40f,height_-r*1.40f,r*1.12f,"V");}
    const float cx=width_*0.5f,cy=height_*0.5f,spread=state.hud.crosshairSpreadPixels,arm=14,thick=3,angle=state.hud.crosshairRotationDegrees*DB_PI/180.0f;
    const float reticle[4]={state.hud.shootJoinTimer>0?1.0f:0.498f,state.hud.shootJoinTimer>0?1.0f:0.906f,1,0.98f*clampf(state.hud.crosshairOpacity,0.0f,1.0f)};
    const auto armQuad=[&](float ox,float oy,float w,float h){const float c=std::cos(angle),s=std::sin(angle);pixelRotatedQuad(cx+ox*c-oy*s,cy+ox*s+oy*c,w,h,angle,reticle);};
    armQuad(0,-spread,thick,arm); armQuad(0,spread,thick,arm); armQuad(-spread,0,arm,thick); armQuad(spread,0,arm,thick);
    if(state.uiPaused){const float pw=std::min(320.0f,width_-24.0f),ph=170.0f,px=width_-pw-12.0f,py=48.0f;quad(px,py,pw,ph,panel);text("PAUSED",px+12,py+12,2.0f,white);text("TAP TO RESUME",px+12,py+38,1.3f,cyan);text("MOVE  LOOK  VACUUM",px+12,py+72,1.2f,white);text("JUMP  ATTACK  SHOOT",px+12,py+92,1.2f,white);text("CAMERA",px+12,py+112,1.2f,white);}
    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
}

void Renderer::drawHumanModel(const float* viewProj,const TargetState& target,float time,bool shadow){
    humanModel_.skin(target.humanAnimationTime,target.attackTimer,target.attackVariant,humanVertices_);if(humanVertices_.empty()||!humanVbo_)return;
    humanNormals_.assign(humanVertices_.size(),0.0f);for(std::size_t i=0;i+8<humanVertices_.size();i+=9){const Vec3 a{humanVertices_[i],humanVertices_[i+1],humanVertices_[i+2]},b{humanVertices_[i+3],humanVertices_[i+4],humanVertices_[i+5]},c{humanVertices_[i+6],humanVertices_[i+7],humanVertices_[i+8]},n=normalized(cross(b-a,c-a));for(int v=0;v<3;++v){humanNormals_[i+v*3]=n.x;humanNormals_[i+v*3+1]=n.y;humanNormals_[i+v*3+2]=n.z;}}
    const bool aliveHuman=!target.slurpable||target.soulCubeAmount<0.995f;const HumanVisualPose pose=makeHumanVisualPose(target.visualYaw,target.scale,time,target.visualReaction,aliveHuman);
    const float t=target.attackTimer>0?1-clampf(target.attackTimer/HUMAN_SWING_ATTACK_DURATION,0,1):0,windup=std::sin(clampf(t/HUMAN_SWING_COMMIT_PHASE,0,1)*DB_PI*0.5f)*(t<HUMAN_SWING_COMMIT_PHASE?1.0f:0.0f),strike=std::sin(clampf((t-HUMAN_SWING_COMMIT_PHASE)/(HUMAN_SWING_END_PHASE-HUMAN_SWING_COMMIT_PHASE),0,1)*DB_PI),side=target.attackVariant%2==0?1.0f:-1.0f,low=target.attackVariant>=2?1.0f:0.0f;
    const Quat q=quaternionFromEulerXYZ(target.attackTimer>0?-strike*(0.18f+low*0.06f)+windup*0.10f:0,target.visualYaw+DB_PI,target.attackTimer>0?side*(strike*0.30f-windup*0.20f):0);
    const Vec3 root{target.pos.x,target.attackTimer>0?std::sin(t*DB_PI)*0.035f*low:0,target.pos.z};float model[16],mvp[16];modelBox(model,root,{pose.scale,pose.scale,pose.scale},q);multiply(mvp,viewProj,model);
    const float shadowColor[4]={0.012f,0.018f,0.022f,0.28f};
    glUseProgram(program_);glUniform1f(uUseNormal_,shadow?-1.0f:1.0f);glUniformMatrix4fv(uModel_,1,GL_FALSE,model);glBindBuffer(GL_ARRAY_BUFFER,humanVbo_);glBufferData(GL_ARRAY_BUFFER,humanVertices_.size()*sizeof(float),humanVertices_.data(),GL_DYNAMIC_DRAW);glEnableVertexAttribArray(static_cast<GLuint>(aPos_));glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,nullptr);glBindBuffer(GL_ARRAY_BUFFER,humanNormalVbo_);glBufferData(GL_ARRAY_BUFFER,humanNormals_.size()*sizeof(float),humanNormals_.data(),GL_DYNAMIC_DRAW);glEnableVertexAttribArray(static_cast<GLuint>(aNormal_));glVertexAttribPointer(static_cast<GLuint>(aNormal_),3,GL_FLOAT,GL_FALSE,0,nullptr);glUniformMatrix4fv(uMvp_,1,GL_FALSE,mvp);glUniform4fv(uColor_,1,shadow?shadowColor:humanModel_.color);glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(humanVertices_.size()/3u));
}

void Renderer::drawSoulFlesh(const float* viewProj,const TargetState& target,const Vec3& center){
    std::array<float,144*3> vertices{};int out=0;
    const auto index=[](int x,int y,int z){return x+y*3+z*9;};
    const auto emit=[&](int node){const Vec3 p=center+target.latticeSurfacePos[node];vertices[out++]=p.x;vertices[out++]=p.y;vertices[out++]=p.z;};
    const auto quad=[&](int a,int b,int c,int d){emit(a);emit(b);emit(c);emit(a);emit(c);emit(d);};
    for(int y=0;y<2;++y)for(int z=0;z<2;++z){quad(index(0,y,z),index(0,y+1,z),index(0,y+1,z+1),index(0,y,z+1));quad(index(2,y,z),index(2,y,z+1),index(2,y+1,z+1),index(2,y+1,z));}
    for(int x=0;x<2;++x)for(int z=0;z<2;++z){quad(index(x,0,z),index(x,0,z+1),index(x+1,0,z+1),index(x+1,0,z));quad(index(x,2,z),index(x+1,2,z),index(x+1,2,z+1),index(x,2,z+1));}
    for(int x=0;x<2;++x)for(int y=0;y<2;++y){quad(index(x,y,0),index(x+1,y,0),index(x+1,y+1,0),index(x,y+1,0));quad(index(x,y,2),index(x,y+1,2),index(x+1,y+1,2),index(x+1,y,2));}
    float identity[16];ident(identity);const float flesh[4]={224.0f/255.0f,160.0f/255.0f,143.0f/255.0f,1.0f};
    glUseProgram(program_);glUniform1f(uUseNormal_,0.0f);glBindBuffer(GL_ARRAY_BUFFER,soulSurfaceVbo_);glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(out*sizeof(float)),vertices.data(),GL_DYNAMIC_DRAW);glEnableVertexAttribArray(static_cast<GLuint>(aPos_));glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,nullptr);glUniformMatrix4fv(uMvp_,1,GL_FALSE,viewProj);glUniform4fv(uColor_,1,flesh);glDrawArrays(GL_TRIANGLES,0,out/3);
    if(target.tetherVisible){const Vec3 delta=target.tetherDestination-target.tetherAnchor;const float len=length(delta);if(len>0.001f){const float yaw=std::atan2(delta.x,delta.z),pitch=-std::asin(clampf(delta.y/len,-1.0f,1.0f));const Quat orientation=quaternionFromEulerXYZ(pitch,yaw,0);const float tether[4]={1.0f,183.0f/255.0f,166.0f/255.0f,0.34f};glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);drawBox(viewProj,target.tetherAnchor+delta*0.5f,{0.13f*target.tetherWidth,0.13f*target.tetherWidth,std::min(len,4.5f)},orientation,tether);glDepthMask(GL_TRUE);glDisable(GL_BLEND);}}
}

void Renderer::drawDoorDataMosh(const GameState& state){
    if(!datamoshProgram_||!datamoshTexture_)return;glBindTexture(GL_TEXTURE_2D,datamoshTexture_);
    if(!state.doorTransition.active||state.doorTransition.progress<=0.018f){glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,width_,height_,0);datamoshFrameReady_=true;return;}if(!datamoshFrameReady_)return;
    constexpr int strips=24,passes=3,floatsPerVertex=4,verticesPerQuad=6;std::array<float,strips*passes*verticesPerQuad*floatsPerVertex> data{};int out=0;const float strength=clampf(state.doorTransition.progress,0,1);
    const auto vertex=[&](float x,float y,float u,float v){data[out++]=x;data[out++]=y;data[out++]=u;data[out++]=v;};
    for(int row=0;row<strips;++row){const float y0=-1+2.0f*row/strips,y1=-1+2.0f*(row+1)/strips,v0=static_cast<float>(row)/strips,v1=static_cast<float>(row+1)/strips,phase=state.time*5.1f+row*1.73f,shiftPixels=(std::sin(phase)*7+std::sin(phase*0.37f)*13)*strength;for(int pass=passes-1;pass>=0;--pass){const float shift=shiftPixels*(pass+1)/passes*2.0f/std::max(1,width_);vertex(-1+shift,y0,0,v0);vertex(1+shift,y0,1,v0);vertex(1+shift,y1,1,v1);vertex(-1+shift,y0,0,v0);vertex(1+shift,y1,1,v1);vertex(-1+shift,y1,0,v1);}}
    glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glUseProgram(datamoshProgram_);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,datamoshTexture_);glUniform1i(datamoshSampler_,0);glUniform1f(datamoshAlpha_,clampf(0.34f+strength*0.56f,0.34f,0.90f)/passes);glBindBuffer(GL_ARRAY_BUFFER,datamoshVbo_);glBufferData(GL_ARRAY_BUFFER,out*sizeof(float),data.data(),GL_DYNAMIC_DRAW);glEnableVertexAttribArray(static_cast<GLuint>(datamoshPos_));glEnableVertexAttribArray(static_cast<GLuint>(datamoshUv_));glVertexAttribPointer(static_cast<GLuint>(datamoshPos_),2,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);glVertexAttribPointer(static_cast<GLuint>(datamoshUv_),2,GL_FLOAT,GL_FALSE,4*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));glDrawArrays(GL_TRIANGLES,0,out/4);glDisableVertexAttribArray(static_cast<GLuint>(datamoshUv_));glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glUseProgram(program_);
}

void Renderer::draw(const GameState& state) {
    glClearColor(Pass7Visual::Background.r, Pass7Visual::Background.g, Pass7Visual::Background.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float proj[16];
    float view[16];
    float viewProj[16];
    // Match the authoritative THREE.PerspectiveCamera exactly. The previous
    // 62-degree mobile projection made the same chase-camera transform feel
    // zoomed, slow, and substantially less spatial than the browser's camera.
    perspective(proj, Pass7Visual::CameraVerticalFovDegrees * DB_PI / 180.0f, static_cast<float>(width_) / static_cast<float>(height_), Pass7Visual::CameraNearPlane, Pass7Visual::CameraFarPlane);
    lookAt(view, state.camera.pos, state.camera.lookTarget, {0.0f, 1.0f, 0.0f});
    multiply(viewProj, proj, view);

    for(int tile=state.topology.currentTileIndex-1;tile<=state.topology.currentTileIndex+1;++tile) drawRoomTile(viewProj,state,tile);

    // Project every caster's geometry along the browser sun vector (30,60,25)
    // onto the floor. This preserves physical direction, length and silhouette
    // while avoiding a shadow-map texture pass on mobile tile GPUs.
    const float shadowMatrix[16]={1,0,0,0,-0.5f,0,-25.0f/60.0f,0,0,0,1,0,0.006f,0.012f,0.005f,1};
    float shadowViewProj[16];multiply(shadowViewProj,viewProj,shadowMatrix);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
    const float shadow[4]={0.012f,0.018f,0.022f,0.28f};
    if(!state.camera.firstPerson){if(phoneModel_.valid())drawStaticModel(shadowViewProj,phoneModel_,phoneVbo_,phoneNormalVbo_,state.phoneTransform.position,state.phoneVisual.bodyScale,state.phoneTransform.orientation,true);else drawBox(shadowViewProj,state.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},state.phoneTransform.orientation,shadow);}
    if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.playerId!=state.multiplayer.localPlayerId&&peer.player.alive){if(phoneModel_.valid())drawStaticModel(shadowViewProj,phoneModel_,phoneVbo_,phoneNormalVbo_,peer.phoneTransform.position,peer.phoneVisual.bodyScale,peer.phoneTransform.orientation,true);else drawBox(shadowViewProj,peer.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},peer.phoneTransform.orientation,shadow);}
    const float shadowTileOrigin=state.topology.currentTileIndex*ROOM_DEPTH;
    for(int offset=-1;offset<=1;++offset)for(auto target:state.targets)if(target.alive){target.pos.z=shadowTileOrigin+static_cast<float>(offset)*ROOM_DEPTH+(target.pos.z-std::floor((target.pos.z+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);if(!target.slurpable||target.soulCubeAmount<0.995f){if(humanModel_.valid())drawHumanModel(shadowViewProj,target,state.time,true);else drawProceduralHuman(shadowViewProj,target,state.time,shadow);}if(target.slurpable&&target.soulVisual.visible&&target.soulCubeAmount>0.001f){const auto& sv=target.soulVisual;const float cube=0.72f*0.78f*target.scale*sv.morphScale;drawBox(shadowViewProj,target.pos+Vec3{0,0.57f+sv.verticalOffset,0},{cube*sv.scale.x,cube*sv.scale.y,cube*sv.scale.z},sv.rotationY,shadow);}}
    for(const auto& flower:state.flowers)if(flower.active){const Vec3 center{flower.pos.x,flower.pos.y,flower.pos.z+shadowTileOrigin};if(flowerModel_.valid())drawStaticModel(shadowViewProj,flowerModel_,flowerVbo_,flowerNormalVbo_,center,{1,1,1},quatAxisAngle({0,1,0},flower.rotationY),true);else drawBox(shadowViewProj,center,{0.54f,0.22f,0.54f},flower.rotationY,shadow);}
    for(const auto& bullet:state.bullets)if(bullet.alive){const float size=0.72f*1.12f*(bullet.brute?1.7f:1.0f);drawBox(shadowViewProj,bullet.pos,{size,size,size},bullet.spin*1.7f,shadow);}
    for(int i=0;i<state.debug.colliderCount;++i){const auto& c=state.roomColliders[i];drawBox(shadowViewProj,{c.center.x,c.center.y,shadowTileOrigin+c.center.z},{c.width,c.height,c.depth},0,shadow);}
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);

    const float phoneBody[4] = {Pass7Visual::PhoneBody.r, Pass7Visual::PhoneBody.g, Pass7Visual::PhoneBody.b, 1.0f};
    const float screenBrightness = std::min(1.0f, 0.45f + state.phoneVisual.screenGlow * 0.36f);
    const float phoneScreen[4] = {Pass7Visual::PhoneEmission.r * screenBrightness, Pass7Visual::PhoneEmission.g * screenBrightness, Pass7Visual::PhoneEmission.b * screenBrightness, 1.0f};
    if (state.phoneVisual.visible) {
        const Vec3 phonePos = state.phoneTransform.position;
        const Quat phoneOrientation = state.phoneTransform.orientation;
        if(phoneModel_.valid()) drawStaticModel(viewProj,phoneModel_,phoneVbo_,phoneNormalVbo_,phonePos,state.phoneVisual.bodyScale,phoneOrientation);
        else drawBox(viewProj, phonePos, {PHONE_BODY_WIDTH * state.phoneVisual.bodyScale.x, PHONE_BODY_HEIGHT * state.phoneVisual.bodyScale.y, PHONE_BODY_DEPTH}, phoneOrientation, phoneBody);
        drawBox(viewProj, state.phoneTransform.screenCenter, {PHONE_SCREEN_WIDTH * state.phoneVisual.screenScale.x, PHONE_SCREEN_HEIGHT * state.phoneVisual.screenScale.y, PHONE_SCREEN_DEPTH}, phoneOrientation, phoneScreen);
    }
    if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.playerId!=state.multiplayer.localPlayerId&&peer.player.alive){const float remoteBody[4]={0.32f,0.86f,1.0f,1.0f},remoteScreen[4]={0.05f,0.55f,0.78f,1.0f};if(phoneModel_.valid())drawStaticModel(viewProj,phoneModel_,phoneVbo_,phoneNormalVbo_,peer.phoneTransform.position,peer.phoneVisual.bodyScale,peer.phoneTransform.orientation);else drawBox(viewProj,peer.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},peer.phoneTransform.orientation,remoteBody);drawBox(viewProj,peer.phoneTransform.screenCenter,{PHONE_SCREEN_WIDTH,PHONE_SCREEN_HEIGHT,PHONE_SCREEN_DEPTH},peer.phoneTransform.orientation,remoteScreen);}

    const MeleeVisualState& melee=state.meleeVisual;
    if(melee.visualTimer>0.0f && !melee.locomotionLunge){
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
    const float targetTileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(int offset=-1;offset<=1;++offset)for (auto target : state.targets) {
        if (!target.alive) continue;
        const float originalZ=target.pos.z;
        target.pos.z=targetTileOrigin+static_cast<float>(offset)*ROOM_DEPTH+(originalZ-std::floor((originalZ+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);
        const float mirrorShift=target.pos.z-originalZ;
        target.tetherAnchor.z+=mirrorShift;target.tetherDestination.z+=mirrorShift;target.latchPoint.z+=mirrorShift;
        if (!target.slurpable || target.soulCubeAmount < 0.995f) {
            if(humanModel_.valid())drawHumanModel(viewProj,target,state.time);else drawProceduralHuman(viewProj, target, state.time, targetColor);
        }
        if (target.slurpable && target.soulCubeAmount > 0.001f) {
            if (!target.soulVisual.visible) continue;
            const auto& sv=target.soulVisual; const Vec3 soulCenter=target.pos+Vec3{0.0f,0.57f+sv.verticalOffset,0.0f}; const float cube=0.72f*0.78f*target.scale*sv.morphScale;
            drawSoulFlesh(viewProj,target,soulCenter);
            const float soulColor[4] = {sv.color.r, sv.color.g, sv.color.b, 0.68f};
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
            drawBox(viewProj, soulCenter, {cube*sv.scale.x,cube*sv.scale.y,cube*sv.scale.z}, sv.rotationY, soulColor);
            glDepthMask(GL_TRUE); glDisable(GL_BLEND);
        }
    }

    const float flowerColor[4]={Pass7Visual::Flower.r,Pass7Visual::Flower.g,Pass7Visual::Flower.b,1.0f};
    const float flowerCore[4]={Pass7Visual::FlowerCore.r,Pass7Visual::FlowerCore.g,Pass7Visual::FlowerCore.b,1.0f};
    const float flowerTileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(const auto& flower:state.flowers){
        if(!flower.active) continue;
        for(int offset=-1;offset<=1;++offset){const Vec3 center{flower.pos.x,flower.pos.y,flower.pos.z+flowerTileOrigin+static_cast<float>(offset)*ROOM_DEPTH};
        if(flowerModel_.valid()) drawStaticModel(viewProj,flowerModel_,flowerVbo_,flowerNormalVbo_,center,{1,1,1},quatAxisAngle({0,1,0},flower.rotationY));
        else {drawBox(viewProj,center,{0.20f,0.20f,0.20f},flower.rotationY,flowerCore);for(int petal=0;petal<5;++petal){const float angle=flower.rotationY+static_cast<float>(petal)*DB_PI*2.0f/5.0f;const Vec3 p=center+Vec3{std::cos(angle)*0.23f,0,std::sin(angle)*0.23f};drawBox(viewProj,p,{0.30f,0.12f,0.16f},-angle,flowerColor);}}
        }
    }

    const float captureTileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(int offset=-1;offset<=1;++offset)for (int captureIndex=0;captureIndex<state.requiredSouls;++captureIndex) {
        const auto& capture=state.captures[captureIndex];
        const Vec3 capturePos=capture.pos+Vec3{0,0,captureTileOrigin+static_cast<float>(offset)*ROOM_DEPTH};
        const float frameColor[4]={0.36f,0.42f,0.46f,1.0f};
        const float holeColor[4]={0.02f,0.03f,0.04f,1.0f};
        drawBox(viewProj,capturePos+Vec3{0,0,-0.04f},{0.72f,0.72f,0.06f},0.0f,frameColor);
        drawBox(viewProj,capturePos,{0.52f,0.52f,0.08f},0.0f,holeColor);
        if(capture.filled){const float soul[4]={Pass7Visual::SoulBase.r,Pass7Visual::SoulBase.g,Pass7Visual::SoulBase.b,1.0f}; drawBox(viewProj,capturePos+Vec3{0,0,0.12f},{0.36f,0.36f,0.36f},state.time*2.0f,soul);}
    }

    const float bulletColor[4] = {1.0f, 0.92f, 0.45f, 1.0f};
    for (const auto& bullet : state.bullets) {
        if (!bullet.alive) continue;
        const float size=0.72f*1.12f*(bullet.brute?1.7f:1.0f);
        drawBox(viewProj, bullet.pos, {size,size,size}, bullet.spin*1.7f, bulletColor);
    }
    const float particleColor[4]={1.0f,0.267f,0.267f,0.9f};
    for(const auto& particle:state.particles) if(particle.life>0.0f) {
        const float t=particle.maxLife>0.0f?clampf(particle.life/particle.maxLife,0.0f,1.0f):0.0f;
        const float size=0.08f*t;
        drawBox(viewProj,particle.pos,{size,size,size},particle.life*4.0f,particleColor);
    }
    drawDoorDataMosh(state);
    drawHud(state);
}
