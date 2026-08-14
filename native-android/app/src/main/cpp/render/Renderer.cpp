#include "Renderer.hpp"
#include "../game/HumanVisual.hpp"
#include "../game/BitmapFont.hpp"
#include "../game/EarlyBrowserVisuals.hpp"

#include <GLES2/gl2.h>
#include <android/log.h>
#include <array>
#include <cmath>
#include <chrono>
#include <cstring>
#include <string>

namespace {
float displayedMobileFps=60.0f;
int mobileFpsFrames=0;
auto mobileFpsWindowStart=std::chrono::steady_clock::now();
constexpr float ROOM_WIDTH = 30.0f;
constexpr float ROOM_DEPTH = 42.0f;
constexpr int ROOM_VISUAL_HORIZON = 0;

constexpr int ROUNDED_SEGMENTS = 7;
constexpr int ROUNDED_RINGS = 5;
constexpr int ROUNDED_VERTEX_COUNT = ROUNDED_SEGMENTS * (ROUNDED_RINGS - 1) * 6;
constexpr int FX_SEGMENTS=12;
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
    "varying float vFog;\n"
    "void main() {\n"
    "  vec3 radial = normalize(aPos + vec3(0.0001));\n"
    "  vec3 modelNormal = normalize(mat3(uModel) * aNormal + vec3(0.0001));\n"
    "  vec3 n = normalize(mix(radial, modelNormal, uUseNormal));\n"
    "  float sun = max(dot(n, normalize(vec3(0.42, 0.84, 0.35))), 0.0);\n"
    "  float fill = max(dot(n, normalize(vec3(-0.46, 0.57, -0.68))), 0.0);\n"
    "  vLight = uUseNormal < -0.5 ? 1.0 : clamp(0.48 + sun * 0.42 + fill * 0.10, 0.0, 1.0);\n"
    "  gl_Position = uMvp * vec4(aPos, 1.0);\n"
    "  vFog = smoothstep(0.72, 0.99, gl_Position.z / max(gl_Position.w, 0.001));\n"
    "}\n";

const char* FRAG_SRC =
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "uniform float uUseNormal;\n"
    "varying float vLight;\n"
    "varying float vFog;\n"
    "void main() {\n"
    "  vec3 lit = uUseNormal > 0.5 ? uColor.rgb * vLight : uColor.rgb;\n"
    "  float luma = dot(lit, vec3(0.2126, 0.7152, 0.0722));\n"
    "  vec3 saturated = mix(vec3(luma), lit, 1.10);\n"
    "  vec3 graded = clamp((saturated - 0.5) * 1.06 + 0.5, 0.0, 1.0);\n"
    "  vec3 atmospheric = mix(uUseNormal > -0.5 ? graded : lit, vec3(0.557,0.792,0.902), vFog);\n"
    "  gl_FragColor = vec4(atmospheric, uColor.a);\n"
    "}\n";

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

void Renderer::drawGrassBatch(const float* viewProj,const GameState& state,int tileIndex){
    constexpr int maxVertices=early_browser_visuals::GrassBladeCountHigh*6;std::array<float,maxVertices*3> vertices{};
    const auto plan=early_browser_visuals::roomPlan(state.roomSeed,state.roomIndex);
    const int budget=state.localSettings.graphicsPreset<=0?early_browser_visuals::GrassBladeCountLow:early_browser_visuals::GrassBladeCountHigh;
    const int count=static_cast<int>(static_cast<float>(budget)*plan.grassAmount);
    const float z0=static_cast<float>(tileIndex)*ROOM_DEPTH;int out=0;
    early_browser_visuals::GrassReactionInputs reaction{state.player.pos,state.phoneTransform.vacuumPullPoint,state.environmentVisual.latestShotOrigin,state.vacuum.power,state.environmentVisual.latestShotAge};
    const auto emit=[&](const Vec3& p){vertices[out++]=p.x;vertices[out++]=p.y;vertices[out++]=p.z;};
    for(int i=0;i<count;++i){auto blade=early_browser_visuals::grassBlade(state.roomSeed,state.roomIndex,tileIndex,i);blade.root.z+=z0;const Vec3 tip=early_browser_visuals::grassTip(blade,state.time,reaction),side{std::cos(blade.phase)*blade.width*0.5f,0,std::sin(blade.phase)*blade.width*0.5f};const Vec3 rootL=blade.root-side,rootR=blade.root+side,tipL=tip-side*0.62f,tipR=tip+side*0.62f;emit(rootL);emit(rootR);emit(tipR);emit(rootL);emit(tipR);emit(tipL);}
    float identity[16];ident(identity);const float color[4]={0.290f,0.486f,0.349f,1.0f};glUseProgram(program_);glUniform1f(uUseNormal_,0.0f);glUniformMatrix4fv(uMvp_,1,GL_FALSE,viewProj);glUniformMatrix4fv(uModel_,1,GL_FALSE,identity);glUniform4fv(uColor_,1,color);glBindBuffer(GL_ARRAY_BUFFER,0);glEnableVertexAttribArray(static_cast<GLuint>(aPos_));glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,vertices.data());glDrawArrays(GL_TRIANGLES,0,count*6);
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

void Renderer::drawCheapHuman(const float* viewProj, const TargetState& target, const float color[4]) {
    const float s = target.scale;
    const float yaw = target.visualYaw + DB_PI;
    const float bob = target.slurpable ? 0.0f : std::sin(target.phase) * 0.018f;
    const Vec3 root = target.pos + Vec3{0.0f, 0.06f + bob, 0.0f};
    const bool brute = target.brute;
    const VisualColor body = brute ? Pass7Visual::BruteEnemy : Pass7Visual::NormalEnemy;
    const float armorMax=brute?4.0f:2.0f;
    const float damage=1.0f-clampf(target.armor/std::max(0.001f,armorMax),0.0f,1.0f);
    const float flash=clampf(target.hitFlash,0.0f,1.0f);
    const float shellAlpha=color[3]*(0.94f-damage*0.48f);
    const float bodyColor[4] = {
        body.r+(1.0f-body.r)*flash*0.42f,
        body.g+(0.86f-body.g)*flash*0.38f,
        body.b+(0.62f-body.b)*flash*0.30f,
        shellAlpha
    };
    if(shellAlpha<0.995f){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);}
    drawBox(viewProj, root + Vec3{0.0f, 0.54f * s, 0.0f}, {0.24f * s, 0.56f * s, 0.18f * s}, yaw, bodyColor);
    drawBox(viewProj, root + Vec3{0.0f, 1.02f * s, 0.0f}, {0.18f * s, 0.18f * s, 0.16f * s}, yaw, bodyColor);
    if (brute) drawBox(viewProj, root + Vec3{0.0f, 0.78f * s, 0.0f}, {0.34f * s, 0.18f * s, 0.20f * s}, yaw, bodyColor);
    if(shellAlpha<0.995f){glDepthMask(GL_TRUE);glDisable(GL_BLEND);}
    if(damage>0.06f||target.attackTimer>0.0f){
        const float pulse=0.68f+0.32f*std::sin(target.phase*1.9f);
        const float weak[4]={Pass7Visual::ElectricMagenta.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.58f+damage*0.34f+flash*0.08f};
        const Vec3 forward{-std::sin(yaw),0.0f,-std::cos(yaw)};
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
        drawBox(viewProj, root + Vec3{0.0f, (1.02f+0.02f*pulse) * s, 0.0f} + forward*(0.095f*s), {0.075f * s*(1.0f+damage*0.22f), 0.075f * s*(1.0f+damage*0.22f), 0.026f * s}, yaw, weak);
        glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    }
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

void Renderer::drawRoomTile(const float* viewProj, const GameState& state, int tileIndex) {
    const float z0=static_cast<float>(tileIndex)*ROOM_DEPTH;
    const auto plan=early_browser_visuals::roomPlan(state.roomSeed,state.roomIndex);
    const bool field=plan.setting==early_browser_visuals::RoomSetting::Field;
    const bool sterile=plan.setting==early_browser_visuals::RoomSetting::Sterile;
    const float groundColor[4] = {field?0.247f:(sterile?0.48f:Pass7Visual::RoomFloor.r),field?0.455f:(sterile?0.50f:Pass7Visual::RoomFloor.g),field?0.282f:(sterile?0.52f:Pass7Visual::RoomFloor.b),1.0f};
    drawBox(viewProj, {0.0f, -0.04f, z0}, {ROOM_WIDTH, 0.08f, ROOM_DEPTH}, 0.0f, groundColor);

    const float wallColor[4] = {Pass7Visual::RoomWall.r, Pass7Visual::RoomWall.g, Pass7Visual::RoomWall.b, 1.0f};
    const float doorWidth=5.35f,doorHeight=3.95f,wallHeight=7.2f;
    const float sideWidth=(ROOM_WIDTH-doorWidth)*0.5f;
    const float sideX=doorWidth*0.5f+sideWidth*0.5f;
    const float topHeight=wallHeight-doorHeight;
    const float topY=doorHeight+topHeight*0.5f;
    if(sterile) drawBox(viewProj,{0,wallHeight+0.08f,z0},{ROOM_WIDTH,0.16f,ROOM_DEPTH},0,wallColor);
    for(float seam:{-ROOM_DEPTH*0.5f,ROOM_DEPTH*0.5f}) {
        drawBox(viewProj,{-sideX,wallHeight*0.5f,z0+seam},{sideWidth,wallHeight,0.5f},0,wallColor);
        drawBox(viewProj,{sideX,wallHeight*0.5f,z0+seam},{sideWidth,wallHeight,0.5f},0,wallColor);
        drawBox(viewProj,{0,topY,z0+seam},{doorWidth,topHeight,0.5f},0,wallColor);
    }
    drawBox(viewProj,{-ROOM_WIDTH*0.5f,wallHeight*0.5f,z0},{0.5f,wallHeight,ROOM_DEPTH},0,wallColor);
    drawBox(viewProj,{ROOM_WIDTH*0.5f,wallHeight*0.5f,z0},{0.5f,wallHeight,ROOM_DEPTH},0,wallColor);
    const float obstacleColor[4]={Pass7Visual::RoomObstacle.r,Pass7Visual::RoomObstacle.g,Pass7Visual::RoomObstacle.b,1.0f};
    for(int i=0;i<std::min(state.debug.colliderCount,plan.obstacleCount);++i){const RoomCollider& collider=state.roomColliders[i]; drawBox(viewProj,{collider.center.x,collider.center.y,z0+collider.center.z},{collider.width,collider.height,collider.depth},0,obstacleColor);}
    if(plan.sidewalks){const float sidewalk[4]={0.32f,0.34f,0.36f,1.0f};drawBox(viewProj,{-5.2f,0.08f,z0},{2.0f,0.16f,ROOM_DEPTH},0,sidewalk);drawBox(viewProj,{5.2f,0.08f,z0},{2.0f,0.16f,ROOM_DEPTH},0,sidewalk);}
    for(int i=0;i<early_browser_visuals::environmentPropCount(plan);++i){
        const auto prop=early_browser_visuals::environmentProp(plan,state.roomSeed,state.roomIndex,i);const Vec3 p=prop.center+Vec3{0,0,z0};using early_browser_visuals::EnvironmentPrimitive;
        const float structure[4]={0.40f,0.47f,0.50f,1},roof[4]={0.28f,0.34f,0.38f,1},dark[4]={0.05f,0.08f,0.09f,1},trunk[4]={0.25f,0.20f,0.14f,1},leaf[4]={0.18f,0.42f,0.24f,1},lawn[4]={0.20f,0.39f,0.23f,1},marker[4]={0.48f,0.55f,0.58f,1},cap[4]={0.72f,0.90f,0.94f,1};
        if(prop.primitive==EnvironmentPrimitive::House){const float w=prop.size.x,h=prop.size.y,d=prop.size.z;drawBox(viewProj,p+Vec3{0,h*0.38f,0},{w,h*0.76f,d},prop.yaw,structure);drawBox(viewProj,p+Vec3{0,h*0.86f,0},{w*0.88f,h*0.20f,d*0.90f},prop.yaw,roof);drawBox(viewProj,p+Vec3{0,h*1.03f,0},{w*0.62f,h*0.16f,d*0.72f},prop.yaw,roof);drawBox(viewProj,p+Vec3{std::sin(prop.yaw)*d*0.505f,h*0.25f,std::cos(prop.yaw)*d*0.505f},{w*0.22f,h*0.42f,0.035f},prop.yaw,dark);}
        else if(prop.primitive==EnvironmentPrimitive::Tree){drawBox(viewProj,p+Vec3{0,prop.size.y*0.35f,0},{prop.size.x*0.20f,prop.size.y*0.70f,prop.size.z*0.20f},prop.yaw,trunk);drawBox(viewProj,p+Vec3{0,prop.size.y*0.88f,0},{prop.size.x,prop.size.y*0.72f,prop.size.z},prop.yaw,leaf);}
        else if(prop.primitive==EnvironmentPrimitive::LawnFragment)drawBox(viewProj,p,prop.size,prop.yaw,lawn);
        else {drawBox(viewProj,p+Vec3{0,prop.size.y*0.5f,0},prop.size,prop.yaw,marker);drawBox(viewProj,p+Vec3{0,prop.size.y+0.08f,0},{prop.size.x*1.28f,0.16f,prop.size.z*1.28f},prop.yaw,cap);}
    }
    if(plan.grass) drawGrassBatch(viewProj,state,tileIndex);
}

void Renderer::drawHud(const GameState& state) {
    float identity[16]; ident(identity);
    float overlayAlpha=1.0f;
    const bool cheapVisuals = state.localSettings.graphicsPreset <= 0;
    glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    const auto quad=[&](float x,float y,float w,float h,const float color[4],float angle=0.0f){
        const Vec3 center{-1.0f+(x+w*0.5f)*2.0f/static_cast<float>(width_),1.0f-(y+h*0.5f)*2.0f/static_cast<float>(height_),0.0f};
        const Vec3 scale{w*2.0f/static_cast<float>(width_),h*2.0f/static_cast<float>(height_),0.01f};
        const float faded[4]={color[0],color[1],color[2],color[3]*overlayAlpha};drawBox(identity,center,scale,quatAxisAngle({0,0,1},-angle),faded);
    };
    const auto pixelRotatedQuad=[&](float cx,float cy,float w,float h,float angle,const float color[4]){
        const float c=std::cos(angle),s=std::sin(angle),hx=w*0.5f,hy=h*0.5f;
        const float corners[8]={-hx,-hy,hx,-hy,hx,hy,-hx,hy};
        const int order[6]={0,1,2,0,2,3};float vertices[18]{};
        for(int i=0;i<6;++i){const int k=order[i]*2;const float px=cx+corners[k]*c-corners[k+1]*s,py=cy+corners[k]*s+corners[k+1]*c;vertices[i*3]=-1.0f+px*2.0f/width_;vertices[i*3+1]=1.0f-py*2.0f/height_;}
        const float faded[4]={color[0],color[1],color[2],color[3]*overlayAlpha};glUseProgram(program_);glUniform1f(uUseNormal_,-1.0f);glUniformMatrix4fv(uMvp_,1,GL_FALSE,identity);glUniformMatrix4fv(uModel_,1,GL_FALSE,identity);glUniform4fv(uColor_,1,faded);glBindBuffer(GL_ARRAY_BUFFER,0);glEnableVertexAttribArray(static_cast<GLuint>(aPos_));glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,vertices);glDrawArrays(GL_TRIANGLES,0,6);
    };
    const auto text=[&](const std::string& value,float x,float y,float scale,const float color[4]){float pen=x;for(char c:value){if(c==' '){pen+=6*scale;continue;}const auto rows=bitmapGlyph(c);for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1u<<(4-col))){const float px=pen+col*scale,py=y+row*scale;if(overlayAlpha<0.999f){const float outline[4]={0,0,0,color[3]};quad(px-1,py,scale,scale,outline);quad(px+1,py,scale,scale,outline);quad(px,py-1,scale,scale,outline);quad(px,py+1,scale,scale,outline);}quad(px,py,scale,scale,color);}pen+=6*scale;}};
    const auto floatingText=[&](const std::string& value,float x,float y,float scale,const float color[4]){const float pulse=clampf(state.cinematic.textInteraction,0.0f,1.0f),tracking=pulse*0.9f*std::min(scale,2.0f),wave=0.48f+pulse*0.78f;const float shadow[4]={0,0,0,0.72f*color[3]};float pen=x-tracking*std::max(0.0f,(static_cast<float>(value.size())-1.0f)*0.5f);for(std::size_t i=0;i<value.size();++i){const float offset=std::sin(state.time*4.2f+static_cast<float>(i)*0.68f)*wave;const std::string glyph(1,value[i]);text(glyph,pen+2.0f,y+offset+2.0f,scale,shadow);text(glyph,pen,y+offset,scale,color);pen+=6.0f*scale+tracking;}};
    const auto rainbow=[&](float hue){hue-=std::floor(hue);const float x=hue*6.0f,i=std::floor(x),f=x-i,q=1.0f-f;switch(static_cast<int>(i)%6){case 0:return Vec3{1,f,0};case 1:return Vec3{q,1,0};case 2:return Vec3{0,1,f};case 3:return Vec3{0,q,1};case 4:return Vec3{f,0,1};default:return Vec3{1,0,q};}};
    const float panel[4]={0.005f,0.012f,0.016f,0.72f}, cyan[4]={Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.95f};
    if(state.localSettings.fpsCounter){const std::string fps="FPS "+std::to_string(static_cast<int>(std::round(displayedMobileFps)));const float whiteFps[4]={Pass7Visual::SignalGreen.r,Pass7Visual::SignalGreen.g,Pass7Visual::SignalGreen.b,0.94f};text(fps,width_-fps.size()*7.2f-12,68,1.2f,whiteFps);}
    if(state.attractMode){
        const float cx=width_*0.5f;
        const std::string title="DATA";
        const float titleScale=5.2f,titleW=title.size()*6.0f*titleScale;
        const float veil[4]={0,0,0,0.10f};
        quad(0,0,static_cast<float>(width_),static_cast<float>(height_),veil);
        float pen=cx-titleW*0.5f;
        for(std::size_t i=0;i<title.size();++i){const Vec3 rgb=rainbow(state.time*0.026f+static_cast<float>(i)*0.115f);const float color[4]={0.55f+rgb.x*0.42f,0.65f+rgb.y*0.34f,0.72f+rgb.z*0.28f,0.96f};text(std::string(1,title[i]),pen,height_*0.16f,titleScale,color);pen+=6.0f*titleScale;}
        glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);return;
    }
    if(state.cinematic.introActive){const float alpha=1.0f-smoothStep01(clampf(state.cinematic.introElapsed/0.42f,0.0f,1.0f));const float pw=static_cast<float>(width_)*0.72f,ph=static_cast<float>(height_)*0.25f,px=(width_-pw)*0.5f,py=(height_-ph)*0.5f,ss=std::max(2.0f,std::min(width_,height_)/240.0f);const float fading[4]={0.92f,0.97f,1,0.94f*alpha};const std::string status="READY",action="START";floatingText(status,px+(pw-status.size()*6*ss)*0.5f,py+ph*0.30f,ss,fading);const float startScale=ss*0.8f;floatingText(action,px+(pw-action.size()*6*startScale)*0.5f,py+ph*0.60f+10,startScale,fading);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);return;}
    if(!state.started) {
        const float pw=static_cast<float>(width_)*0.72f,ph=static_cast<float>(height_)*0.25f,px=(width_-pw)*0.5f,py=(height_-ph)*0.5f;
        const float menuAlpha=state.dead?smoothStep01(clampf(state.cinematic.deathElapsed/0.45f,0.0f,1.0f)):1.0f;const float white[4]={0.92f,0.97f,1,0.94f*menuAlpha};const std::string action=state.dead?"TAP TO RESTART":"START";const float startScale=std::max(2.0f,std::min(width_,height_)/240.0f)*(state.dead?1.0f:0.8f);floatingText(action,px+(pw-action.size()*6*startScale)*0.5f,py+ph*0.46f,startScale,white);
        glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); return;
    }
    if(state.camera.spectatedPlayerId>=0){
        const std::string label="SPECTATING  P"+std::to_string(state.camera.spectatedPlayerId+1);
        const float scale=1.35f,tw=label.size()*6.0f*scale,pw=tw+24.0f,px=(width_-pw)*0.5f;
        const float spectatorPanel[4]={0.005f,0.012f,0.016f,0.62f};
        quad(px,18,pw,24,spectatorPanel);
        text(label,px+12.0f,25,scale,cyan);
        glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);return;
    }
    const int goals=std::max(1,state.hud.requiredGoals); const float gs=22,gap=8,total=goals*gs+(goals-1)*gap,start=(width_-total)*0.5f;
    for(int i=0;i<goals;++i){const float empty[4]={0.01f,0.02f,0.025f,0.90f}; const float filled[4]={Pass7Visual::AcidChartreuse.r,Pass7Visual::AcidChartreuse.g,Pass7Visual::AcidChartreuse.b,0.95f}; quad(start+i*(gs+gap),18,gs,gs,cyan); quad(start+i*(gs+gap)+2,20,gs-4,gs-4,i<state.hud.filledGoals?filled:empty);}
    quad(12,74,120,82,panel);
    constexpr int mosaicColumns=5,mosaicRows=5,mosaicCells=mosaicColumns*mosaicRows;
    const int filledSoulPixels=state.hud.storedSouls<=0?0:std::max(1,static_cast<int>(std::ceil(state.hud.storedSouls/static_cast<float>(PHONE_CAPACITY)*mosaicCells)));
    const bool tvPreview=state.roomIndex==10&&tvGifWall_.available();
    const float tile=9.0f,mosaicGap=2.0f,mosaicW=mosaicColumns*tile+(mosaicColumns-1)*mosaicGap,mosaicX=12.0f+(120.0f-mosaicW)*0.5f,mosaicY=96.0f;
    for(int i=0;i<mosaicCells;++i){const int col=i%mosaicColumns,row=i/mosaicColumns;VisualColor c=Pass7Visual::DataMosaicPalette[i];if(tvPreview){const auto tv=tvGifWall_.sample(col*(TvGifWall::Columns-1)/(mosaicColumns-1),row*(TvGifWall::Rows-1)/(mosaicRows-1),0.0f,state.secretTv.signal);c={tv.r,tv.g,tv.b};}const bool filled=i<filledSoulPixels;const float shade=filled?1.0f:0.22f,alpha=filled?0.92f:0.20f,tileColor[4]={c.r*shade,c.g*shade,c.b*shade,alpha},x=mosaicX+col*(tile+mosaicGap),y=mosaicY+row*(tile+mosaicGap);quad(x,y,tile,tile,tileColor);if(filled&&i==filledSoulPixels-1){const float pulse=0.5f+0.5f*std::sin(state.time*8.0f),edge[4]={c.r,c.g,c.b,0.28f+0.18f*pulse};quad(x-1,y-1,tile+2,1,edge);quad(x-1,y+tile,tile+2,1,edge);}}
    const float white[4]={1,1,1,0.94f},soft[4]={0.78f,0.94f,1,0.82f},green[4]={Pass7Visual::SignalGreen.r,Pass7Visual::SignalGreen.g,Pass7Visual::SignalGreen.b,0.94f};text("SOULS "+std::to_string(state.hud.storedSouls),20,80,1.2f,soft);text("ROOM: "+std::to_string(state.roomIndex),12,170,1.5f,white);text("GOALS: "+std::to_string(state.hud.filledGoals)+"/"+std::to_string(state.hud.requiredGoals),12,187,1.5f,white);text(state.roomClear?"DOOR: OPEN":"DOOR: LOOP",12,204,1.5f,state.roomClear?green:white);text("TOKENS: "+std::to_string(state.progression.permanent.tokens),12,221,1.25f,green);
    if(state.progression.run.accuracyStacks>0){char accuracy[32]{};std::snprintf(accuracy,sizeof(accuracy),"ACCURACY X%.2F",state.progression.run.accuracyMultiplier);text(accuracy,12,304,1.15f,green);}
    if(state.progression.run.headshotRegenTax>0.01f){char tax[32]{};std::snprintf(tax,sizeof(tax),"REGEN -%d%%",static_cast<int>(std::round(state.progression.run.headshotRegenTax*100.0f)));const float warm[4]={1.0f,0.72f,0.62f,0.94f};text(tax,12,320,1.05f,warm);}
    if(state.hud.buildLabel[0])text(state.hud.buildLabel.data(),12,336,1.0f,soft);
    overlayAlpha=state.hud.critMarkerOpacity;
    if(!cheapVisuals){const Vec3 forward=normalized(state.camera.lookTarget-state.camera.pos),right=normalized(cross(forward,{0,1,0})),up=cross(right,forward);const float tanHalf=std::tan(state.camera.verticalFovDegrees*DB_PI/360.0f),aspect=static_cast<float>(width_)/std::max(1,height_);constexpr char glyphs[]="01ABCDEFHIKMNPRSTXYZ+-/:";for(int i=0;i<TARGET_COUNT;++i){const TargetState& target=state.targets[i];if(!target.alive||target.slurpable)continue;const float attackT=target.attackTimer>0?1-clampf(target.attackTimer/HUMAN_SWING_ATTACK_DURATION,0,1):-1.0f,attackBob=target.attackTimer>0?std::sin(attackT*DB_PI)*0.035f*(target.attackVariant>=2?1.0f:0.0f):0;const Vec3 world{target.pos.x,(PASS7_HUMAN_VISUAL_SPEC.totalHeight-PASS7_HUMAN_VISUAL_SPEC.headRadius)*target.scale+attackBob,target.pos.z},delta=world-state.camera.pos;const float depth=dot(delta,forward);if(depth<=0.18f||depth>16.0f)continue;const float nx=dot(delta,right)/(depth*tanHalf*aspect),ny=dot(delta,up)/(depth*tanHalf);if(std::abs(nx)>1.04f||std::abs(ny)>1.04f)continue;const float armorMax=target.brute?4.0f:2.0f,damage=1.0f-clampf(target.armor/armorMax,0,1);const bool perfectReady=attackT>=0.22f&&attackT<=0.46f;const Vec3 rgb=rainbow(0.51f+damage*0.38f);const int cycle=(static_cast<int>(state.time*10.0f)+i*7+state.roomIndex*3)%static_cast<int>(sizeof(glyphs)-1);const float perspectiveScale=clampf(8.0f/depth,0.82f,1.55f),marker=(11.0f+damage*4.0f+(perfectReady?3.0f:0))*perspectiveScale,scale=(1.35f+damage*0.28f+(perfectReady?0.18f:0))*perspectiveScale,sx=(nx*0.5f+0.5f)*width_,sy=(0.5f-ny*0.5f)*height_,alpha=0.72f+damage*0.20f+(perfectReady?0.08f:0),spin=state.time*0.9f+i*0.37f;const float core[4]={rgb.x,rgb.y,rgb.z,0.10f+damage*0.08f},line[4]={rgb.x,rgb.y,rgb.z,alpha},shadow[4]={0,0,0,alpha*0.85f};pixelRotatedQuad(sx,sy,marker,marker,DB_PI*0.25f+spin,core);pixelRotatedQuad(sx-marker,sy,marker*0.52f,2,spin*0.08f,line);pixelRotatedQuad(sx+marker,sy,marker*0.52f,2,spin*0.08f,line);pixelRotatedQuad(sx,sy-marker,2,marker*0.52f,spin*0.08f,line);pixelRotatedQuad(sx,sy+marker,2,marker*0.52f,spin*0.08f,line);const float color[4]={rgb.x,rgb.y,rgb.z,alpha};text(std::string(1,glyphs[cycle]),sx-2.5f*scale+1,sy-3.5f*scale+1,scale,shadow);text(std::string(1,glyphs[cycle]),sx-2.5f*scale,sy-3.5f*scale,scale,color);}}
    overlayAlpha=1.0f;
    {const auto labelFor=[](int signal)->const char*{switch(signal){case 1:return "HELP";case 2:return "PING";case 3:return "GROUP";case 4:return "OK";default:return "";}};const auto colorFor=[](int signal)->VisualColor{switch(signal){case 1:return Pass7Visual::ElectricMagenta;case 2:return Pass7Visual::ElectricCyan;case 3:return Pass7Visual::AcidChartreuse;case 4:return Pass7Visual::WarmGold;default:return Pass7Visual::ElectricCyan;}};const Vec3 forward=normalized(state.camera.lookTarget-state.camera.pos),right=normalized(cross(forward,{0,1,0})),up=cross(right,forward);const float tanHalf=std::tan(state.camera.verticalFovDegrees*DB_PI/360.0f),aspect=static_cast<float>(width_)/std::max(1,height_);const auto drawSignal=[&](const PlayerState& player){if(player.commSignal<1||player.commSignal>4||player.commSignalTimer<=0.0f)return;const Vec3 world=player.pos+Vec3{0,1.05f,0},delta=world-state.camera.pos;const float depth=dot(delta,forward);if(depth<=0.18f||depth>24.0f)return;const float nx=dot(delta,right)/(depth*tanHalf*aspect),ny=dot(delta,up)/(depth*tanHalf);if(std::abs(nx)>1.08f||std::abs(ny)>1.08f)return;const char* label=labelFor(player.commSignal);const VisualColor c=colorFor(player.commSignal);const float sx=(nx*0.5f+0.5f)*width_,sy=(0.5f-ny*0.5f)*height_,fade=clampf(player.commSignalTimer/0.35f,0.0f,1.0f),scale=clampf(9.0f/depth,1.15f,2.15f),tw=std::strlen(label)*6.0f*scale,pw=tw+18.0f*scale,ph=13.0f*scale,pulse=0.5f+0.5f*std::sin(state.time*8.0f);const float panelCallout[4]={Pass7Visual::DeepPlum.r*0.12f,Pass7Visual::DeepPlum.g*0.12f,Pass7Visual::DeepPlum.b*0.12f,0.52f*fade},edgeCallout[4]={c.r,c.g,c.b,(0.58f+0.18f*pulse)*fade},labelCallout[4]={c.r,c.g,c.b,0.96f*fade};quad(sx-pw*0.5f,sy-ph*0.5f,pw,ph,panelCallout);quad(sx-pw*0.5f,sy-ph*0.5f,pw,1.4f*scale,edgeCallout);text(label,sx-tw*0.5f,sy-3.5f*scale,scale,labelCallout);};drawSignal(state.player);for(const auto& peer:state.multiplayer.peers)if(peer.active)drawSignal(peer.player);}
    if(state.hud.headshotPulse>0.001f){const float charge=clampf(state.hud.headshotKillCharge,0,1),pulse=state.hud.headshotPulse,eased=pulse*pulse,w=static_cast<float>(width_),h=static_cast<float>(height_),breath=0.5f+0.5f*std::sin(state.time*2.4f),mist=10.0f+charge*8.0f+state.hud.perfectPulse*4.0f;const Vec3 core=rainbow(0.51f+charge*0.40f+state.progression.run.accuracyStacks*0.012f),accent=rainbow(0.68f+charge*0.22f+state.time*0.014f);const float veilA=eased*(0.035f+charge*0.070f),wispA=pulse*(0.075f+charge*0.105f),sparkA=pulse*(0.10f+charge*0.14f),veilCore[4]={core.x,core.y,core.z,veilA},veilAccent[4]={accent.x,accent.y,accent.z,veilA*0.90f};quad(0,0,w,mist,veilCore);quad(0,h-mist,w,mist,veilAccent);quad(0,0,mist,h,veilAccent);quad(w-mist,0,mist,h,veilCore);for(int i=0;i<3;++i){const float phase=state.time*(0.55f+i*0.17f)+i*2.1f,drift=0.5f+0.5f*std::sin(phase),len=w*(0.22f+0.10f*i+0.08f*breath),thick=1.2f+i*1.1f+charge*1.4f,x=clampf(drift*(w+len)-len,0.0f,w-len),y=clampf((0.5f+0.5f*std::sin(phase*0.81f+1.7f))*(h+len)-len,0.0f,h-len);const Vec3 c=i==1?accent:core;const float top[4]={c.x,c.y,c.z,wispA*(0.72f-0.14f*i)},bottom[4]={c.x,c.y,c.z,wispA*(0.59f-0.11f*i)},sideA[4]={c.x,c.y,c.z,wispA*(0.50f-0.10f*i)},sideB[4]={c.x,c.y,c.z,wispA*(0.46f-0.09f*i)};quad(x,2.0f+i*4.0f,len,thick,top);quad(w-x-len,h-3.0f-i*4.4f,len,thick,bottom);quad(2.0f+i*4.0f,y,thick,len,sideA);quad(w-3.0f-i*4.4f,h-y-len,thick,len,sideB);}const float corner=clampf(std::min(w,h)*0.10f,34.0f,84.0f),cornerCore[4]={core.x,core.y,core.z,sparkA},cornerAccent[4]={accent.x,accent.y,accent.z,sparkA*0.85f},cornerLow[4]={accent.x,accent.y,accent.z,sparkA*0.75f},cornerLowB[4]={core.x,core.y,core.z,sparkA*0.65f};quad(0,0,corner,2.0f,cornerCore);quad(0,0,2.0f,corner,cornerAccent);quad(w-corner,h-2.0f,corner,2.0f,cornerLow);quad(w-2.0f,h-corner,2.0f,corner,cornerLowB);}
    quad(12,236,148,30,panel); const float track[4]={0.025f,0.035f,0.04f,0.95f}; quad(21,250,132,6,track);
    const float battery[4]={state.hud.lowBattery?1.0f:0.92f,state.hud.lowBattery?0.18f:0.97f,state.hud.lowBattery?0.12f:1.0f,0.98f}; quad(21,250,132*clampf(state.hud.batteryFill,0,1),6,battery);
    text("BATTERY",20,240,1.1f,state.hud.lowBattery?battery:white);
    if(state.energy.comboHits>0&&state.time-state.energy.lastComboHitTime<=1.8f){char multiplier[16]{};std::snprintf(multiplier,sizeof(multiplier),"X%.2F",state.energy.comboMultiplier);text(multiplier,112,240,1.0f,green);}
    if(state.hud.flowerStacks>0 || state.hud.supplementalFill>0.001f){const float flower[4]={0.35f,1,0.68f,0.98f};quad(12,274,148,24,panel);quad(21,283,132,6,track);quad(21,283,132*clampf(state.hud.supplementalFill,0,1),6,flower);text("POWER X"+std::to_string(state.hud.flowerStacks),20,276,1.0f,flower);}
    if(state.hud.energyTicker[0]&&state.time<state.hud.energyTickerUntil){const std::string ticker=state.hud.energyTicker.data();const float scale=1.35f,tw=ticker.size()*6*scale,pw=std::max(118.0f,tw+16.0f),px=(width_-pw)*0.5f;const VisualColor tc=state.hud.energyTickerType==1?Pass7Visual::Copper:(state.hud.energyTickerType==0?Pass7Visual::SignalGreen:Pass7Visual::ElectricCyan);const float tickerColor[4]={tc.r,tc.g,tc.b,0.94f};quad(px,72,pw,18,panel);text(ticker,(width_-tw)*0.5f,77,scale,tickerColor);}
    if(state.player.grabbedByTarget>=0){const std::string hint="WIGGLE  LEFT  RIGHT";const float s=1.7f,warm[4]={1.0f,0.82f,0.68f,0.94f};text(hint,(width_-hint.size()*6*s)*0.5f,height_*0.69f,s,warm);}
    if(state.player.downed){const std::string hint="SIGNAL DOWN  "+std::to_string(static_cast<int>(std::ceil(state.player.bleedoutTimer)));const float s=1.8f,cost[4]={1.0f,0.48f,0.42f,0.96f};text(hint,(width_-hint.size()*6*s)*0.5f,height_*0.55f,s,cost);}
    if(state.player.inSecretRoom){const std::string hint=state.secretTv.broken?"NO SIGNAL":"SIGNAL "+std::to_string(state.secretTv.signal)+"   SHOOT TO DONATE";const float s=1.35f,tv[4]={0.72f,0.94f,0.96f,0.88f};text(hint,(width_-hint.size()*6*s)*0.5f,54,s,tv);}
    const float cx=width_*0.5f,cy=height_*0.5f,spread=state.hud.crosshairSpreadPixels,arm=14,thick=3,angle=state.hud.crosshairRotationDegrees*DB_PI/180.0f;
    const float reticle[4]={state.hud.shootJoinTimer>0?1.0f:0.498f,state.hud.shootJoinTimer>0?1.0f:0.906f,1,0.98f*clampf(state.hud.crosshairOpacity,0.0f,1.0f)};
    const auto armQuad=[&](float ox,float oy,float w,float h){const float c=std::cos(angle),s=std::sin(angle);pixelRotatedQuad(cx+ox*c-oy*s,cy+ox*s+oy*c,w,h,angle,reticle);};
    armQuad(0,-spread,thick,arm); armQuad(0,spread,thick,arm); armQuad(-spread,0,arm,thick); armQuad(spread,0,arm,thick);
    if(state.upgradeMenu.active){const float pw=std::min(680.0f,width_-24.0f),ph=300.0f,px=(width_-pw)*0.5f,py=(height_-ph)*0.5f,cellW=(pw-24)/3;const float glass[4]={0.01f,0.03f,0.04f,0.16f},edge[4]={0.62f,0.96f,1.0f,0.62f},button[4]={0.16f,0.86f,1.0f,0.13f};quad(px,py,pw,ph,glass);quad(px,py,pw,1,edge);quad(px,py+ph-1,pw,1,edge);text("ROUND "+std::to_string(state.roomIndex),px+18,py+16,2.0f,white);text("RUN",px+18,py+42,1.35f,green);const char* labels[3]={"SHOT","LUNGE","ATTACK"};for(int i=0;i<3;++i){const float cx=px+12+i*cellW+cellW*0.5f,scale=2.15f;quad(px+14+i*cellW,py+66,cellW-8,76,button);text(labels[i],cx-std::strlen(labels[i])*6*scale*0.5f,py+94,scale,white);}text("PERMANENT  TOKENS "+std::to_string(state.progression.permanent.tokens),px+18,py+158,1.2f,green);for(int i=0;i<3;++i){const float cx=px+12+i*cellW+cellW*0.5f,scale=2.05f;quad(px+14+i*cellW,py+184,cellW-8,66,button);text(labels[i],cx-std::strlen(labels[i])*6*scale*0.5f,py+207,scale,white);const std::string level=std::to_string(state.progression.permanent.levels[i])+"/5";text(level,px+12+(i+1)*cellW-level.size()*6*0.9f-10,py+256,0.9f,soft);}text("COST 1 TOKEN",px+18,py+278,1.05f,soft);}
    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
}

void Renderer::drawHumanModel(const float* viewProj,const TargetState& target,float time,bool shadow){
    humanModel_.skin(target.humanAnimationTime,target.attackTimer,target.attackVariant,humanVertices_);if(humanVertices_.empty()||!humanVbo_)return;
    const float thinning=humanShellThinningAmount(target.armor,target.brute?4.0f:2.0f,target.slurpable);
    if(thinning>0.0f)for(std::size_t i=0;i+8<humanVertices_.size();i+=9){const std::size_t triangle=i/9;const Vec3 a{humanVertices_[i],humanVertices_[i+1],humanVertices_[i+2]},b{humanVertices_[i+3],humanVertices_[i+4],humanVertices_[i+5]},c{humanVertices_[i+6],humanVertices_[i+7],humanVertices_[i+8]},center=(a+b+c)*(1.0f/3.0f);if(humanShellTriangleMissingTowardCrit(triangle,thinning,center)){for(int vertex=1;vertex<3;++vertex)for(int axis=0;axis<3;++axis)humanVertices_[i+vertex*3+axis]=humanVertices_[i+axis];continue;}for(int vertex=0;vertex<3;++vertex){const Vec3 p{humanVertices_[i+vertex*3],humanVertices_[i+vertex*3+1],humanVertices_[i+vertex*3+2]},absorbed=humanShellAbsorbTowardCrit(p,triangle,thinning);humanVertices_[i+vertex*3]=absorbed.x;humanVertices_[i+vertex*3+1]=absorbed.y;humanVertices_[i+vertex*3+2]=absorbed.z;}}
    humanNormals_.assign(humanVertices_.size(),0.0f);for(std::size_t i=0;i+8<humanVertices_.size();i+=9){const Vec3 a{humanVertices_[i],humanVertices_[i+1],humanVertices_[i+2]},b{humanVertices_[i+3],humanVertices_[i+4],humanVertices_[i+5]},c{humanVertices_[i+6],humanVertices_[i+7],humanVertices_[i+8]},n=normalized(cross(b-a,c-a));for(int v=0;v<3;++v){humanNormals_[i+v*3]=n.x;humanNormals_[i+v*3+1]=n.y;humanNormals_[i+v*3+2]=n.z;}}
    const bool aliveHuman=!target.slurpable;const HumanVisualPose pose=makeHumanVisualPose(target.visualYaw,target.scale,time,target.visualReaction,aliveHuman);
    const float t=target.attackTimer>0?1-clampf(target.attackTimer/HUMAN_SWING_ATTACK_DURATION,0,1):0,windup=std::sin(clampf(t/HUMAN_SWING_COMMIT_PHASE,0,1)*DB_PI*0.5f)*(t<HUMAN_SWING_COMMIT_PHASE?1.0f:0.0f),strike=std::sin(clampf((t-HUMAN_SWING_COMMIT_PHASE)/(HUMAN_SWING_END_PHASE-HUMAN_SWING_COMMIT_PHASE),0,1)*DB_PI),side=target.attackVariant%2==0?1.0f:-1.0f,low=target.attackVariant>=2?1.0f:0.0f,reach=target.attackTimer>0?smoothStep01(clampf((t-HUMAN_SWING_COMMIT_PHASE)/(HUMAN_SWING_END_PHASE-HUMAN_SWING_COMMIT_PHASE),0,1)):0.0f;
    const Quat q=quaternionFromEulerXYZ(target.attackTimer>0?windup*0.08f-reach*(0.16f+low*0.05f):0,target.visualYaw+DB_PI,target.attackTimer>0?side*(strike*0.18f-windup*0.24f):0);
    const Vec3 attackForward=lengthSq(target.attackDirection)>0.001f?normalized(target.attackDirection):Vec3{-std::sin(target.visualYaw),0,-std::cos(target.visualYaw)};
    const Vec3 attackLunge=attackForward*(target.attackTimer>0?reach*0.075f*target.scale:0.0f);
    const Vec3 root{target.pos.x+attackLunge.x,target.attackTimer>0?std::sin(t*DB_PI)*0.024f*low:0,target.pos.z+attackLunge.z};float model[16],mvp[16];modelBox(model,root,{pose.scale,pose.scale,pose.scale},q);multiply(mvp,viewProj,model);
    const float shadowColor[4]={0.012f,0.018f,0.022f,0.28f};const bool parryCue=target.attackTimer>0&&t>=0.22f&&t<=0.46f;const float cue=parryCue?(0.10f+0.05f*std::sin(time*28.0f)):0.0f;const float cueColor[4]={humanModel_.color[0]+(0.55f-humanModel_.color[0])*cue,humanModel_.color[1]+(0.96f-humanModel_.color[1])*cue,humanModel_.color[2]+(1.0f-humanModel_.color[2])*cue,humanModel_.color[3]};
    glUseProgram(program_);glUniform1f(uUseNormal_,shadow?-1.0f:1.0f);glUniformMatrix4fv(uModel_,1,GL_FALSE,model);glBindBuffer(GL_ARRAY_BUFFER,humanVbo_);glBufferData(GL_ARRAY_BUFFER,humanVertices_.size()*sizeof(float),humanVertices_.data(),GL_DYNAMIC_DRAW);glEnableVertexAttribArray(static_cast<GLuint>(aPos_));glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,nullptr);glBindBuffer(GL_ARRAY_BUFFER,humanNormalVbo_);glBufferData(GL_ARRAY_BUFFER,humanNormals_.size()*sizeof(float),humanNormals_.data(),GL_DYNAMIC_DRAW);glEnableVertexAttribArray(static_cast<GLuint>(aNormal_));glVertexAttribPointer(static_cast<GLuint>(aNormal_),3,GL_FLOAT,GL_FALSE,0,nullptr);glUniformMatrix4fv(uMvp_,1,GL_FALSE,mvp);glUniform4fv(uColor_,1,shadow?shadowColor:cueColor);glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(humanVertices_.size()/3u));
}

void Renderer::drawSoulFlesh(const float* viewProj,const TargetState& target,const Vec3& center){
    std::array<float,144*3> vertices{};int out=0;
    const auto index=[](int x,int y,int z){return x+y*3+z*9;};
    const auto emit=[&](int node){const Vec3 p=center+target.latticeSurfacePos[node];vertices[out++]=p.x;vertices[out++]=p.y;vertices[out++]=p.z;};
    const auto quad=[&](int a,int b,int c,int d){emit(a);emit(b);emit(c);emit(a);emit(c);emit(d);};
    for(int y=0;y<2;++y)for(int z=0;z<2;++z){quad(index(0,y,z),index(0,y+1,z),index(0,y+1,z+1),index(0,y,z+1));quad(index(2,y,z),index(2,y,z+1),index(2,y+1,z+1),index(2,y+1,z));}
    for(int x=0;x<2;++x)for(int z=0;z<2;++z){quad(index(x,0,z),index(x,0,z+1),index(x+1,0,z+1),index(x+1,0,z));quad(index(x,2,z),index(x+1,2,z),index(x+1,2,z+1),index(x,2,z+1));}
    for(int x=0;x<2;++x)for(int y=0;y<2;++y){quad(index(x,y,0),index(x+1,y,0),index(x+1,y+1,0),index(x,y+1,0));quad(index(x,y,2),index(x,y+1,2),index(x+1,y+1,2),index(x+1,y,2));}
    float identity[16];ident(identity);const float flesh[4]={Pass7Visual::SoulFlesh.r,Pass7Visual::SoulFlesh.g,Pass7Visual::SoulFlesh.b,1.0f};
    glUseProgram(program_);glUniform1f(uUseNormal_,0.0f);glBindBuffer(GL_ARRAY_BUFFER,soulSurfaceVbo_);glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(out*sizeof(float)),vertices.data(),GL_DYNAMIC_DRAW);glEnableVertexAttribArray(static_cast<GLuint>(aPos_));glVertexAttribPointer(static_cast<GLuint>(aPos_),3,GL_FLOAT,GL_FALSE,0,nullptr);glUniformMatrix4fv(uMvp_,1,GL_FALSE,viewProj);glUniform4fv(uColor_,1,flesh);glDrawArrays(GL_TRIANGLES,0,out/3);
    if(target.tetherVisible){const Vec3 delta=target.tetherDestination-target.tetherAnchor;const float len=length(delta);if(len>0.001f){const float yaw=std::atan2(delta.x,delta.z),pitch=-std::asin(clampf(delta.y/len,-1.0f,1.0f));const Quat orientation=quaternionFromEulerXYZ(pitch,yaw,0);const float tether[4]={Pass7Visual::Tether.r,Pass7Visual::Tether.g,Pass7Visual::Tether.b,0.34f};glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);drawBox(viewProj,target.tetherAnchor+delta*0.5f,{0.13f*target.tetherWidth,0.13f*target.tetherWidth,std::min(len,4.5f)},orientation,tether);glDepthMask(GL_TRUE);glDisable(GL_BLEND);}}
}

void Renderer::drawDoorDataMosh(const GameState& state){
    if(!state.localSettings.portalWindow||!datamoshProgram_||!datamoshTexture_)return;glBindTexture(GL_TEXTURE_2D,datamoshTexture_);
    if(!state.doorTransition.active||state.doorTransition.progress<=0.018f){glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,width_,height_,0);datamoshFrameReady_=true;return;}if(!datamoshFrameReady_)return;
    constexpr int strips=24,passes=3,floatsPerVertex=4,verticesPerQuad=6;std::array<float,strips*passes*verticesPerQuad*floatsPerVertex> data{};int out=0;const float strength=clampf(state.doorTransition.progress,0,1);
    const auto vertex=[&](float x,float y,float u,float v){data[out++]=x;data[out++]=y;data[out++]=u;data[out++]=v;};
    for(int row=0;row<strips;++row){const float y0=-1+2.0f*row/strips,y1=-1+2.0f*(row+1)/strips,v0=static_cast<float>(row)/strips,v1=static_cast<float>(row+1)/strips,phase=state.time*5.1f+row*1.73f,shiftPixels=(std::sin(phase)*7+std::sin(phase*0.37f)*13)*strength;for(int pass=passes-1;pass>=0;--pass){const float shift=shiftPixels*(pass+1)/passes*2.0f/std::max(1,width_);vertex(-1+shift,y0,0,v0);vertex(1+shift,y0,1,v0);vertex(1+shift,y1,1,v1);vertex(-1+shift,y0,0,v0);vertex(1+shift,y1,1,v1);vertex(-1+shift,y1,0,v1);}}
    glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glUseProgram(datamoshProgram_);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,datamoshTexture_);glUniform1i(datamoshSampler_,0);glUniform1f(datamoshAlpha_,clampf(0.34f+strength*0.56f,0.34f,0.90f)/passes);glBindBuffer(GL_ARRAY_BUFFER,datamoshVbo_);glBufferData(GL_ARRAY_BUFFER,out*sizeof(float),data.data(),GL_DYNAMIC_DRAW);glEnableVertexAttribArray(static_cast<GLuint>(datamoshPos_));glEnableVertexAttribArray(static_cast<GLuint>(datamoshUv_));glVertexAttribPointer(static_cast<GLuint>(datamoshPos_),2,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);glVertexAttribPointer(static_cast<GLuint>(datamoshUv_),2,GL_FLOAT,GL_FALSE,4*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));glDrawArrays(GL_TRIANGLES,0,out/4);glDisableVertexAttribArray(static_cast<GLuint>(datamoshUv_));glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glUseProgram(program_);
}

void Renderer::draw(const GameState& state) {
    ++mobileFpsFrames;const auto now=std::chrono::steady_clock::now();const float elapsed=std::chrono::duration<float>(now-mobileFpsWindowStart).count();if(elapsed>=0.5f){displayedMobileFps=mobileFpsFrames/elapsed;mobileFpsFrames=0;mobileFpsWindowStart=now;}
    glClearColor(0.557f,0.792f,0.902f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float proj[16];
    float view[16];
    float viewProj[16];
    // Match the authoritative THREE.PerspectiveCamera exactly. The previous
    // 62-degree mobile projection made the same chase-camera transform feel
    // zoomed, slow, and substantially less spatial than the browser's camera.
    perspective(proj, state.camera.verticalFovDegrees * DB_PI / 180.0f, static_cast<float>(width_) / static_cast<float>(height_), Pass7Visual::CameraNearPlane, Pass7Visual::CameraFarPlane);
    lookAt(view, state.camera.pos, state.camera.lookTarget, {0.0f, 1.0f, 0.0f});
    multiply(viewProj, proj, view);
    const bool cheapVisuals = state.localSettings.graphicsPreset <= 0;
    const auto actorVisible=[&](const Vec3& position){const Vec3 delta=position-state.camera.pos;const float maxDist=cheapVisuals?38.0f:55.0f;return lengthSq(delta)<maxDist*maxDist&&dot(delta,state.camera.forward)>-8.0f;};

    for(int tile=state.topology.currentTileIndex-ROOM_VISUAL_HORIZON;tile<=state.topology.currentTileIndex+ROOM_VISUAL_HORIZON;++tile)drawRoomTile(viewProj,state,tile);

    if(state.secretTv.available){
        const float knock=clampf(state.secretTv.knockPulse,0.0f,1.0f);
        const float membrane[4]={Pass7Visual::TvMembrane.r,Pass7Visual::TvMembrane.g,Pass7Visual::TvMembrane.b,(state.secretTv.broken?0.12f:0.25f)+knock*0.28f};
        const float breathe=0.04f+0.035f*std::sin(state.time*2.1f);
        const float push=knock*(0.10f+0.018f*std::sin(state.time*41.0f));
        const Vec3 wallCenter=state.secretTv.entrancePos+Vec3{0.0f,1.22f,0.0f};
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
        drawBox(viewProj,wallCenter+state.secretTv.entranceNormal*push,{0.08f+push*0.42f,2.30f+breathe+knock*0.10f,2.20f+breathe+knock*0.08f},0,membrane);
        glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    }
    if(state.player.inSecretRoom){
        const float floor[4]={Pass7Visual::SecretFloor.r,Pass7Visual::SecretFloor.g,Pass7Visual::SecretFloor.b,1},wall[4]={Pass7Visual::SecretWall.r,Pass7Visual::SecretWall.g,Pass7Visual::SecretWall.b,1},black[4]={Pass7Visual::SecretBlack.r,Pass7Visual::SecretBlack.g,Pass7Visual::SecretBlack.b,1},cable[4]={Pass7Visual::SecretCable.r,Pass7Visual::SecretCable.g,Pass7Visual::SecretCable.b,1};
        drawBox(viewProj,{40.4f,-0.04f,0},{7.4f,0.08f,6.4f},0,floor);
        drawBox(viewProj,{36.7f,2.4f,0},{0.12f,4.8f,6.4f},0,wall);
        drawBox(viewProj,{40.4f,2.4f,-3.2f},{7.4f,4.8f,0.12f},0,wall);drawBox(viewProj,{40.4f,2.4f,3.2f},{7.4f,4.8f,0.12f},0,wall);
        drawBox(viewProj,{42.25f,0.70f,0},{1.75f,1.35f,0.82f},-1.5708f,black);
        float proximity=0;const Vec3 tvPosition{41.82f,0.78f,0};const auto includePhone=[&](const PlayerState& player,bool active){if(active&&player.inSecretRoom)proximity=std::max(proximity,1.0f-clampf(length(player.pos-tvPosition)/6.0f,0,1));};includePhone(state.player,true);if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)includePhone(peer.player,peer.active);const float fullness=clampf(state.secretTv.signal/24.0f,0,1);
        if(state.secretTv.broken){const float v=0.18f+0.16f*std::abs(std::sin(state.time*47.0f+state.secretTv.signal*1.7f));const float staticColor[4]={v,v+0.035f,v+0.045f,1};drawBox(viewProj,{41.82f,0.78f,0},{0.035f,0.86f,1.22f},-1.5708f,staticColor);}
        else {const float cellY=0.86f/TvGifWall::Rows,cellZ=1.22f/TvGifWall::Columns;for(int row=0;row<TvGifWall::Rows;++row)for(int col=0;col<TvGifWall::Columns;++col){const auto rgb=tvGifWall_.sample(col,row,state.time,state.secretTv.signal);const float magnetic=proximity*(0.010f+0.018f*(1-fullness)),phase=state.time*5.1f+row*0.83f+col*0.29f,yWarp=std::sin(phase)*magnetic,zWarp=std::sin(phase*0.63f+row)*magnetic*1.6f,clarity=0.62f+0.38f*fullness,flicker=1-(1-fullness)*proximity*(0.05f+0.05f*std::sin(state.time*17+row));const float color[4]={rgb.r*clarity*flicker,rgb.g*clarity*flicker,rgb.b*clarity*flicker,1};drawBox(viewProj,{41.805f,0.78f-0.43f+cellY*(row+0.5f)+yWarp,-0.61f+cellZ*(col+0.5f)+zWarp},{0.038f,cellY*1.04f,cellZ*1.04f},-1.5708f,color);}}
        drawBox(viewProj,{41.35f,0.18f,-0.80f},{1.8f,0.055f,0.055f},0.18f,cable);drawBox(viewProj,{41.45f,0.16f,0.76f},{2.1f,0.045f,0.045f},-0.22f,cable);
    }

    // Project every caster's geometry along the browser sun vector (30,60,25)
    // onto the floor. This preserves physical direction, length and silhouette
    // while avoiding a shadow-map texture pass on mobile tile GPUs.
    if(state.localSettings.shadows){const float shadowMatrix[16]={1,0,0,0,-0.5f,0,-25.0f/60.0f,0,0,0,1,0,0.006f,0.012f,0.005f,1};
    float shadowViewProj[16];multiply(shadowViewProj,viewProj,shadowMatrix);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
    const float shadow[4]={0.012f,0.018f,0.022f,0.28f};
    if(state.phoneVisual.visible){if(phoneModel_.valid())drawStaticModel(shadowViewProj,phoneModel_,phoneVbo_,phoneNormalVbo_,state.phoneTransform.position,state.phoneVisual.bodyScale,state.phoneTransform.orientation,true);else drawBox(shadowViewProj,state.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},state.phoneTransform.orientation,shadow);}
    if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.playerId!=state.multiplayer.localPlayerId&&peer.player.alive){if(phoneModel_.valid())drawStaticModel(shadowViewProj,phoneModel_,phoneVbo_,phoneNormalVbo_,peer.phoneTransform.position,peer.phoneVisual.bodyScale,peer.phoneTransform.orientation,true);else drawBox(shadowViewProj,peer.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},peer.phoneTransform.orientation,shadow);}
    const float shadowTileOrigin=state.topology.currentTileIndex*ROOM_DEPTH;
    for(int offset=-1;offset<=1;++offset)for(auto target:state.targets)if(target.alive){target.pos.z=shadowTileOrigin+static_cast<float>(offset)*ROOM_DEPTH+(target.pos.z-std::floor((target.pos.z+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);if(!actorVisible(target.pos))continue;if(!target.slurpable){if(humanModel_.valid())drawHumanModel(shadowViewProj,target,state.time,true);else drawProceduralHuman(shadowViewProj,target,state.time,shadow);}if(target.slurpable&&target.soulVisual.visible&&target.soulCubeAmount>0.001f){const auto& sv=target.soulVisual;const float cube=0.72f*0.78f*target.scale*sv.morphScale;drawBox(shadowViewProj,target.pos+Vec3{0,0.57f+sv.verticalOffset,0},{cube*sv.scale.x,cube*sv.scale.y,cube*sv.scale.z},sv.rotationY,shadow);}}
    for(const auto& flower:state.flowers)if(flower.active){const Vec3 center{flower.pos.x,flower.pos.y,flower.pos.z+shadowTileOrigin};if(flowerModel_.valid())drawStaticModel(shadowViewProj,flowerModel_,flowerVbo_,flowerNormalVbo_,center,{1,1,1},quatAxisAngle({0,1,0},flower.rotationY),true);else drawBox(shadowViewProj,center,{0.54f,0.22f,0.54f},flower.rotationY,shadow);}
    for(const auto& bullet:state.bullets)if(bullet.alive){const float size=0.72f*1.12f*(bullet.brute?1.7f:1.0f);drawBox(shadowViewProj,bullet.pos,{size,size,size},bullet.spin*1.7f,shadow);}
    for(int i=0;i<state.debug.colliderCount;++i){const auto& c=state.roomColliders[i];drawBox(shadowViewProj,{c.center.x,c.center.y,shadowTileOrigin+c.center.z},{c.width,c.height,c.depth},0,shadow);}
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);}

    const float phoneBody[4] = {Pass7Visual::PhoneBody.r, Pass7Visual::PhoneBody.g, Pass7Visual::PhoneBody.b, 1.0f};
    const float screenBrightness = std::min(1.0f, 0.45f + state.phoneVisual.screenGlow * 0.36f);
    const float actionGlow=clampf(state.vacuum.power*0.45f+state.energy.dischargePositionAmount*0.85f,0.0f,1.0f);
    const float phoneScreen[4] = {Pass7Visual::PhoneEmission.r * screenBrightness+actionGlow*0.08f, Pass7Visual::PhoneEmission.g * screenBrightness+actionGlow*0.18f, Pass7Visual::PhoneEmission.b * screenBrightness+actionGlow*0.22f, 1.0f};
    if (state.phoneVisual.visible) {
        const Vec3 phonePos = state.phoneTransform.position;
        const Quat phoneOrientation = state.phoneTransform.orientation;
        if(!cheapVisuals&&phoneModel_.valid()) drawStaticModel(viewProj,phoneModel_,phoneVbo_,phoneNormalVbo_,phonePos,state.phoneVisual.bodyScale,phoneOrientation);
        else drawBox(viewProj, phonePos, {PHONE_BODY_WIDTH * state.phoneVisual.bodyScale.x, PHONE_BODY_HEIGHT * state.phoneVisual.bodyScale.y, PHONE_BODY_DEPTH}, phoneOrientation, phoneBody);
        drawBox(viewProj, state.phoneTransform.screenCenter, {PHONE_SCREEN_WIDTH * state.phoneVisual.screenScale.x, PHONE_SCREEN_HEIGHT * state.phoneVisual.screenScale.y, PHONE_SCREEN_DEPTH}, phoneOrientation, phoneScreen);
    }
    if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.playerId!=state.multiplayer.localPlayerId&&peer.player.alive){const float remoteBody[4]={0.32f,0.86f,1.0f,1.0f},remoteScreen[4]={0.05f,0.55f,0.78f,1.0f};if(!cheapVisuals&&phoneModel_.valid())drawStaticModel(viewProj,phoneModel_,phoneVbo_,phoneNormalVbo_,peer.phoneTransform.position,peer.phoneVisual.bodyScale,peer.phoneTransform.orientation);else drawBox(viewProj,peer.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},peer.phoneTransform.orientation,remoteBody);drawBox(viewProj,peer.phoneTransform.screenCenter,{PHONE_SCREEN_WIDTH,PHONE_SCREEN_HEIGHT,PHONE_SCREEN_DEPTH},peer.phoneTransform.orientation,remoteScreen);}

    const MeleeVisualState& melee=state.meleeVisual;
    if(melee.visualTimer>0.0f && !melee.locomotionLunge){
        const float t=1.0f-clampf(melee.visualTimer/std::max(0.001f,melee.visualDuration),0.0f,1.0f);
        const float hitBoost=melee.visualHit?1.25f:0.72f;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        const float cyan[4]={Pass7Visual::ElectricMagenta.r,Pass7Visual::ElectricMagenta.g,Pass7Visual::ElectricMagenta.b,(1.0f-t)*0.66f};
        const float slashScale=(0.75f+t*0.72f)*hitBoost;
        const Quat slashQ=quatAxisAngle({0,1,0},state.player.yaw)*quatAxisAngle({1,0,0},DB_PI*0.5f)*quatAxisAngle({0,0,1},-DB_PI*0.28f+t*DB_PI*1.15f);
        drawFxStrip(viewProj,melee.origin+melee.direction*(0.66f+0.22f*t),{slashScale,slashScale,1},slashQ,cyan,FX_ARC.data(),FX_STRIP_VERTICES);
        const Vec3 delta=melee.impact-melee.origin; const float len=std::max(0.3f,length(delta));
        const float yaw=std::atan2(delta.x,delta.z); const float pitch=-std::asin(clampf(delta.y/std::max(len,0.001f),-1.0f,1.0f));
        const Quat lineQ=quatAxisAngle({0,1,0},yaw)*quatAxisAngle({1,0,0},pitch); const float streak[4]={Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,(1.0f-t)*0.44f};
        const float strike=std::sin(t*DB_PI); drawFxStrip(viewProj,melee.origin+delta*0.46f,{1+strike*1.2f,1+strike*1.2f,len*(0.70f+strike*0.18f)},lineQ,streak,FX_STREAK.data(),FX_STRIP_VERTICES);
        const float white[4]={Pass7Visual::AcidChartreuse.r,Pass7Visual::AcidChartreuse.g,Pass7Visual::AcidChartreuse.b,melee.visualHit?(1.0f-t)*0.82f:(1.0f-t)*0.24f};
        const float ringScale=(0.45f+t*1.45f)*hitBoost; drawFxStrip(viewProj,melee.impact,{ringScale,ringScale,1},{},white,FX_RING.data(),FX_STRIP_VERTICES);
        glDisable(GL_BLEND);
    }

    const float targetColor[4] = {Pass7Visual::NormalEnemy.r, Pass7Visual::NormalEnemy.g, Pass7Visual::NormalEnemy.b, 1.0f};
    const float targetTileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(int offset=-1;offset<=1;++offset)for (auto target : state.targets) {
        if (!target.alive) continue;
        const float originalZ=target.pos.z;
        target.pos.z=targetTileOrigin+static_cast<float>(offset)*ROOM_DEPTH+(originalZ-std::floor((originalZ+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);
        if(!actorVisible(target.pos))continue;
        const float mirrorShift=target.pos.z-originalZ;
        target.tetherAnchor.z+=mirrorShift;target.tetherDestination.z+=mirrorShift;target.latchPoint.z+=mirrorShift;
        if (!target.slurpable) {
            if(cheapVisuals)drawCheapHuman(viewProj,target,targetColor);
            else if(humanModel_.valid())drawHumanModel(viewProj,target,state.time);
            else drawProceduralHuman(viewProj, target, state.time, targetColor);
        }
        if (target.slurpable && target.soulCubeAmount > 0.001f) {
            if (!target.soulVisual.visible) continue;
            const auto& sv=target.soulVisual; const Vec3 soulCenter=target.pos+Vec3{0.0f,0.57f+sv.verticalOffset,0.0f}; const float cube=0.72f*0.78f*target.scale*sv.morphScale;
            if(!cheapVisuals)drawSoulFlesh(viewProj,target,soulCenter);
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
        for(int offset=-ROOM_VISUAL_HORIZON;offset<=ROOM_VISUAL_HORIZON;++offset){const Vec3 center{flower.pos.x,flower.pos.y,flower.pos.z+flowerTileOrigin+static_cast<float>(offset)*ROOM_DEPTH};
        if(!cheapVisuals&&flowerModel_.valid()) drawStaticModel(viewProj,flowerModel_,flowerVbo_,flowerNormalVbo_,center,{1,1,1},quatAxisAngle({0,1,0},flower.rotationY));
        else if(cheapVisuals)drawBox(viewProj,center,{0.36f,0.18f,0.36f},flower.rotationY,flowerColor);
        else {drawBox(viewProj,center,{0.20f,0.20f,0.20f},flower.rotationY,flowerCore);for(int petal=0;petal<5;++petal){const float angle=flower.rotationY+static_cast<float>(petal)*DB_PI*2.0f/5.0f;const Vec3 p=center+Vec3{std::cos(angle)*0.23f,0,std::sin(angle)*0.23f};drawBox(viewProj,p,{0.30f,0.12f,0.16f},-angle,flowerColor);}}
        }
    }

    const float captureTileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(int offset=-ROOM_VISUAL_HORIZON;offset<=ROOM_VISUAL_HORIZON;++offset)for (int captureIndex=0;captureIndex<state.requiredSouls;++captureIndex) {
        const auto& capture=state.captures[captureIndex];
        const Vec3 capturePos=capture.pos+Vec3{0,0,captureTileOrigin+static_cast<float>(offset)*ROOM_DEPTH};
        const float frameColor[4]={Pass7Visual::MetallicTeal.r*0.74f,Pass7Visual::MetallicTeal.g*0.74f,Pass7Visual::MetallicTeal.b*0.74f,1.0f};
        const float holeColor[4]={0.02f,0.03f,0.04f,1.0f};
        drawBox(viewProj,capturePos+Vec3{0,0,-0.04f},{0.72f,0.72f,0.06f},0.0f,frameColor);
        drawBox(viewProj,capturePos,{0.52f,0.52f,0.08f},0.0f,holeColor);
        if(capture.filled){const float soul[4]={Pass7Visual::SoulBase.r,Pass7Visual::SoulBase.g,Pass7Visual::SoulBase.b,1.0f}; drawBox(viewProj,capturePos+Vec3{0,0,0.12f},{0.36f,0.36f,0.36f},state.time*2.0f,soul);}
    }

    const float bulletColor[4] = {Pass7Visual::SoulBase.r, Pass7Visual::SoulBase.g, Pass7Visual::SoulBase.b, 0.68f};
    for (const auto& bullet : state.bullets) {
        if (!bullet.alive) continue;
        const float size=0.72f*1.12f*(bullet.brute?1.7f:1.0f);
        drawBox(viewProj, bullet.pos, {size,size,size}, bullet.spin*1.7f, bulletColor);
    }
    if(state.localSettings.particles)for(const auto& particle:state.particles) if(particle.life>0.0f) {
        const float t=particle.maxLife>0.0f?clampf(particle.life/particle.maxLife,0.0f,1.0f):0.0f;
        const float size=particle.size*t;
        const float flameColor[4]={1.0f,0.267f,0.267f,0.9f};
        const float shellColor[4]={0.16f,0.39f,0.42f,0.82f*t};
        const float* particleColor=particle.kind==1?shellColor:flameColor;
        drawBox(viewProj,particle.pos,{size,size,size},particle.life*4.0f,particleColor);
    }
    if(state.localSettings.portalWindow)drawDoorDataMosh(state);
    drawHud(state);
}
