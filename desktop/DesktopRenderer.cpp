#include "DesktopRenderer.hpp"
#include "HumanVisual.hpp"
#include "BitmapFont.hpp"
#include "RoomEnvironment.hpp"
#include "RoomLighting.hpp"
#include "PhoneDisplayLayout.hpp"
#include "PhoneStencilReveal.hpp"
#include "GameplayPhoneModel.hpp"
#include "RenderContracts.hpp"
#include "FieldGrassTexture.hpp"
#include "CitySurfaceTexture.hpp"
#include "FacetedRock.hpp"
#include "MarkerPillarGeometry.hpp"
#include "world/RoomGeometry.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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
#include <array>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
float displayedFps=60.0f;
int fpsFrames=0;
auto fpsWindowStart=std::chrono::steady_clock::now();
constexpr float ROOM_WIDTH = world::RoomWidth;
constexpr float ROOM_DEPTH = world::RoomDepth;
constexpr int ROOM_VISUAL_HORIZON = 2;
constexpr float ROOM_WALL_HEIGHT = world::RoomWallHeight;
constexpr float PI = 3.14159265358979323846f;
constexpr float SHADOW_PROJECTION_SCALE = 0.5f;

struct MenuFontAtlas {
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info{};
    bool cpuReady = false;

    bool load(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        if (bytes.empty()) return false;
        cpuReady = stbtt_InitFont(&info, bytes.data(), stbtt_GetFontOffsetForIndex(bytes.data(), 0)) != 0;
        return cpuReady;
    }
};

MenuFontAtlas menuRegularFont;
MenuFontAtlas menuSemiboldFont;

void hashPhoneDisplayValue(std::uint64_t& hash, std::uint64_t value) {
    hash ^= value;
    hash *= 1099511628211ull;
}

void hashPhoneDisplayString(std::uint64_t& hash, const std::string& value) {
    for (unsigned char c : value) hashPhoneDisplayValue(hash, c);
    hashPhoneDisplayValue(hash, 0xffu);
}

void hashPhoneDisplayFloat(std::uint64_t& hash, float value, float scale) {
    hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(std::lround(value * scale)));
}

Vec3 gradedSceneColor(float r,float g,float b) {
    const float luma=r*0.2126f+g*0.7152f+b*0.0722f;
    r=luma+(r-luma)*1.10f;g=luma+(g-luma)*1.10f;b=luma+(b-luma)*1.10f;
    return {clampf((r-0.5f)*1.06f+0.5f,0.0f,1.0f),clampf((g-0.5f)*1.06f+0.5f,0.0f,1.0f),clampf((b-0.5f)*1.06f+0.5f,0.0f,1.0f)};
}
Vec3 mix3(const Vec3& a,const Vec3& b,float t){const float u=clampf(t,0.0f,1.0f);return {a.x+(b.x-a.x)*u,a.y+(b.y-a.y)*u,a.z+(b.z-a.z)*u};}
void gradedColor(float r,float g,float b,float a=1.0f){const Vec3 color=gradedSceneColor(r,g,b);glColor4f(color.x,color.y,color.z,a);}

Vec3 cross3(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
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

void drawGroundShadow(const Vec3& caster, float halfWidth, float halfDepth, float height, float alpha) {
    constexpr int segments = 8;
    const Vec3& sunDirection=render_contract::DesktopSceneLighting.sun.direction;
    const float sunXOverY=sunDirection.x/sunDirection.y;
    const float sunZOverY=sunDirection.z/sunDirection.y;
    const Vec3 center{
        caster.x-height*SHADOW_PROJECTION_SCALE*sunXOverY,
        0.012f,
        caster.z-height*SHADOW_PROJECTION_SCALE*sunZOverY
    };
    const float stretch=height*0.16f;
    const float angle=std::atan2(-sunXOverY,-sunZOverY);
    const float c=std::cos(angle),s=std::sin(angle);
    glColor4f(0.012f,0.018f,0.022f,alpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(center.x,center.y,center.z);
    for(int i=0;i<=segments;++i){
        const float a=2.0f*PI*static_cast<float>(i)/static_cast<float>(segments);
        const float localX=std::cos(a)*(halfWidth+stretch*0.35f);
        const float localZ=std::sin(a)*(halfDepth+stretch);
        glVertex3f(center.x+localX*c-localZ*s,center.y,center.z+localX*s+localZ*c);
    }
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
    gradedColor(r,g,b);
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
    const HumanVisualSpec& spec = HUMAN_VISUAL_SPEC;
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
Quat quaternionFromEulerXYZ(float x,float y,float z) {
    const float c1=std::cos(x*0.5f),c2=std::cos(y*0.5f),c3=std::cos(z*0.5f),s1=std::sin(x*0.5f),s2=std::sin(y*0.5f),s3=std::sin(z*0.5f);
    return {s1*c2*c3+c1*s2*s3,c1*s2*c3-s1*c2*s3,c1*c2*s3+s1*s2*c3,c1*c2*c3-s1*s2*s3};
}

unsigned int compileStaticModel(const StaticModelData& model, bool shadow = false) {
    if (!model.valid()) return 0;
    const GLuint list=glGenLists(1);
    if (!list) return 0;
    glNewList(list,GL_COMPILE);
    for(const StaticModelBatch& batch:model.batches) {
        // Shadow display lists are drawn inside one renderer-owned blended pass.
        // Do not let source-material alpha toggle that pass off partway through
        // a multi-batch model, which would make subsequent shadow batches opaque.
        const bool translucent=!shadow&&batch.color[3]<0.995f;if(translucent){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);}
        if(shadow) glColor4f(0.012f,0.018f,0.022f,0.28f); else gradedColor(batch.color[0],batch.color[1],batch.color[2],batch.color[3]);
        glBegin(GL_TRIANGLES);
        for(std::uint32_t i=batch.start;i+2<batch.start+batch.count;i+=3) {
            const std::size_t a=static_cast<std::size_t>(i)*3u;
            const std::size_t b=a+3u;
            const std::size_t c=a+6u;
            const Vec3 va{model.vertices[a],model.vertices[a+1],model.vertices[a+2]};
            const Vec3 vb{model.vertices[b],model.vertices[b+1],model.vertices[b+2]};
            const Vec3 vc{model.vertices[c],model.vertices[c+1],model.vertices[c+2]};
            const Vec3 normal=normalized(cross3(vb-va,vc-va));
            glNormal3f(normal.x,normal.y,normal.z);
            glVertex3f(va.x,va.y,va.z); glVertex3f(vb.x,vb.y,vb.z); glVertex3f(vc.x,vc.y,vc.z);
        }
        glEnd();
        if(translucent){glDepthMask(GL_TRUE);glDisable(GL_BLEND);}
    }
    glEndList();
    return list;
}

}

void DesktopRenderer::setAssetRoot(const std::filesystem::path& root) {
    tvGifWall_.load(root.parent_path()/"tv-gifs");
    const std::filesystem::path fontRoot = root.parent_path() / "fonts";
    const bool menuRegularLoaded = menuRegularFont.load(fontRoot / "SourceSans3-Regular.ttf");
    const bool menuSemiboldLoaded = menuSemiboldFont.load(fontRoot / "SourceSans3-Semibold.ttf");
    StaticModelData phone;
    const bool phoneLoaded=phone.load((root/"phone.dbmesh").string());
    const bool humanLoaded=humanModel_.load((root/"human.dbhuman").string());
    phoneModelList_=phoneLoaded?compileStaticModel(phone):0; phoneShadowList_=phoneLoaded?compileStaticModel(phone,true):0;
    std::printf("Models: phone=%s flower=prismatic human=%s menuFont=%s/%s\n",phoneModelList_?"loaded":"fallback",humanLoaded?"loaded":"fallback",menuRegularLoaded?"regular":"bitmap",menuSemiboldLoaded?"semibold":"bitmap");
}

void DesktopRenderer::resize(int width, int height) {
    const int nextWidth=std::max(1,width),nextHeight=std::max(1,height);
    if(nextWidth!=width_||nextHeight!=height_)datamoshFrameReady_=false;
    width_=nextWidth;height_=nextHeight;glViewport(0,0,width_,height_);
}

void DesktopRenderer::setHudVisible(bool visible) {
    hudVisible_ = visible;
}

void DesktopRenderer::setAtmosphereProfile(render_contract::AtmosphereProfile profile){lightingControl_.reference=profile;}
void DesktopRenderer::setLightingControl(const render_contract::RuntimeLightingControl& control){lightingControl_=control;}
render_contract::RuntimeLightingControl& DesktopRenderer::lightingControl(){return lightingControl_;}
const render_contract::RuntimeLightingControl& DesktopRenderer::lightingControl() const{return lightingControl_;}
render_contract::SceneAtmosphere DesktopRenderer::resolvedAtmosphere(const GameState& state) const{return render_contract::resolveSceneAtmosphere(lightingControl_,{state.time,state.roomIndex,state.vacuum.power*0.62f+state.energy.dischargePositionAmount});}

void DesktopRenderer::drawBox(const Vec3& p, const Vec3& s, float pitch, float yaw, float roll, float r, float g, float b, float a) {
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(yaw * 180.0f / PI, 0, 1, 0);
    glRotatef(pitch * 180.0f / PI, 1, 0, 0);
    glRotatef(roll * 180.0f / PI, 0, 0, 1);
    glScalef(s.x, s.y, s.z); gradedColor(r,g,b,a); cube(); glPopMatrix();
}

void fxRibbon(const Vec3& p,const Quat& q,const Vec3& scale,float start,float sweep,int segments,float inner,float outer,float r,float g,float b,float a){
    const float matrix[16]={1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y+q.z*q.w),2*(q.x*q.z-q.y*q.w),0,2*(q.x*q.y-q.z*q.w),1-2*(q.x*q.x+q.z*q.z),2*(q.y*q.z+q.x*q.w),0,2*(q.x*q.z+q.y*q.w),2*(q.y*q.z-q.x*q.w),1-2*(q.x*q.x+q.y*q.y),0,0,0,0,1};
    glPushMatrix(); glTranslatef(p.x,p.y,p.z); glMultMatrixf(matrix); glScalef(scale.x,scale.y,scale.z); gradedColor(r,g,b,a);
    glBegin(GL_TRIANGLE_STRIP); for(int i=0;i<=segments;++i){const float angle=start+sweep*static_cast<float>(i)/segments; const float c=std::cos(angle),s=std::sin(angle); glVertex3f(c*outer,s*outer,0); glVertex3f(c*inner,s*inner,0);} glEnd(); glPopMatrix();
}

void fxStreak(const Vec3& p,const Quat& q,float length,float width,float r,float g,float b,float a){
    constexpr int segments=12; const float matrix[16]={1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y+q.z*q.w),2*(q.x*q.z-q.y*q.w),0,2*(q.x*q.y-q.z*q.w),1-2*(q.x*q.x+q.z*q.z),2*(q.y*q.z+q.x*q.w),0,2*(q.x*q.z+q.y*q.w),2*(q.y*q.z-q.x*q.w),1-2*(q.x*q.x+q.y*q.y),0,0,0,0,1};
    glPushMatrix(); glTranslatef(p.x,p.y,p.z); glMultMatrixf(matrix); gradedColor(r,g,b,a); glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=segments;++i){const float angle=i*PI*2.0f/segments,c=std::cos(angle),s=std::sin(angle); glVertex3f(c*width,s*width,-length*0.5f); glVertex3f(c*width*0.32f,s*width*0.32f,length*0.5f);} glEnd(); glPopMatrix();
}

struct CpuCanvas {
    int w = PhoneDisplayState::LogicalWidth;
    int h = PhoneDisplayState::LogicalHeight;
    std::vector<unsigned char>& pixels;
};

void cpuClear(CpuCanvas& canvas, float r, float g, float b, float a = 1.0f) {
    const unsigned char rr = static_cast<unsigned char>(clampf(r, 0.0f, 1.0f) * 255.0f);
    const unsigned char gg = static_cast<unsigned char>(clampf(g, 0.0f, 1.0f) * 255.0f);
    const unsigned char bb = static_cast<unsigned char>(clampf(b, 0.0f, 1.0f) * 255.0f);
    const unsigned char aa = static_cast<unsigned char>(clampf(a, 0.0f, 1.0f) * 255.0f);
    for (int i = 0; i < canvas.w * canvas.h; ++i) {
        const int at = i * 4;
        canvas.pixels[at + 0] = rr;
        canvas.pixels[at + 1] = gg;
        canvas.pixels[at + 2] = bb;
        canvas.pixels[at + 3] = aa;
    }
}

void cpuRect(CpuCanvas& canvas, float x, float y, float w, float h, float r, float g, float b, float a) {
    const int x0 = std::max(0, static_cast<int>(std::floor(x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(y)));
    const int x1 = std::min(canvas.w, static_cast<int>(std::ceil(x + w)));
    const int y1 = std::min(canvas.h, static_cast<int>(std::ceil(y + h)));
    const float alpha = clampf(a, 0.0f, 1.0f);
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            const int at = (py * canvas.w + px) * 4;
            canvas.pixels[at + 0] = static_cast<unsigned char>((static_cast<float>(canvas.pixels[at + 0]) * (1.0f - alpha) + r * 255.0f * alpha));
            canvas.pixels[at + 1] = static_cast<unsigned char>((static_cast<float>(canvas.pixels[at + 1]) * (1.0f - alpha) + g * 255.0f * alpha));
            canvas.pixels[at + 2] = static_cast<unsigned char>((static_cast<float>(canvas.pixels[at + 2]) * (1.0f - alpha) + b * 255.0f * alpha));
            canvas.pixels[at + 3] = 255;
        }
    }
}

void cpuMeter(CpuCanvas& canvas, float x, float y, float w, float h, float fill, VisualColor color, float alpha = 0.82f) {
    cpuRect(canvas, x, y, w, h, 0.005f, 0.020f, 0.025f, 0.68f);
    cpuRect(canvas, x, y, w, 2.0f, VisualIdentity::ElectricCyan.r, VisualIdentity::ElectricCyan.g, VisualIdentity::ElectricCyan.b, 0.30f);
    cpuRect(canvas, x, y + h - 2.0f, w, 2.0f, VisualIdentity::ElectricCyan.r, VisualIdentity::ElectricCyan.g, VisualIdentity::ElectricCyan.b, 0.18f);
    cpuRect(canvas, x, y, w * clampf(fill, 0.0f, 1.0f), h, color.r, color.g, color.b, alpha);
}

const MenuFontAtlas& cpuMenuFont(bool semibold) {
    return semibold && menuSemiboldFont.cpuReady ? menuSemiboldFont : menuRegularFont;
}

float cpuTextWidth(const std::string& text, float px, bool semibold = false) {
    const MenuFontAtlas& font = cpuMenuFont(semibold);
    if (!font.cpuReady) return static_cast<float>(text.size()) * px * 0.55f;
    const float scale = stbtt_ScaleForPixelHeight(&font.info, px);
    float width = 0.0f;
    int previous = 0;
    for (unsigned char c : text) {
        int advance = 0, bearing = 0;
        stbtt_GetCodepointHMetrics(&font.info, c, &advance, &bearing);
        if (previous) width += static_cast<float>(stbtt_GetCodepointKernAdvance(&font.info, previous, c)) * scale;
        width += static_cast<float>(advance) * scale;
        previous = c;
    }
    return width;
}

void cpuText(CpuCanvas& canvas, const std::string& text, float x, float baseline, float px, float r, float g, float b, float a, bool semibold = false, bool centered = false) {
    const MenuFontAtlas& font = cpuMenuFont(semibold);
    if (!font.cpuReady) return;
    float pen = centered ? x - cpuTextWidth(text, px, semibold) * 0.5f : x;
    const float scale = stbtt_ScaleForPixelHeight(&font.info, px);
    int previous = 0;
    for (unsigned char c : text) {
        int advance = 0, bearing = 0;
        stbtt_GetCodepointHMetrics(&font.info, c, &advance, &bearing);
        if (previous) pen += static_cast<float>(stbtt_GetCodepointKernAdvance(&font.info, previous, c)) * scale;
        int bw = 0, bh = 0, xoff = 0, yoff = 0;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font.info, scale, scale, c, &bw, &bh, &xoff, &yoff);
        if (bw > 0 && bh > 0) {
            const int dstX = static_cast<int>(std::floor(pen)) + xoff;
            const int dstY = static_cast<int>(std::floor(baseline)) + yoff;
            for (int yy = 0; yy < bh; ++yy) {
                const int py = dstY + yy;
                if (py < 0 || py >= canvas.h) continue;
                for (int xx = 0; xx < bw; ++xx) {
                    const int pxOut = dstX + xx;
                    if (pxOut < 0 || pxOut >= canvas.w) continue;
                    const float alpha = (static_cast<float>(bitmap[yy * bw + xx]) / 255.0f) * clampf(a, 0.0f, 1.0f);
                    const int at = (py * canvas.w + pxOut) * 4;
                    canvas.pixels[at + 0] = static_cast<unsigned char>(static_cast<float>(canvas.pixels[at + 0]) * (1.0f - alpha) + r * 255.0f * alpha);
                    canvas.pixels[at + 1] = static_cast<unsigned char>(static_cast<float>(canvas.pixels[at + 1]) * (1.0f - alpha) + g * 255.0f * alpha);
                    canvas.pixels[at + 2] = static_cast<unsigned char>(static_cast<float>(canvas.pixels[at + 2]) * (1.0f - alpha) + b * 255.0f * alpha);
                    canvas.pixels[at + 3] = 255;
                }
            }
        }
        stbtt_FreeBitmap(bitmap, nullptr);
        pen += static_cast<float>(advance) * scale;
        previous = c;
    }
}

float cpuStencilTextWidth(const std::string& text, float px) {
    if (text.empty()) return 0.0f;
    return cpuTextWidth(text, px, true) * phone_stencil::HorizontalScale +
        static_cast<float>(text.size() - 1) * px * phone_stencil::TrackingEm;
}

void cpuStencilText(CpuCanvas& canvas,const std::string& text,float x,float baseline,float px,
                    float r,float g,float b,float a,float age,bool centered=false) {
    const MenuFontAtlas& font=cpuMenuFont(true);if(!font.cpuReady||text.empty())return;
    float pen=centered?x-cpuStencilTextWidth(text,px)*0.5f:x;
    const float scale=stbtt_ScaleForPixelHeight(&font.info,px);
    std::uint32_t seed=phone_stencil::textSeed(text);const bool leftToRight=(seed&1u)==0u;int previous=0;
    for(std::size_t i=0;i<text.size();++i){
        const unsigned char c=static_cast<unsigned char>(text[i]);seed=phone_stencil::glyphSeed(seed,c,i);
        if(previous)pen+=static_cast<float>(stbtt_GetCodepointKernAdvance(&font.info,previous,c))*scale*phone_stencil::HorizontalScale;
        int advance=0,bearing=0;stbtt_GetCodepointHMetrics(&font.info,c,&advance,&bearing);
        const float reveal=phone_stencil::glyphReveal(age,i,text.size(),leftToRight);
        const float travel=(leftToRight?-1.0f:1.0f)*(1.0f-reveal)*px*0.055f;
        const float jitterX=(static_cast<float>((seed>>4)&7u)-3.5f)*0.12f,jitterY=(static_cast<float>((seed>>9)&7u)-3.5f)*0.11f;
        if(c!=' '){
            int bw=0,bh=0,xoff=0,yoff=0;unsigned char* bitmap=stbtt_GetCodepointBitmap(&font.info,scale,scale,c,&bw,&bh,&xoff,&yoff);
            const int compressedW=std::max(1,static_cast<int>(std::ceil(static_cast<float>(bw)*phone_stencil::HorizontalScale)));
            const bool bridge=compressedW>=8&&c!='I'&&c!='i'&&c!='l'&&c!='1';const int bridgeX=static_cast<int>(std::round((0.30f+static_cast<float>((seed>>13)&3u)*0.13f)*static_cast<float>(compressedW-1)));
            if(bitmap&&bw>0&&bh>0)for(int yy=0;yy<bh;++yy)for(int dx=0;dx<compressedW;++dx){
                const float yn=bh>1?static_cast<float>(yy)/static_cast<float>(bh-1):0.0f;if(bridge&&reveal<0.96f&&std::abs(dx-bridgeX)<=(px>=42.0f?1:0)&&yn>0.20f&&yn<0.80f)continue;
                const int sx=std::min(bw-1,static_cast<int>(static_cast<float>(dx)/phone_stencil::HorizontalScale));unsigned char ink=0;
                for(int sy=std::max(0,yy-1);sy<=std::min(bh-1,yy+1);++sy)for(int nx=std::max(0,sx-1);nx<=std::min(bw-1,sx+1);++nx)ink=std::max(ink,bitmap[sy*bw+nx]);
                const float alpha=static_cast<float>(ink)/255.0f*a*reveal;if(alpha<=0.01f)continue;
                const int ox=static_cast<int>(std::floor(pen+static_cast<float>(xoff)*phone_stencil::HorizontalScale+static_cast<float>(dx)+jitterX+travel)),oy=static_cast<int>(std::floor(baseline+static_cast<float>(yoff+yy)+jitterY));if(ox<0||ox>=canvas.w||oy<0||oy>=canvas.h)continue;
                const int at=(oy*canvas.w+ox)*4;canvas.pixels[at]=static_cast<unsigned char>(canvas.pixels[at]*(1-alpha)+r*255*alpha);canvas.pixels[at+1]=static_cast<unsigned char>(canvas.pixels[at+1]*(1-alpha)+g*255*alpha);canvas.pixels[at+2]=static_cast<unsigned char>(canvas.pixels[at+2]*(1-alpha)+b*255*alpha);canvas.pixels[at+3]=255;
            }
            stbtt_FreeBitmap(bitmap,nullptr);
            if((seed&3u)!=0u&&reveal>0.0f&&reveal<1.0f){const float fleck=std::max(1.0f,px*0.025f);cpuRect(canvas,pen+jitterX+travel*1.7f,baseline-px*0.45f,fleck,fleck,r,g,b,a*0.28f);}
        }
        pen+=static_cast<float>(advance)*scale*phone_stencil::HorizontalScale+px*phone_stencil::TrackingEm;previous=c;
    }
}

void drawPaletteMenuTitle(const std::string& text, float centerX, float centerY, float px, float time, float opacity = 1.0f) {
    const MenuFontAtlas& font = cpuMenuFont(true);
    if (!font.cpuReady || text.empty()) return;
    constexpr float rasterSupersample = 2.0f;
    constexpr float rasterToScreen = 1.0f / rasterSupersample;
    struct CachedTitleGlyph {
        unsigned char code = 0;
        int bw = 0, bh = 0, xoff = 0, yoff = 0, advance = 0;
        std::vector<unsigned char> bitmap;
    };
    static float cachedPx = -1.0f;
    static std::vector<CachedTitleGlyph> cachedGlyphs;
    const float baseline = centerY + px * 0.34f;
    const float scale = stbtt_ScaleForPixelHeight(&font.info, px);
    const float rasterScale = stbtt_ScaleForPixelHeight(&font.info, px * rasterSupersample);
    if (std::abs(cachedPx - px) > 0.01f || cachedGlyphs.size() != text.size()) {
        cachedPx = px;
        cachedGlyphs.clear();
        cachedGlyphs.reserve(text.size());
        for (unsigned char c : text) {
            CachedTitleGlyph glyph;
            glyph.code = c;
            int bearing = 0;
            stbtt_GetCodepointHMetrics(&font.info, c, &glyph.advance, &bearing);
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&font.info, rasterScale, rasterScale, c, &glyph.bw, &glyph.bh, &glyph.xoff, &glyph.yoff);
            if (bitmap && glyph.bw > 0 && glyph.bh > 0)
                glyph.bitmap.assign(bitmap, bitmap + glyph.bw * glyph.bh);
            stbtt_FreeBitmap(bitmap, nullptr);
            cachedGlyphs.push_back(std::move(glyph));
        }
    }
    float pen = centerX - cpuTextWidth(text, px, true) * 0.5f;
    int previous = 0;
    glBegin(GL_QUADS);
    for (std::size_t i = 0; i < text.size(); ++i) {
        const CachedTitleGlyph& glyph = cachedGlyphs[i];
        const unsigned char c = glyph.code;
        if (previous) pen += static_cast<float>(stbtt_GetCodepointKernAdvance(&font.info, previous, c)) * scale;
        const float hue = std::fmod(time * 0.026f + static_cast<float>(i) * 0.115f, 1.0f);
        const float k = hue * 6.0f, f = k - std::floor(k), q = 1.0f - f;
        Vec3 color{1.0f, f, 0.0f};
        switch (static_cast<int>(k) % 6) {
            case 1: color = {q, 1.0f, 0.0f}; break;
            case 2: color = {0.0f, 1.0f, f}; break;
            case 3: color = {0.0f, q, 1.0f}; break;
            case 4: color = {f, 0.0f, 1.0f}; break;
            case 5: color = {1.0f, 0.0f, q}; break;
        }
        const float r = 0.55f + color.x * 0.42f;
        const float g = 0.65f + color.y * 0.34f;
        const float b = 0.72f + color.z * 0.28f;
        for (int yy = 0; yy < glyph.bh; ++yy) for (int xx = 0; xx < glyph.bw; ++xx) {
            const float alpha = static_cast<float>(glyph.bitmap[yy * glyph.bw + xx]) / 255.0f * 0.96f * opacity;
            if (alpha <= 0.01f) continue;
            const float x = pen + static_cast<float>(glyph.xoff + xx) * rasterToScreen;
            const float y = baseline + static_cast<float>(glyph.yoff + yy) * rasterToScreen;
            glColor4f(r, g, b, alpha);
            glVertex2f(x, y); glVertex2f(x + rasterToScreen, y); glVertex2f(x + rasterToScreen, y + rasterToScreen); glVertex2f(x, y + rasterToScreen);
        }
        pen += static_cast<float>(glyph.advance) * scale;
        previous = c;
    }
    glEnd();
}

void renderPhoneDisplayPixels(const GameState& state, std::vector<unsigned char>& pixels) {
    pixels.resize(PhoneDisplayState::LogicalWidth * PhoneDisplayState::LogicalHeight * 4);
    CpuCanvas canvas{PhoneDisplayState::LogicalWidth, PhoneDisplayState::LogicalHeight, pixels};
    const PhoneDisplayState& display = state.phoneDisplay;
    if (state.dead || display.mode == PhoneDisplayMode::Off || display.mode == PhoneDisplayMode::Death) {
        cpuClear(canvas, 0.006f, 0.010f, 0.013f, 1.0f);
        return;
    }
    const Vec3 tint = display.screenTint;
    cpuClear(canvas, 0.025f + tint.x * 0.18f, 0.045f + tint.y * 0.16f, 0.060f + tint.z * 0.15f, 1.0f);
    cpuRect(canvas, 0, 0, static_cast<float>(canvas.w), static_cast<float>(canvas.h), 0.03f, 0.55f, 0.62f, 0.10f + display.brightness * 0.08f);

    const bool menuVisible = state.cinematic.introActive || !state.started ||
        (state.started && state.uiPaused && !state.multiplayer.enabled && !state.upgradeMenu.active);
    if (!menuVisible) {
        const GameplayPhoneModel phone = makeGameplayPhoneModel(state);
        const float vacuum = clampf(state.vacuum.power, 0.0f, 1.0f);
        const float activity = clampf(vacuum * 0.70f + display.powerPulse * 0.20f + display.capturePulse * 0.20f, 0.0f, 1.0f);
        const VisualColor stable = VisualIdentity::ElectricCyan;
        const VisualColor stored = VisualIdentity::AcidChartreuse;
        const VisualColor warning = VisualIdentity::Copper;
        const VisualColor batteryColor = phone.lowBattery ? warning : stable;
        const float reveal = phone_stencil::appearanceAge(display.transitionProgress);

        cpuStencilText(canvas,"ROOM "+std::to_string(phone.roomIndex),84.0f,112.0f,30.0f,
            stable.r,stable.g,stable.b,0.62f,reveal);
        const std::string tokenText="TOKEN "+std::to_string(phone.tokens);
        const float tokenPx=30.0f;
        cpuStencilText(canvas,tokenText,636.0f-cpuTextWidth(tokenText,tokenPx,false),112.0f,tokenPx,
            stored.r,stored.g,stored.b,0.62f,reveal);

        const int objectiveCount=std::max(1,phone.requiredGoals);
        const float objectiveGap=12.0f;
        const float objectiveCell=std::min(46.0f,(552.0f-objectiveGap*static_cast<float>(objectiveCount-1))/static_cast<float>(objectiveCount));
        const float objectiveW=objectiveCell*objectiveCount+objectiveGap*(objectiveCount-1);
        const float objectiveX=(720.0f-objectiveW)*0.5f;
        for(int i=0;i<objectiveCount;++i){
            const bool filled=i<phone.filledGoals;
            const VisualColor color=filled?stored:stable;
            const float x=objectiveX+i*(objectiveCell+objectiveGap);
            cpuRect(canvas,x,164.0f,objectiveCell,objectiveCell,color.r,color.g,color.b,filled?0.92f:0.38f);
            cpuRect(canvas,x+4.0f,168.0f,objectiveCell-8.0f,objectiveCell-8.0f,0.008f,0.020f,0.026f,filled?0.18f:0.86f);
        }

        cpuStencilText(canvas,"CHARGE",84.0f,300.0f,34.0f,
            batteryColor.r,batteryColor.g,batteryColor.b,0.78f,reveal);
        const std::string batteryValue=std::to_string(phone.batteryPercent);
        cpuStencilText(canvas,batteryValue,636.0f-cpuTextWidth(batteryValue,54.0f,true),310.0f,54.0f,
            batteryColor.r,batteryColor.g,batteryColor.b,0.94f,reveal);
        cpuRect(canvas,84.0f,338.0f,552.0f,18.0f,0.008f,0.020f,0.026f,0.92f);
        cpuRect(canvas,84.0f,338.0f,552.0f*phone.batteryFill,18.0f,
            batteryColor.r,batteryColor.g,batteryColor.b,0.90f);

        constexpr int columns=5,rows=6,cells=columns*rows;
        const float cell=54.0f,gap=12.0f,gridW=columns*cell+(columns-1)*gap;
        const float gridX=(720.0f-gridW)*0.5f,gridY=470.0f;
        for(int i=0;i<cells;++i){
            const int col=i%columns,row=i/columns;
            const VisualColor c=VisualIdentity::DataMosaicPalette[i%25];
            const bool filled=i<phone.storedSouls;
            const float wave=0.5f+0.5f*std::sin(state.time*0.9f+i*0.37f+display.screenNoisePhase*8.0f);
            const float alpha=filled?(0.76f+activity*0.16f+wave*0.08f):0.10f;
            const float shade=filled?1.0f:0.24f;
            cpuRect(canvas,gridX+col*(cell+gap),gridY+row*(cell+gap),cell,cell,
                c.r*shade,c.g*shade,c.b*shade,alpha);
        }
        const std::string soulValue=std::to_string(phone.storedSouls)+" / "+std::to_string(phone.soulCapacity);
        cpuStencilText(canvas,soulValue,360.0f-cpuTextWidth(soulValue,42.0f,true)*0.5f,930.0f,42.0f,
            stored.r,stored.g,stored.b,0.88f,reveal);

        if(phone.supplementalActive){
            const std::string power="POWER "+std::to_string(phone.flowerStacks);
            cpuStencilText(canvas,power,84.0f,1030.0f,30.0f,stored.r,stored.g,stored.b,0.74f,reveal);
            cpuRect(canvas,84.0f,1052.0f,552.0f,10.0f,0.008f,0.020f,0.026f,0.90f);
            cpuRect(canvas,84.0f,1052.0f,552.0f*phone.supplementalFill,10.0f,stored.r,stored.g,stored.b,0.82f);
        }
        if (phone.lowBattery) {
            const float pulse = 0.35f + 0.25f * std::sin(state.time * 4.1f);
            cpuRect(canvas,0,0,static_cast<float>(canvas.w),18.0f,warning.r,warning.g,warning.b,pulse);
            cpuRect(canvas,0,static_cast<float>(canvas.h)-18.0f,static_cast<float>(canvas.w),18.0f,warning.r,warning.g,warning.b,pulse);
        }
        return;
    }

    const PhoneDisplayMenuLayout layout = makePhoneDisplayMenuLayout(state, cpuTextWidth);
    const float stencilAge=phone_stencil::appearanceAge(display.transitionProgress);
    const Vec3 resolvedAccent=phoneDisplayResolvedAccent(display);
    const VisualColor channelAccent{resolvedAccent.x,resolvedAccent.y,resolvedAccent.z};
    if (!layout.title.empty()) {
        if (layout.paletteTitle) {
            float pen = layout.logicalW * 0.5f - cpuTextWidth(layout.title, layout.titlePx, true) * 0.5f;
            for (std::size_t i = 0; i < layout.title.size(); ++i) {
                const std::string letter(1, layout.title[i]);
                const float hue = std::fmod(state.time * 0.026f + static_cast<float>(i) * 0.115f, 1.0f);
                const float k = hue * 6.0f, f = k - std::floor(k), q = 1.0f - f;
                Vec3 color{1.0f, f, 0.0f};
                switch (static_cast<int>(k) % 6) {
                    case 1: color = {q, 1.0f, 0.0f}; break;
                    case 2: color = {0.0f, 1.0f, f}; break;
                    case 3: color = {0.0f, q, 1.0f}; break;
                    case 4: color = {f, 0.0f, 1.0f}; break;
                    case 5: color = {1.0f, 0.0f, q}; break;
                }
                cpuText(canvas, letter, pen, layout.titleCenterY + layout.titlePx * 0.34f, layout.titlePx, 0.55f + color.x * 0.42f, 0.65f + color.y * 0.34f, 0.72f + color.z * 0.28f, 0.96f, true);
                pen += cpuTextWidth(letter, layout.titlePx, true) + 2.0f;
            }
        } else {
            cpuStencilText(canvas,layout.title,layout.logicalW*0.5f,layout.titleCenterY+layout.titlePx*0.34f,layout.titlePx,0.90f,0.97f,1.0f,0.96f,stencilAge,true);
        }
    }
    if (layout.joinCode) {
        const std::string room = state.multiplayer.roomCode.data();
        std::string typed;
        for (int i = 0; i < 6; ++i) { typed += i < static_cast<int>(room.size()) ? room[i] : '_'; if (i < 5) typed += ' '; }
        cpuStencilText(canvas,typed,layout.logicalW*0.5f,layout.content.y+layout.content.h*0.50f,46.0f,0.88f,1.0f,1.0f,0.94f,stencilAge,true);
    }
    for (int i = 0; i < layout.rowCount; ++i) {
        const PhoneDisplayMenuRow& row = layout.rows[i];
        if (!row.visible) continue;
        const bool selected = row.selectable && state.hud.menuSelection == row.selectableIndex;
        if (row.peek) {
            const float top = std::max(row.visual.y, layout.content.y);
            const float bottom = std::min(row.visual.y + row.visual.h, layout.content.y + layout.content.h);
            const float height = std::max(0.0f, bottom - top);
            cpuRect(canvas, row.visual.x + 26.0f, top, row.visual.w - 52.0f, height,
                    VisualIdentity::MetallicTeal.r, VisualIdentity::MetallicTeal.g,
                    VisualIdentity::MetallicTeal.b, 0.24f);
            continue;
        }
        if (row.kind == PhoneMenuRowKind::Section) {
            cpuStencilText(canvas,row.label,row.labelX,row.baselineY,row.fontPx,VisualIdentity::MetallicTeal.r,VisualIdentity::MetallicTeal.g,VisualIdentity::MetallicTeal.b,0.62f,stencilAge);
            continue;
        }
        const PhoneMenuEmphasis emphasis=phoneMenuEmphasis(row.action);
        if (selected) {
            const float response=clampf(state.cinematic.textInteraction,0.0f,1.0f);
            const VisualColor focusColor=emphasis==PhoneMenuEmphasis::Destructive?VisualIdentity::Copper:
                (emphasis==PhoneMenuEmphasis::Primary?VisualIdentity::AcidChartreuse:channelAccent);
            cpuRect(canvas,row.visual.x+18.0f,row.visual.y+7.0f,row.visual.w-36.0f,row.visual.h-14.0f,
                focusColor.r,focusColor.g,focusColor.b,0.075f+response*0.055f);
        }
        const float alpha = selected ? 1.0f : 0.72f;
        if (row.kind == PhoneMenuRowKind::TwoColumn) {
            const VisualColor labelColor=emphasis==PhoneMenuEmphasis::Destructive?VisualIdentity::Copper:VisualColor{selected?1.0f:0.70f,selected?1.0f:0.88f,1.0f};
            cpuStencilText(canvas,row.label,row.labelX,row.baselineY,row.fontPx,labelColor.r,labelColor.g,labelColor.b,alpha,stencilAge);
            const float valueWidth=cpuStencilTextWidth(row.value,row.fontPx);
            const float valueLeft=row.valueRightX-valueWidth;
            cpuStencilText(canvas,row.value,valueLeft,row.baselineY,row.fontPx,selected?channelAccent.r:VisualIdentity::MetallicTeal.r,selected?channelAccent.g:VisualIdentity::MetallicTeal.g,selected?channelAccent.b:VisualIdentity::MetallicTeal.b,selected?0.98f:0.78f,stencilAge);
            const float amount=phoneMenuVisualAmount(row.action,state.localSettings);
            if(amount>=0.0f&&row.horizontal==PhoneMenuHorizontal::Adjust){
                const float trackW=150.0f,trackH=5.0f;
                const float trackX=row.valueRightX-trackW,trackY=row.baselineY+13.0f;
                cpuRect(canvas,trackX,trackY,trackW,trackH,VisualIdentity::DeepPlum.r,VisualIdentity::DeepPlum.g,VisualIdentity::DeepPlum.b,0.58f);
                const VisualColor fill=selected?channelAccent:VisualIdentity::MetallicTeal;
                cpuRect(canvas,trackX,trackY,std::max(trackH,trackW*amount),trackH,fill.r,fill.g,fill.b,selected?0.94f:0.62f);
                const float thumbX=trackX+clampf(amount,0.0f,1.0f)*(trackW-trackH);
                cpuRect(canvas,thumbX,trackY-2.0f,trackH,trackH+4.0f,fill.r,fill.g,fill.b,selected?1.0f:0.76f);
            }
        } else {
            const bool centered=state.dead&&row.action==PhoneMenuAction::Restart;
            const VisualColor actionColor=emphasis==PhoneMenuEmphasis::Destructive?VisualIdentity::Copper:
                (selected&&emphasis==PhoneMenuEmphasis::Primary?VisualIdentity::AcidChartreuse:VisualColor{selected?1.0f:0.70f,selected?1.0f:0.88f,1.0f});
            cpuStencilText(canvas,row.label,centered?layout.logicalW*0.5f:row.labelX,row.baselineY,row.fontPx,actionColor.r,actionColor.g,actionColor.b,alpha,stencilAge,centered);
        }
    }
    if (!layout.navigationHint.empty()) {
        const float hintPx=fitPhoneDisplayTextPx(layout.navigationHint,26.0f,layout.safe.w,false,cpuTextWidth);
        cpuStencilText(canvas,layout.navigationHint,layout.logicalW*0.5f,layout.logicalH-66.0f,
            hintPx,channelAccent.r,channelAccent.g,channelAccent.b,0.62f,stencilAge,true);
    }
}

std::uint64_t phoneDisplayRenderKey(const GameState& state) {
    std::uint64_t hash = 1469598103934665603ull;
    const PhoneDisplayState& display = state.phoneDisplay;
    hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(display.mode));
    hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(display.previousMode));
    hashPhoneDisplayValue(hash, display.interactive ? 1u : 0u);
    hashPhoneDisplayFloat(hash, display.brightness, 255.0f);
    hashPhoneDisplayFloat(hash, display.contentOpacity, 255.0f);
    hashPhoneDisplayFloat(hash, display.transitionProgress, 240.0f);
    hashPhoneDisplayFloat(hash, display.screenTint.x, 255.0f);
    hashPhoneDisplayFloat(hash, display.screenTint.y, 255.0f);
    hashPhoneDisplayFloat(hash, display.screenTint.z, 255.0f);
    hashPhoneDisplayFloat(hash, display.material.rimEmission, 255.0f);
    hashPhoneDisplayValue(hash, state.started ? 1u : 0u);
    hashPhoneDisplayValue(hash, state.dead ? 1u : 0u);
    hashPhoneDisplayValue(hash, state.uiPaused ? 1u : 0u);
    hashPhoneDisplayValue(hash, state.cinematic.introActive ? 1u : 0u);
    hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(state.localSettings.menuPage));
    hashPhoneDisplayFloat(hash, state.localSettings.menuScroll, 10.0f);
    hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(std::max(0, state.hud.menuSelection)));
    hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(std::max(-1, state.localSettings.rebindingAction) + 1));
    hashPhoneDisplayValue(hash, state.localSettings.musicMuted ? 1u : 0u);
    hashPhoneDisplayValue(hash, state.localSettings.sfxMuted ? 1u : 0u);
    hashPhoneDisplayValue(hash, state.localSettings.shadows ? 1u : 0u);
    hashPhoneDisplayValue(hash, state.localSettings.particles ? 1u : 0u);
    hashPhoneDisplayValue(hash, state.localSettings.fpsCounter ? 1u : 0u);
    hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(std::max(0, state.localSettings.graphicsPreset)));
    hashPhoneDisplayFloat(hash, state.localSettings.musicVolume, 100.0f);
    hashPhoneDisplayFloat(hash, state.localSettings.sfxVolume, 100.0f);
    hashPhoneDisplayFloat(hash, state.localSettings.mouseLookSensitivity, 100.0f);
    hashPhoneDisplayFloat(hash, state.localSettings.controllerLookSensitivity, 100.0f);
    hashPhoneDisplayValue(hash, state.localSettings.controllerTriggerSensitivity);
    hashPhoneDisplayValue(hash, state.localSettings.controllerVibration);
    hashPhoneDisplayFloat(hash, state.vacuum.power, 240.0f);
    hashPhoneDisplayFloat(hash, state.hud.criticalHitPulse, 120.0f);
    hashPhoneDisplayValue(hash, state.hud.lowBattery ? 1u : 0u);
    const GameplayPhoneModel phone=makeGameplayPhoneModel(state);
    hashPhoneDisplayFloat(hash,phone.batteryFill,100.0f);
    hashPhoneDisplayValue(hash,static_cast<std::uint64_t>(phone.storedSouls));
    hashPhoneDisplayValue(hash,static_cast<std::uint64_t>(phone.filledGoals));
    hashPhoneDisplayValue(hash,static_cast<std::uint64_t>(phone.requiredGoals));
    hashPhoneDisplayValue(hash,phone.roomClear?1u:0u);
    hashPhoneDisplayValue(hash,static_cast<std::uint64_t>(phone.roomIndex));
    hashPhoneDisplayValue(hash,static_cast<std::uint64_t>(phone.tokens));
    hashPhoneDisplayValue(hash,phone.supplementalActive?1u:0u);
    hashPhoneDisplayFloat(hash,phone.supplementalFill,100.0f);
    hashPhoneDisplayValue(hash,static_cast<std::uint64_t>(phone.flowerStacks));
    for (int key : state.localSettings.keyboardBindings) {
        hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(std::max(0, key)));
    }
    hashPhoneDisplayString(hash, state.multiplayer.roomCode.data());

    const PhoneMenuPageViewModel page = makePhoneMenuPageModel(state);
    hashPhoneDisplayString(hash, page.title);
    hashPhoneDisplayValue(hash, page.paletteTitle ? 1u : 0u);
    hashPhoneDisplayValue(hash, page.joinCode ? 1u : 0u);
    hashPhoneDisplayValue(hash, page.tablePage ? 1u : 0u);
    hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(page.elementCount));
    for (int i = 0; i < page.elementCount; ++i) {
        const PhoneMenuElement& element = page.elements[i];
        hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(element.kind));
        hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(element.action));
        hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(element.horizontal));
        hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(std::max(-1, element.bindingAction) + 1));
        hashPhoneDisplayValue(hash, element.selectable ? 1u : 0u);
        hashPhoneDisplayString(hash, element.label);
        hashPhoneDisplayString(hash, element.value);
    }
    if (page.paletteTitle) {
        hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(std::floor(state.time * 15.0f)));
    }
    const PhoneMenuElement* selectedElement=phoneMenuElementForSelection(page,state.hud.menuSelection);
    if(selectedElement&&selectedElement->horizontal==PhoneMenuHorizontal::Adjust)
        hashPhoneDisplayValue(hash,static_cast<std::uint64_t>(std::floor(state.time*8.0f)));
    if (!state.dead && state.started && !state.uiPaused) {
        hashPhoneDisplayValue(hash, static_cast<std::uint64_t>(std::floor(state.time * 10.0f)));
        hashPhoneDisplayFloat(hash, display.powerPulse, 100.0f);
        hashPhoneDisplayFloat(hash, display.capturePulse, 100.0f);
        hashPhoneDisplayFloat(hash, display.screenNoisePhase, 20.0f);
    }
    return hash;
}

void DesktopRenderer::drawBox(const Vec3& p, const Vec3& s, const Quat& q, float r, float g, float b) {
    const float matrix[16] = {
        1-2*(q.y*q.y+q.z*q.z), 2*(q.x*q.y+q.z*q.w), 2*(q.x*q.z-q.y*q.w), 0,
        2*(q.x*q.y-q.z*q.w), 1-2*(q.x*q.x+q.z*q.z), 2*(q.y*q.z+q.x*q.w), 0,
        2*(q.x*q.z+q.y*q.w), 2*(q.y*q.z-q.x*q.w), 1-2*(q.x*q.x+q.y*q.y), 0,
        0,0,0,1
    };
    glPushMatrix(); glTranslatef(p.x,p.y,p.z); glMultMatrixf(matrix); glScalef(s.x,s.y,s.z);
    gradedColor(r,g,b); cube(); glPopMatrix();
}

void DesktopRenderer::drawSecretTvScreen(const GameState& state, float phoneProximity) const {
    if(!tvGifWall_.available())return;
    if(!tvScreenTexture_)glGenTextures(1,&tvScreenTexture_);
    const float fullness=clampf(static_cast<float>(state.secretTv.signal)/24.0f,0.0f,1.0f);
    const float clarity=0.80f+0.20f*fullness;
    const float proximityWash=phoneProximity*(1.0f-fullness);
    const float flicker=1.0f-proximityWash*(0.035f+0.030f*std::sin(state.time*15.0f));
    const float brokenDim=state.secretTv.broken?0.38f:1.0f;
    unsigned char pixels[TvGifWall::Columns*TvGifWall::Rows*3]{};
    for(int y=0;y<TvGifWall::Rows;++y)for(int x=0;x<TvGifWall::Columns;++x){
        const auto color=tvGifWall_.sample(x,y,state.time,state.secretTv.signal);
        const float slowBand=1.0f-proximityWash*0.045f*std::sin(state.time*2.3f+static_cast<float>(y)*0.75f);
        const float gain=clampf(clarity*flicker*slowBand*brokenDim,0.0f,1.22f);
        const std::size_t at=static_cast<std::size_t>((TvGifWall::Rows-1-y)*TvGifWall::Columns+x)*3u;
        pixels[at+0]=static_cast<unsigned char>(clampf(color.r*gain,0.0f,1.0f)*255.0f);
        pixels[at+1]=static_cast<unsigned char>(clampf(color.g*gain,0.0f,1.0f)*255.0f);
        pixels[at+2]=static_cast<unsigned char>(clampf(color.b*gain,0.0f,1.0f)*255.0f);
    }
    glBindTexture(GL_TEXTURE_2D,tvScreenTexture_);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,TvGifWall::Columns,TvGifWall::Rows,0,GL_RGB,GL_UNSIGNED_BYTE,pixels);
    glDisable(GL_LIGHTING);glEnable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
    glColor4f(1.0f,1.0f,1.0f,state.secretTv.broken?0.74f:0.98f);
    constexpr float x=41.785f,cy=0.80f,cz=0.0f,halfY=0.455f,halfZ=0.655f;
    glBegin(GL_QUADS);
    glNormal3f(-1,0,0);
    glTexCoord2f(0,0);glVertex3f(x,cy-halfY,cz-halfZ);
    glTexCoord2f(1,0);glVertex3f(x,cy-halfY,cz+halfZ);
    glTexCoord2f(1,1);glVertex3f(x,cy+halfY,cz+halfZ);
    glTexCoord2f(0,1);glVertex3f(x,cy+halfY,cz-halfZ);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    const float sheen=0.10f+0.08f*std::sin(state.time*0.7f);
    glColor4f(VisualIdentity::ElectricCyan.r,VisualIdentity::ElectricCyan.g,VisualIdentity::ElectricCyan.b,0.08f+sheen*0.22f);
    glBegin(GL_QUADS);
    glVertex3f(x-0.002f,cy+halfY*0.82f,cz-halfZ);
    glVertex3f(x-0.002f,cy+halfY*0.82f,cz+halfZ);
    glVertex3f(x-0.002f,cy+halfY,cz+halfZ);
    glVertex3f(x-0.002f,cy+halfY,cz-halfZ);
    glEnd();
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_TEXTURE_2D);glEnable(GL_LIGHTING);
}

void DesktopRenderer::drawPhoneDisplayTexture(const GameState& state) const {
    if (!phoneDisplayTexture_) glGenTextures(1, &phoneDisplayTexture_);
    glBindTexture(GL_TEXTURE_2D, phoneDisplayTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const std::uint64_t renderKey = phoneDisplayRenderKey(state);
    if (!phoneDisplayCacheValid_ || renderKey != phoneDisplayCacheKey_ || !phoneDisplayTextureAllocated_) {
        renderPhoneDisplayPixels(state, phoneDisplayPixels_);
        if (!phoneDisplayTextureAllocated_) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PhoneDisplayState::LogicalWidth, PhoneDisplayState::LogicalHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, phoneDisplayPixels_.data());
            phoneDisplayTextureAllocated_ = true;
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, PhoneDisplayState::LogicalWidth, PhoneDisplayState::LogicalHeight, GL_RGBA, GL_UNSIGNED_BYTE, phoneDisplayPixels_.data());
        }
        phoneDisplayCacheKey_ = renderKey;
        phoneDisplayCacheValid_ = true;
    }

    const PhoneTransformState& phone = state.phoneTransform;
    const float halfW = PHONE_SCREEN_WIDTH * state.phoneVisual.screenScale.x * 0.5f;
    const float halfH = PHONE_SCREEN_HEIGHT * state.phoneVisual.screenScale.y * 0.5f;
    const Vec3 center = phone.screenCenter + phone.screenNormal * (PHONE_SCREEN_DEPTH * 0.52f);
    const Vec3 rx = phone.screenRight * halfW;
    const Vec3 uy = phone.screenUp * halfH;
    const PhoneDisplayState& display = state.phoneDisplay;
    const float alpha = clampf(0.12f + display.brightness * 0.88f, 0.0f, 1.0f);

    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex3f((center-rx-uy).x, (center-rx-uy).y, (center-rx-uy).z);
    glTexCoord2f(1, 1); glVertex3f((center+rx-uy).x, (center+rx-uy).y, (center+rx-uy).z);
    glTexCoord2f(1, 0); glVertex3f((center+rx+uy).x, (center+rx+uy).y, (center+rx+uy).z);
    glTexCoord2f(0, 0); glVertex3f((center-rx+uy).x, (center-rx+uy).y, (center-rx+uy).z);
    glEnd();

    const Vec3 rim = display.emissionColor;
    glDisable(GL_TEXTURE_2D);
    glColor4f(rim.x, rim.y, rim.z, clampf(display.material.rimEmission * 0.22f, 0.03f, 0.16f));
    glBegin(GL_LINE_LOOP);
    glVertex3f((center-rx-uy).x, (center-rx-uy).y, (center-rx-uy).z);
    glVertex3f((center+rx-uy).x, (center+rx-uy).y, (center+rx-uy).z);
    glVertex3f((center+rx+uy).x, (center+rx+uy).y, (center+rx+uy).z);
    glVertex3f((center-rx+uy).x, (center-rx+uy).y, (center-rx+uy).z);
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
}

void DesktopRenderer::drawStaticModel(unsigned int list, const Vec3& p, const Vec3& s, const Quat& q) {
    const float matrix[16] = {
        1-2*(q.y*q.y+q.z*q.z), 2*(q.x*q.y+q.z*q.w), 2*(q.x*q.z-q.y*q.w), 0,
        2*(q.x*q.y-q.z*q.w), 1-2*(q.x*q.x+q.z*q.z), 2*(q.y*q.z+q.x*q.w), 0,
        2*(q.x*q.z+q.y*q.w), 2*(q.y*q.z-q.x*q.w), 1-2*(q.x*q.x+q.y*q.y), 0,
        0,0,0,1
    };
    glPushMatrix(); glTranslatef(p.x,p.y,p.z); glMultMatrixf(matrix); glScalef(s.x,s.y,s.z); glCallList(list); glPopMatrix();
}

void DesktopRenderer::drawHumanModel(const TargetState& target,float time,room_environment::RoomSetting setting,bool shadow) const {
    humanModel_.skin(target.humanAnimationTime,target.attackTimer,target.attackVariant,humanVertices_,{target.hitFlash,target.hitDirectionLocal,time});if(humanVertices_.empty())return;
    const bool aliveHuman=!target.slurpable;const HumanVisualPose pose=makeHumanVisualPose(target.visualYaw,target.scale,time,target.visualReaction,aliveHuman);
    const float attackT=target.attackTimer>0?1-clampf(target.attackTimer/HUMAN_SWING_ATTACK_DURATION,0.0f,1.0f):0;
    const float windup=std::sin(clampf(attackT/HUMAN_SWING_COMMIT_PHASE,0.0f,1.0f)*PI*0.5f)*(attackT<HUMAN_SWING_COMMIT_PHASE?1.0f:0.0f),strike=std::sin(clampf((attackT-HUMAN_SWING_COMMIT_PHASE)/(HUMAN_SWING_END_PHASE-HUMAN_SWING_COMMIT_PHASE),0.0f,1.0f)*PI);
    const float side=target.attackVariant%2==0?1.0f:-1.0f,low=target.attackVariant>=2?1.0f:0.0f;
    const float reach=target.attackTimer>0?smoothStep01(clampf((attackT-HUMAN_SWING_COMMIT_PHASE)/(HUMAN_SWING_END_PHASE-HUMAN_SWING_COMMIT_PHASE),0.0f,1.0f)):0.0f;
    const Vec3 attackForward=lengthSq(target.attackDirection)>0.001f?normalized(target.attackDirection):Vec3{-std::sin(target.visualYaw),0,-std::cos(target.visualYaw)};
    const Vec3 bodyRight{attackForward.z,0,-attackForward.x};
    const float forwardSpeed=dot3(target.vel,attackForward),sideSpeed=dot3(target.vel,bodyRight);
    const float speed=std::sqrt(target.vel.x*target.vel.x+target.vel.z*target.vel.z);
    const float plantedPulse=std::sin(target.visualWalkPhase*0.5f)*clampf(speed/5.0f,0.0f,1.0f);
    const float motionPitch=clampf(-forwardSpeed*0.052f,-0.34f,0.20f);
    const float motionRoll=clampf(sideSpeed*0.060f+plantedPulse*0.045f,-0.30f,0.30f);
    const float impactPitch=-target.hitFlash*0.24f-target.vacuumPullAmount*0.16f;
    const float impactRoll=target.hitDirectionLocal*target.hitFlash*0.30f;
    const Quat rootQ=quaternionFromEulerXYZ(
        motionPitch+impactPitch+(target.attackTimer>0?windup*0.08f-reach*(0.16f+low*0.05f):0),
        target.visualYaw+PI,
        motionRoll+impactRoll+(target.attackTimer>0?side*(strike*0.18f-windup*0.24f):0));
    const Vec3 attackLunge=attackForward*(target.attackTimer>0?reach*0.075f*target.scale:0.0f);
    const Vec3 root{target.pos.x+attackLunge.x,target.pos.y+(target.attackTimer>0?std::sin(attackT*PI)*0.024f*low:0),target.pos.z+attackLunge.z};
    const float matrix[16]={1-2*(rootQ.y*rootQ.y+rootQ.z*rootQ.z),2*(rootQ.x*rootQ.y+rootQ.z*rootQ.w),2*(rootQ.x*rootQ.z-rootQ.y*rootQ.w),0,2*(rootQ.x*rootQ.y-rootQ.z*rootQ.w),1-2*(rootQ.x*rootQ.x+rootQ.z*rootQ.z),2*(rootQ.y*rootQ.z+rootQ.x*rootQ.w),0,2*(rootQ.x*rootQ.z+rootQ.y*rootQ.w),2*(rootQ.y*rootQ.z-rootQ.x*rootQ.w),1-2*(rootQ.x*rootQ.x+rootQ.y*rootQ.y),0,0,0,0,1};
    // Model-file materials are deliberately not authoritative here: the source
    // asset is pale and made enemies read as unstyled mannequins. Gameplay
    // actors belong to Data's dark shell / bright signal value hierarchy.
    const VisualColor base=target.brute?VisualIdentity::BruteEnemy:VisualIdentity::NormalEnemy;const VisualColor damageColor=humanDamageSurfaceColor(base,setting,target.armor,target.brute?4.0f:2.0f,target.slurpable,target.hitFlash);
    const auto& reaction=target.visualReaction;
    const float searchSignal=reaction.searchAmount*reaction.awareness*(0.12f+reaction.uncertainty*0.10f);
    const float threatSignal=reaction.commitment*reaction.awareness*0.22f;
    const float fractureSignal=reaction.disruption*0.16f;
    const VisualColor signalColor{
        damageColor.r+(VisualIdentity::ElectricCyan.r-damageColor.r)*searchSignal+(VisualIdentity::ElectricMagenta.r-damageColor.r)*threatSignal+(VisualIdentity::WarmGold.r-damageColor.r)*fractureSignal,
        damageColor.g+(VisualIdentity::ElectricCyan.g-damageColor.g)*searchSignal+(VisualIdentity::ElectricMagenta.g-damageColor.g)*threatSignal+(VisualIdentity::WarmGold.g-damageColor.g)*fractureSignal,
        damageColor.b+(VisualIdentity::ElectricCyan.b-damageColor.b)*searchSignal+(VisualIdentity::ElectricMagenta.b-damageColor.b)*threatSignal+(VisualIdentity::WarmGold.b-damageColor.b)*fractureSignal};
    const bool parryCue=target.attackTimer>0&&attackT>=0.22f&&attackT<=0.46f;const float cue=parryCue?(0.10f+0.05f*std::sin(time*28.0f)):0.0f;const float cueColor[4]={signalColor.r+(0.55f-signalColor.r)*cue,signalColor.g+(0.96f-signalColor.g)*cue,signalColor.b+(1.0f-signalColor.b)*cue,humanModel_.color[3]};
    glPushMatrix();glTranslatef(root.x,root.y,root.z);glMultMatrixf(matrix);glScalef(pose.scale*pose.expressiveScale.x,pose.scale*pose.expressiveScale.y,pose.scale*pose.expressiveScale.z);if(shadow)glColor4f(0.012f,0.018f,0.022f,0.28f);else gradedColor(cueColor[0],cueColor[1],cueColor[2],cueColor[3]);glBegin(GL_TRIANGLES);
    const float thinning=humanShellThinningAmount(target.armor,target.brute?4.0f:2.0f,target.slurpable);
    for(std::size_t i=0;i+8<humanVertices_.size();i+=9){const std::size_t triangle=i/9;const Vec3 rawA{humanVertices_[i],humanVertices_[i+1],humanVertices_[i+2]},rawB{humanVertices_[i+3],humanVertices_[i+4],humanVertices_[i+5]},rawC{humanVertices_[i+6],humanVertices_[i+7],humanVertices_[i+8]},center=(rawA+rawB+rawC)*(1.0f/3.0f);if(humanShellTriangleMissingTowardCrit(triangle,thinning,center))continue;const Vec3 a=humanShellAbsorbTowardCrit(rawA,triangle,thinning),b=humanShellAbsorbTowardCrit(rawB,triangle,thinning),c=humanShellAbsorbTowardCrit(rawC,triangle,thinning),n=normalized(cross3(b-a,c-a));glNormal3f(n.x,n.y,n.z);glVertex3f(a.x,a.y,a.z);glVertex3f(b.x,b.y,b.z);glVertex3f(c.x,c.y,c.z);}glEnd();glPopMatrix();
}

void DesktopRenderer::drawSoulFlesh(const TargetState& target,const Vec3& center){
    auto index=[](int x,int y,int z){return x+y*3+z*9;};
    auto emitQuad=[&](int ia,int ib,int ic,int id){const Vec3 a=center+target.latticeSurfacePos[ia],b=center+target.latticeSurfacePos[ib],c=center+target.latticeSurfacePos[ic],d=center+target.latticeSurfacePos[id];Vec3 n=normalized(cross3(b-a,c-a));glNormal3f(n.x,n.y,n.z);glVertex3f(a.x,a.y,a.z);glVertex3f(b.x,b.y,b.z);glVertex3f(c.x,c.y,c.z);n=normalized(cross3(c-a,d-a));glNormal3f(n.x,n.y,n.z);glVertex3f(a.x,a.y,a.z);glVertex3f(c.x,c.y,c.z);glVertex3f(d.x,d.y,d.z);};
    gradedColor(224.0f/255.0f,160.0f/255.0f,143.0f/255.0f);glBegin(GL_TRIANGLES);
    for(int y=0;y<2;++y)for(int z=0;z<2;++z){emitQuad(index(0,y,z),index(0,y+1,z),index(0,y+1,z+1),index(0,y,z+1));emitQuad(index(2,y,z),index(2,y,z+1),index(2,y+1,z+1),index(2,y+1,z));}
    for(int x=0;x<2;++x)for(int z=0;z<2;++z){emitQuad(index(x,0,z),index(x,0,z+1),index(x+1,0,z+1),index(x+1,0,z));emitQuad(index(x,2,z),index(x+1,2,z),index(x+1,2,z+1),index(x,2,z+1));}
    for(int x=0;x<2;++x)for(int y=0;y<2;++y){emitQuad(index(x,y,0),index(x+1,y,0),index(x+1,y+1,0),index(x,y+1,0));emitQuad(index(x,y,2),index(x,y+1,2),index(x+1,y+1,2),index(x+1,y,2));}
    glEnd();
    if(target.tetherVisible){const Vec3 endpoint=target.tetherAnchor;const Vec3 destination=target.tetherDestination;const Vec3 delta=destination-endpoint;const float len=length(delta);if(len>0.001f){const Vec3 mid=endpoint+delta*0.5f;const float yaw=std::atan2(delta.x,delta.z),pitch=-std::asin(clampf(delta.y/len,-1,1));glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);drawBox(mid,{0.13f*target.tetherWidth,0.13f*target.tetherWidth,std::min(len,4.5f)},pitch,yaw,0,VisualIdentity::Tether.r,VisualIdentity::Tether.g,VisualIdentity::Tether.b,0.34f);glDepthMask(GL_TRUE);glDisable(GL_BLEND);}}
}

void DesktopRenderer::drawFieldGrass(int tileIndex) const{
    if(!fieldGrassTexture_){const auto pixels=field_grass_texture::pixels();glGenTextures(1,&fieldGrassTexture_);glBindTexture(GL_TEXTURE_2D,fieldGrassTexture_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,field_grass_texture::Size,field_grass_texture::Size,0,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());}
    const float z0=static_cast<float>(tileIndex)*ROOM_DEPTH,scale=render_contract::FieldOpenGround.textureWorldScale;
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,fieldGrassTexture_);glColor3f(1,1,1);glNormal3f(0,1,0);glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex3f(-ROOM_WIDTH*0.5f,0.003f,z0-ROOM_DEPTH*0.5f);glTexCoord2f(ROOM_WIDTH/scale,0);glVertex3f(ROOM_WIDTH*0.5f,0.003f,z0-ROOM_DEPTH*0.5f);glTexCoord2f(ROOM_WIDTH/scale,ROOM_DEPTH/scale);glVertex3f(ROOM_WIDTH*0.5f,0.003f,z0+ROOM_DEPTH*0.5f);glTexCoord2f(0,ROOM_DEPTH/scale);glVertex3f(-ROOM_WIDTH*0.5f,0.003f,z0+ROOM_DEPTH*0.5f);glEnd();glDisable(GL_TEXTURE_2D);
}

void DesktopRenderer::drawCityGround(int tileIndex) const{
    if(!citySurfaceTexture_){const auto pixels=city_surface_texture::pixels();glGenTextures(1,&citySurfaceTexture_);glBindTexture(GL_TEXTURE_2D,citySurfaceTexture_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,city_surface_texture::Size,city_surface_texture::Size,0,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());}
    const float z0=static_cast<float>(tileIndex)*ROOM_DEPTH,scale=render_contract::CityGround.textureWorldScale;
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,citySurfaceTexture_);glColor3f(1,1,1);glNormal3f(0,1,0);glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex3f(-ROOM_WIDTH*0.5f,0.003f,z0-ROOM_DEPTH*0.5f);glTexCoord2f(ROOM_WIDTH/scale,0);glVertex3f(ROOM_WIDTH*0.5f,0.003f,z0-ROOM_DEPTH*0.5f);glTexCoord2f(ROOM_WIDTH/scale,ROOM_DEPTH/scale);glVertex3f(ROOM_WIDTH*0.5f,0.003f,z0+ROOM_DEPTH*0.5f);glTexCoord2f(0,ROOM_DEPTH/scale);glVertex3f(-ROOM_WIDTH*0.5f,0.003f,z0+ROOM_DEPTH*0.5f);glEnd();glDisable(GL_TEXTURE_2D);
}

void DesktopRenderer::drawFacetedRock(const room_environment::EnvironmentPropSpec& prop,int roomSeed,int roomIndex,int propIndex,float zOffset,const VisualColor& color){
    const auto mesh=faceted_rock::makeMesh(prop,roomSeed,roomIndex,propIndex,zOffset);
    gradedColor(color.r,color.g,color.b);glBegin(GL_TRIANGLES);
    for(int i=0;i<mesh.vertexCount;++i){glNormal3f(mesh.normals[i*3],mesh.normals[i*3+1],mesh.normals[i*3+2]);glVertex3f(mesh.positions[i*3],mesh.positions[i*3+1],mesh.positions[i*3+2]);}
    glEnd();
}

void DesktopRenderer::drawSlopeWedge(const SlopeSupport& slope,float zOffset,const VisualColor& color){
    const auto mesh=makeSlopeWedgeMesh(slope,zOffset);gradedColor(color.r,color.g,color.b);glBegin(GL_TRIANGLES);
    for(int i=0;i<mesh.vertexCount;++i){glNormal3f(mesh.normals[i*3],mesh.normals[i*3+1],mesh.normals[i*3+2]);glVertex3f(mesh.positions[i*3],mesh.positions[i*3+1],mesh.positions[i*3+2]);}glEnd();
}

void DesktopRenderer::drawRoomTile(const GameState& state,int tileIndex,const scene_lighting_response::Response& lightingResponse) const {
    const auto plan=room_environment::roomPlan(state.roomSeed,state.roomIndex);
    const auto lightRig=room_lighting::roomLightRig(plan.setting,plan.form);
    const float z0 = static_cast<float>(tileIndex) * ROOM_DEPTH;
    const float doorWidth = 5.35f;
    const float doorHeight = 3.95f;
    const float sideW = (ROOM_WIDTH - doorWidth) * 0.5f;
    const float sideX = doorWidth * 0.5f + sideW * 0.5f;
    const float topH = ROOM_WALL_HEIGHT - doorHeight;
    const float topY = doorHeight + topH * 0.5f;
    const float wallR = VisualIdentity::RoomWall.r, wallG = VisualIdentity::RoomWall.g, wallB = VisualIdentity::RoomWall.b;
    const bool field=plan.setting==room_environment::RoomSetting::Field,sterile=plan.setting==room_environment::RoomSetting::Sterile,coastal=plan.setting==room_environment::RoomSetting::Coastal;
    drawBox({0,-0.04f,z0},{ROOM_WIDTH,0.08f,ROOM_DEPTH},0,0,0,field?VisualIdentity::FieldGround.r:(sterile?0.58f:(coastal?0.24f:VisualIdentity::RoomFloor.r)),field?VisualIdentity::FieldGround.g:(sterile?0.61f:(coastal?0.43f:VisualIdentity::RoomFloor.g)),field?VisualIdentity::FieldGround.b:(sterile?0.63f:(coastal?0.50f:VisualIdentity::RoomFloor.b)));
    if(field&&plan.form==room_environment::RoomForm::Open)drawFieldGrass(tileIndex);
    if(plan.setting==room_environment::RoomSetting::City)drawCityGround(tileIndex);
    if(coastal)drawBox({0,0.005f,z0},{23.5f,0.01f,35.5f},0,0,0,0.64f,0.58f,0.43f);
    if(sterile){
        drawBox({0,ROOM_WALL_HEIGHT+0.08f,z0},{ROOM_WIDTH,0.16f,ROOM_DEPTH},0,0,0,wallR,wallG,wallB);
        glDisable(GL_LIGHTING);
        for(int i=0;i<lightRig.localLightCount;++i){const auto& fixture=lightRig.localLights[i];if(fixture.visibleFixture)drawBox(fixture.localPosition+Vec3{0,0,z0},fixture.fixtureSize,0,0,0,fixture.color.r*fixture.intensity,fixture.color.g*fixture.intensity,fixture.color.b*fixture.intensity);}
        glEnable(GL_LIGHTING);
    }
    for (float seam : {-ROOM_DEPTH*0.5f, ROOM_DEPTH*0.5f}) {
        drawBox({-sideX,ROOM_WALL_HEIGHT*0.5f,z0+seam},{sideW,ROOM_WALL_HEIGHT,0.5f},0,0,0,wallR,wallG,wallB);
        drawBox({ sideX,ROOM_WALL_HEIGHT*0.5f,z0+seam},{sideW,ROOM_WALL_HEIGHT,0.5f},0,0,0,wallR,wallG,wallB);
        drawBox({0,topY,z0+seam},{doorWidth,topH,0.5f},0,0,0,wallR,wallG,wallB);
    }
    drawBox({-ROOM_WIDTH*0.5f,ROOM_WALL_HEIGHT*0.5f,z0},{0.5f,ROOM_WALL_HEIGHT,ROOM_DEPTH},0,0,0,wallR,wallG,wallB);
    drawBox({ ROOM_WIDTH*0.5f,ROOM_WALL_HEIGHT*0.5f,z0},{0.5f,ROOM_WALL_HEIGHT,ROOM_DEPTH},0,0,0,wallR,wallG,wallB);
    if(tileIndex==state.topology.currentTileIndex&&lightingResponse.exitGlow>0.01f){
        const VisualColor guide=sterile?VisualColor{0.62f,0.88f,0.94f}:VisualColor{0.72f,0.90f,0.82f};
        glDisable(GL_LIGHTING);drawBox({0,doorHeight+0.12f,z0-ROOM_DEPTH*0.5f+0.27f},{doorWidth,0.12f,0.08f},0,0,0,guide.r*(0.38f+lightingResponse.exitGlow*0.62f),guide.g*(0.38f+lightingResponse.exitGlow*0.62f),guide.b*(0.38f+lightingResponse.exitGlow*0.62f));glEnable(GL_LIGHTING);
    }
    const int authoredObstacleCount=(state.traversalLab||state.slopeLab)?state.debug.colliderCount:std::min(state.debug.colliderCount,plan.obstacleCount);
    for (int i=0;i<authoredObstacleCount;++i) {
        const RoomCollider& c=state.roomColliders[i];
        drawBox({c.center.x,c.center.y,z0+c.center.z},{c.width,c.height,c.depth},0,0,0,VisualIdentity::RoomObstacle.r,VisualIdentity::RoomObstacle.g,VisualIdentity::RoomObstacle.b);
        if(plan.setting==room_environment::RoomSetting::City&&plan.form==room_environment::RoomForm::Corridor&&room_environment::obstacleRole(plan,state.roomSeed,state.roomIndex,i)==room_environment::EnvironmentRole::Landmark){
            const float tierH=gameplay::WORLD_SCALE.storyHeight*0.34f;
            drawBox({c.center.x,c.topY+tierH*0.5f,z0+c.center.z},{c.width*0.58f,tierH,c.depth*0.62f},0,0,0,0.34f,0.40f,0.44f);
        }
    }
    for(int i=0;i<state.slopeSupportCount;++i)drawSlopeWedge(state.slopeSupports[i],z0,{0.39f,0.42f,0.36f});
    if(state.slopeLab)for(int i=0;i<state.rockSupportCount;++i){const auto& rock=state.rockSupports[i];const VisualColor substrate=roomSubstrateColor(plan.setting);drawFacetedRock(rock.prop,rock.roomSeed,rock.roomIndex,rock.propIndex,z0,{substrate.r*0.82f,substrate.g*0.82f,substrate.b*0.82f});}
    const auto traversalPresentation=room_environment::traversalPresentationFor(plan.setting,state.roomInspector||state.traversalLab);
    const auto geometry=state.slopeLab?room_environment::RoomGeometryCapacityPlan{}:room_environment::roomGeometryCapacityPlan(plan,state.roomSeed,state.roomIndex,ROOM_COLLIDER_COUNT);
    for(int i=0;i<plan.traversal.surfaceCount;++i){const auto& surface=plan.traversal.surfaces[i];if(!geometry.traversalIncluded[i]||room_environment::usesShallowElevation(plan,surface))continue;const auto spec=room_environment::physicalTraversalObstacle(surface);
        drawBox(spec.center+Vec3{0,0,z0},spec.size,0,0,0,traversalPresentation.color.x,traversalPresentation.color.y,traversalPresentation.color.z);
    }
    if(plan.sidewalks){
        const bool canyon=plan.form==room_environment::RoomForm::Canyon,skyline=plan.form==room_environment::RoomForm::Skyline;
        const float walkX=canyon?3.55f:(skyline?8.15f:5.2f),walkW=canyon?1.1f:(skyline?2.4f:1.35f);
        drawBox({-walkX,0.025f,z0},{walkW,0.05f,ROOM_DEPTH-1.0f},0,0,0,0.43f,0.45f,0.46f);drawBox({walkX,0.025f,z0},{walkW,0.05f,ROOM_DEPTH-1.0f},0,0,0,0.43f,0.45f,0.46f);
    }
    for(int i=0;i<room_environment::environmentPropCount(plan);++i){
        if(!geometry.propIncluded[i])continue;
        const auto prop=room_environment::environmentProp(plan,state.roomSeed,state.roomIndex,i);const Vec3 p=prop.center+Vec3{0,0,z0};
        using room_environment::EnvironmentPrimitive;
        if(prop.primitive==EnvironmentPrimitive::House){for(const auto& part:house_geometry::parts(prop,z0)){const VisualColor color=part.surface==0?VisualColor{0.40f,0.47f,0.50f}:(part.surface==3?VisualColor{0.05f,0.08f,0.09f}:VisualColor{part.surface==1?0.30f:0.26f,part.surface==1?0.37f:0.32f,part.surface==1?0.41f:0.36f});drawBox(part.center,part.size,0,part.yaw,0,color.r,color.g,color.b);}}
        else if(prop.primitive==EnvironmentPrimitive::Tree){
            int trunkIndex=0;for(const auto& part:tree_geometry::trunkParts(prop,z0)){const float shade=static_cast<float>(trunkIndex++);drawBox(part.center,part.size,0,part.yaw,0,0.25f+shade*0.02f,0.20f+shade*0.01f,0.14f);}
            bool activeClimb=false;
            if(state.player.treeClimbing&&tileIndex==state.topology.currentTileIndex&&state.player.treeCollider>=0&&state.player.treeCollider<state.debug.colliderCount){
                const RoomCollider& climbTree=state.roomColliders[state.player.treeCollider];
                activeClimb=climbTree.kind==RoomColliderKind::TreeTrunk&&std::abs(climbTree.center.x-prop.center.x)<0.05f&&std::abs(climbTree.center.z-prop.center.z)<0.05f;
            }
            for(const auto& part:tree_geometry::crownParts(prop,z0)){
                const float radius=std::max(part.size.x,part.size.z)*0.58f;
                const bool occluding=activeClimb&&room_environment::treeFoliageClusterOccludes(state.camera.pos,state.player.pos,part.center,radius);
                if(occluding){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);}
                drawBox(part.center,part.size,0,part.yaw,0,0.16f,0.43f,0.23f,occluding?0.28f:1.0f);
                if(occluding){glDepthMask(GL_TRUE);glDisable(GL_BLEND);}
            }
        }
        else if(prop.primitive==EnvironmentPrimitive::LawnFragment)drawBox(p,prop.size,0,prop.yaw,0,0.20f,0.39f,0.23f);
        else if(prop.primitive==EnvironmentPrimitive::Ruin){for(const auto& part:ruin_geometry::parts(prop,z0)){const VisualColor color=part.surface==0?VisualColor{0.38f,0.36f,0.30f}:VisualColor{0.29f,0.28f,0.25f};drawBox(part.center,part.size,0,0,0,color.r,color.g,color.b);}}
        else if(prop.primitive==EnvironmentPrimitive::Rock){const VisualColor substrate=roomSubstrateColor(plan.setting);drawFacetedRock(prop,state.roomSeed,state.roomIndex,i,z0,{substrate.r*0.82f,substrate.g*0.82f,substrate.b*0.82f});}
        else {for(const auto& part:marker_pillar_geometry::parts(prop,z0)){const VisualColor color=part.surface==0?VisualColor{0.48f,0.55f,0.58f}:VisualColor{0.72f,0.90f,0.94f};drawBox(part.center,part.size,0,0,0,color.r,color.g,color.b);}}
    }
    if(plan.grass){const int maximum=state.localSettings.graphicsPreset<=0?room_environment::GrassBladeCountLow:room_environment::GrassBladeCountHigh,grassCount=static_cast<int>(maximum*plan.grassAmount);room_environment::GrassReactionInputs reaction{state.player.pos,state.phoneTransform.vacuumPullPoint,state.environmentVisual.latestShotOrigin,state.vacuum.power,state.environmentVisual.latestShotAge};glDisable(GL_LIGHTING);glBegin(GL_QUADS);for(int i=0;i<grassCount;++i){auto blade=room_environment::grassBlade(state.roomSeed,state.roomIndex,tileIndex,i);blade.root.z+=z0;const Vec3 tip=room_environment::grassTip(blade,state.time,reaction),side{std::cos(blade.phase)*blade.width*0.5f,0,std::sin(blade.phase)*blade.width*0.5f};const Vec3 rootL=blade.root-side,rootR=blade.root+side,tipL=tip-side*0.62f,tipR=tip+side*0.62f;gradedColor(VisualIdentity::GrassRoot.r,VisualIdentity::GrassRoot.g,VisualIdentity::GrassRoot.b);glVertex3f(rootL.x,rootL.y,rootL.z);glVertex3f(rootR.x,rootR.y,rootR.z);gradedColor(VisualIdentity::GrassTip.r,VisualIdentity::GrassTip.g,VisualIdentity::GrassTip.b);glVertex3f(tipR.x,tipR.y,tipR.z);glVertex3f(tipL.x,tipL.y,tipL.z);}glEnd();glEnable(GL_LIGHTING);}
}

void DesktopRenderer::applyCamera(const GameState& state, float aspect) {
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); perspective(state.camera.verticalFovDegrees, aspect, VisualIdentity::CameraNearPlane, VisualIdentity::CameraFarPlane);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); lookAt(state.camera.pos, state.camera.lookTarget, {0,1,0});
}

void DesktopRenderer::drawHud(const GameState& state) const {
    // GLFW reports the Retina backing framebuffer here, not macOS logical
    // points. Render HUD geometry on a bounded logical canvas so a 2x backing
    // scale does not make every label and meter appear half-sized. Keeping the
    // aspect ratio intact also makes the same rule useful at 1440p and 4K.
    const int framebufferWidth=width_,framebufferHeight=height_;
    const float hudScale=clampf(std::min(static_cast<float>(framebufferWidth)/1280.0f,static_cast<float>(framebufferHeight)/720.0f),1.0f,2.5f);
    struct RestoreFramebufferSize { int& width;int& height;int oldWidth;int oldHeight;~RestoreFramebufferSize(){width=oldWidth;height=oldHeight;} } restore{width_,height_,framebufferWidth,framebufferHeight};
    width_=std::max(1,static_cast<int>(std::lround(framebufferWidth/hudScale)));
    height_=std::max(1,static_cast<int>(std::lround(framebufferHeight/hudScale)));
    float overlayAlpha=1.0f;
    const auto quad=[&](float x,float y,float w,float h,float r,float g,float b,float a) {
        glColor4f(r,g,b,a*overlayAlpha);
        glBegin(GL_QUADS);
        glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
        glEnd();
    };
    const auto rotatedQuad=[&](float cx,float cy,float w,float h,float angle,float r,float g,float b,float a) {
        const float c=std::cos(angle),s=std::sin(angle),hx=w*0.5f,hy=h*0.5f;
        const Vec3 corners[4]={{-hx,-hy,0},{hx,-hy,0},{hx,hy,0},{-hx,hy,0}};
        glColor4f(r,g,b,a*overlayAlpha); glBegin(GL_QUADS);
        for(const Vec3& p:corners) glVertex2f(cx+p.x*c-p.y*s,cy+p.x*s+p.y*c);
        glEnd();
    };
    const auto text=[&](const std::string& value,float x,float y,float scale,float r=1.0f,float g=1.0f,float b=1.0f,float a=0.94f){
        float pen=x;for(char c:value){if(c==' '){pen+=6*scale;continue;}const auto rows=bitmapGlyph(c);for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1u<<(4-col))){const float px=pen+col*scale,py=y+row*scale;if(overlayAlpha<0.999f){quad(px-1,py,scale,scale,0,0,0,a);quad(px+1,py,scale,scale,0,0,0,a);quad(px,py-1,scale,scale,0,0,0,a);quad(px,py+1,scale,scale,0,0,0,a);}quad(px,py,scale,scale,r,g,b,a);}pen+=6*scale;}
    };
    const auto rainbow=[&](float hue){hue-=std::floor(hue);const float x=hue*6.0f,i=std::floor(x),f=x-i,q=1.0f-f;switch(static_cast<int>(i)%6){case 0:return Vec3{1,f,0};case 1:return Vec3{q,1,0};case 2:return Vec3{0,1,f};case 3:return Vec3{0,q,1};case 4:return Vec3{f,0,1};default:return Vec3{1,0,q};}};

    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0.0,static_cast<double>(width_),static_cast<double>(height_),0.0,-1.0,1.0);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    const float menuUiScale=clampf(std::min(static_cast<float>(width_)/1280.0f,static_cast<float>(height_)/720.0f),0.55f,1.8f);
    const float menuCanvasW=static_cast<float>(width_)/menuUiScale,menuCanvasH=static_cast<float>(height_)/menuUiScale;
    if(state.localSettings.fpsCounter){const std::string fps="FPS "+std::to_string(static_cast<int>(std::round(displayedFps)));text(fps,width_-fps.size()*7.2f-12,68,1.2f,0.72f,1.0f,0.90f);}
    if(state.attractMode){
        const float cx=width_*0.5f;
        const float exitLinear=state.cinematic.attractExitActive?clampf(state.cinematic.attractExitElapsed/0.62f,0.0f,1.0f):0.0f;
        const float exitEase=exitLinear*exitLinear*(3.0f-2.0f*exitLinear);
        quad(0,0,static_cast<float>(width_),static_cast<float>(height_),0.0f,0.0f,0.0f,0.10f+exitEase*0.90f);
        const float titleOpacity=1.0f-clampf((exitEase-0.68f)/0.32f,0.0f,1.0f);
        drawPaletteMenuTitle("DATA",cx,height_*(0.19f+exitEase*0.25f),(96.0f-exitEase*26.0f)*menuUiScale,state.time,titleOpacity);
        glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);return;
    }
    const bool pausedSolo=state.started&&state.uiPaused&&!state.multiplayer.enabled&&!state.upgradeMenu.active;
    if(state.dead){
        const float fade=clampf(state.cinematic.overlayFade,0.0f,1.0f);
        quad(0,0,static_cast<float>(width_),static_cast<float>(height_),0.0f,0.0f,0.0f,0.18f*fade);
        const float awaken=clampf(state.cinematic.restartAwaken,0.0f,1.0f);
        const float pulse=0.5f+0.5f*std::sin(state.time*1.5f);
        const float titleScale=2.6f+awaken*0.35f;
        const std::string again="Again?";
        const std::string quit="Quit";
        const float cx=width_*0.5f;
        const float cy=height_*0.56f;
        const auto drawDeathChoice=[&](const std::string& label,int choice,float y){
            const bool selected=state.cinematic.deathChoice==choice;
            const float scale=choice==0?titleScale:1.55f;
            const float tw=static_cast<float>(label.size())*6.0f*scale;
            const float alpha=(selected?0.98f:0.54f)*fade;
            if(selected){
                quad(cx-tw*0.5f-24.0f,y+3.5f*scale,7.0f,7.0f,VisualIdentity::ElectricCyan.r,VisualIdentity::ElectricCyan.g,VisualIdentity::ElectricCyan.b,(0.78f+0.16f*pulse)*fade);
            }
            text(label,cx-tw*0.5f,y,scale,selected?1.0f:0.70f,selected?1.0f:0.88f,1.0f,alpha);
        };
        drawDeathChoice(again,0,cy);
        drawDeathChoice(quit,1,cy+58.0f);
        glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);return;
    }
    if(state.cinematic.introActive||!state.started||pausedSolo){
        if(state.cinematic.menuEnterActive){
            const float linear=clampf(state.cinematic.menuEnterElapsed/0.48f,0.0f,1.0f);
            const float fade=1.0f-linear*linear*(3.0f-2.0f*linear);
            quad(0,0,static_cast<float>(width_),static_cast<float>(height_),0.0f,0.0f,0.0f,fade);
        }
        glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);return;
    }

    if(state.multiplayer.enabled){const std::string code=state.multiplayer.roomCode.data();const std::string net=state.multiplayer.status.data(),focus=code.empty()?net:code;const float w=std::max(92.0f,static_cast<float>(focus.size())*8.1f+18.0f),x=width_-w-12.0f;quad(x,12,w,30,0.005f,0.012f,0.016f,0.54f);text(focus,x+(w-focus.size()*7.2f)*0.5f,20,1.2f,0.66f,0.96f,1.0f);}
    if(state.camera.spectatedPlayerId>=0){
        const std::string label="SPECTATING  P"+std::to_string(state.camera.spectatedPlayerId+1);
        const float scale=1.35f,tw=label.size()*6.0f*scale,pw=tw+24.0f,px=(width_-pw)*0.5f;
        quad(px,18,pw,24,0.005f,0.012f,0.016f,0.62f);
        quad(px,18,pw,1.5f,VisualIdentity::ElectricCyan.r,VisualIdentity::ElectricCyan.g,VisualIdentity::ElectricCyan.b,0.82f);
        text(label,px+12.0f,25,scale,0.72f,0.96f,1.0f,0.94f);
        glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);return;
    }

    if(state.traversalLab){
        const float panelW=520.0f,panelX=(width_-panelW)*0.5f;
        quad(panelX,12,panelW,74,0.005f,0.012f,0.016f,0.72f);
        quad(panelX,12,panelW,1,VisualIdentity::ElectricCyan.r,VisualIdentity::ElectricCyan.g,VisualIdentity::ElectricCyan.b,0.78f);
        text("TRAVERSAL LAB",panelX+12,20,1.65f,0.82f,1.0f,0.94f);
        text("CENTER  GAPS 1.5  2.0  2.5",panelX+12,40,1.15f,1.0f,1.0f,1.0f);
        text("LEFT  LEDGES     RIGHT  ASCENT",panelX+12,55,1.05f,0.72f,0.94f,1.0f);
        text("SPACE JUMP   F LUNGE   SHIFT SPRINT",panelX+12,70,1.0f,0.82f,1.0f,0.88f);
    }
    if(state.roomInspector){
        const auto& r=state.roomInspectorReport;const float panelW=std::min(780.0f,static_cast<float>(width_)-24.0f),panelX=(width_-panelW)*0.5f;
        quad(panelX,12,panelW,132,0.005f,0.012f,0.016f,0.82f);quad(panelX,12,panelW,1,VisualIdentity::ElectricCyan.r,VisualIdentity::ElectricCyan.g,VisualIdentity::ElectricCyan.b,0.82f);
        text(std::string(room_environment::premiseName(r.premise))+"  "+room_environment::settingName(r.setting)+" / "+room_environment::formName(r.form)+" / "+room_environment::scaleName(r.scale)+" / "+room_environment::conditionName(r.condition)+" / INTENT "+room_environment::traversalIntentName(r.traversalIntent),panelX+12,20,1.30f,0.82f,1.0f,0.94f);
        text("SEED "+std::to_string(r.seed)+"  ROOM "+std::to_string(r.roomIndex)+"  ROUTE "+(r.requiredRouteValid?"VALID":"INVALID"),panelX+12,42,1.05f,1.0f,1.0f,1.0f);
        text("SURF "+std::to_string(r.traversalSurfaceCount)+"  EDGE "+std::to_string(r.traversalEdgeCount)+" (REQ "+std::to_string(r.requiredEdgeCount)+")  COLLIDER "+std::to_string(r.colliderCount)+"  PROP "+std::to_string(r.presentationPropCount),panelX+12,61,1.0f,0.72f,0.94f,1.0f);
        text("ROLE MASS "+std::to_string(r.environmentRoleCounts[static_cast<int>(room_environment::EnvironmentRole::Mass)])+"  LANDMARK "+std::to_string(r.environmentRoleCounts[static_cast<int>(room_environment::EnvironmentRole::Landmark)])+"  TRAVERSAL "+std::to_string(r.environmentRoleCounts[static_cast<int>(room_environment::EnvironmentRole::Traversal)])+"  DETAIL "+std::to_string(r.environmentRoleCounts[static_cast<int>(room_environment::EnvironmentRole::Detail)])+"  HUMAN "+std::to_string(gameplay::WORLD_SCALE.humanHeight).substr(0,4)+"  STORY "+std::to_string(gameplay::WORLD_SCALE.storyHeight).substr(0,4),panelX+12,80,0.88f,0.82f,0.94f,1.0f);
        text("ENEMY "+std::to_string(r.enemyCount)+" / "+std::to_string(r.enemyBudget)+"  TRANSPARENT "+std::to_string(r.transparentPrimitiveCount)+"  VISIBLE~ "+std::to_string(r.visiblePrimitiveEstimate)+"  DRAW "+(r.drawCallBucket==0?"LOW":(r.drawCallBucket==1?"MED":"HIGH")),panelX+12,79,1.0f,0.82f,1.0f,0.88f);
        text(std::string("[ ] PREMISE  R SEED  E ENEMIES ")+(state.roomInspectorEnemies?"ON":"OFF"),panelX+12,106,1.0f,1.0f,1.0f,1.0f);
        text("5 KEEP  6 TUNE  7 REDESIGN  8 REMOVE",panelX+12,118,1.0f,0.72f,1.0f,0.74f);
    }
    if((state.traversalLab||state.slopeLab||state.rallyLab||state.roomInspector)&&state.hud.buildLabel[0])
        text(state.hud.buildLabel.data(),12,336,1.0f,0.58f,0.92f,1.0f,0.82f);
    // The collision-authoritative head center owns a cycling data glyph, so
    // the aim cue cannot drift away from the actual critical volume.
    overlayAlpha=state.hud.critMarkerOpacity;
    {const Vec3 viewForward=normalized(state.camera.lookTarget-state.camera.pos),viewRight=normalized(cross3(viewForward,{0,1,0})),viewUp=cross3(viewRight,viewForward);const float tanHalf=std::tan(state.camera.verticalFovDegrees*PI/360.0f),aspect=static_cast<float>(width_)/std::max(1,height_);constexpr char glyphs[]="01ABCDEFHIKMNPRSTXYZ+-/:";for(int i=0;i<TARGET_COUNT;++i){const TargetState& target=state.targets[i];if(!target.alive||target.slurpable)continue;const float attackT=target.attackTimer>0?1-clampf(target.attackTimer/HUMAN_SWING_ATTACK_DURATION,0,1):-1.0f,attackBob=target.attackTimer>0?std::sin(attackT*PI)*0.035f*(target.attackVariant>=2?1.0f:0.0f):0;const Vec3 world{target.pos.x,(HUMAN_VISUAL_SPEC.totalHeight-HUMAN_VISUAL_SPEC.headRadius)*target.scale+attackBob,target.pos.z},delta=world-state.camera.pos;const float depth=dot3(delta,viewForward);if(depth<=0.18f||depth>16.0f)continue;const float nx=dot3(delta,viewRight)/(depth*tanHalf*aspect),ny=dot3(delta,viewUp)/(depth*tanHalf);if(std::abs(nx)>1.04f||std::abs(ny)>1.04f)continue;const float armorMax=target.brute?4.0f:2.0f,damage=1.0f-clampf(target.armor/armorMax,0,1);const bool perfectReady=attackT>=0.22f&&attackT<=0.46f;const Vec3 baseColor=rainbow(0.51f+damage*0.38f);const Vec3 magenta{VisualIdentity::ElectricMagenta.r,VisualIdentity::ElectricMagenta.g,VisualIdentity::ElectricMagenta.b};const Vec3 color=mix3(baseColor,magenta,clampf(state.hud.criticalHitPulse*0.75f+(perfectReady?0.18f:0.0f),0.0f,1.0f));const int cycle=(static_cast<int>(state.time*10.0f)+i*7+state.roomIndex*3)%static_cast<int>(sizeof(glyphs)-1);const float perspectiveScale=clampf(8.0f/depth,0.82f,1.55f),marker=(11.0f+damage*4.0f+(perfectReady?3.0f:0))*perspectiveScale,scale=(1.35f+damage*0.28f+(perfectReady?0.18f:0))*perspectiveScale,sx=(nx*0.5f+0.5f)*width_,sy=(0.5f-ny*0.5f)*height_,alpha=0.72f+damage*0.20f+(perfectReady?0.08f:0),spin=state.time*0.9f+i*0.37f;rotatedQuad(sx,sy,marker,marker,PI*0.25f+spin,color.x,color.y,color.z,0.10f+damage*0.08f);rotatedQuad(sx-marker,sy,marker*0.52f,2,spin*0.08f,color.x,color.y,color.z,alpha);rotatedQuad(sx+marker,sy,marker*0.52f,2,spin*0.08f,color.x,color.y,color.z,alpha);rotatedQuad(sx,sy-marker,2,marker*0.52f,spin*0.08f,color.x,color.y,color.z,alpha);rotatedQuad(sx,sy+marker,2,marker*0.52f,spin*0.08f,color.x,color.y,color.z,alpha);text(std::string(1,glyphs[cycle]),sx-2.5f*scale+1,sy-3.5f*scale+1,scale,0,0,0,alpha*0.85f);text(std::string(1,glyphs[cycle]),sx-2.5f*scale,sy-3.5f*scale,scale,color.x,color.y,color.z,alpha);}}
    overlayAlpha=1.0f;
    {const auto labelFor=[](int signal)->const char*{switch(signal){case 1:return "HELP";case 2:return "PING";case 3:return "GROUP";case 4:return "OK";default:return "";}};const auto colorFor=[](int signal)->VisualColor{switch(signal){case 1:return VisualIdentity::ElectricMagenta;case 2:return VisualIdentity::ElectricCyan;case 3:return VisualIdentity::AcidChartreuse;case 4:return VisualIdentity::WarmGold;default:return VisualIdentity::ElectricCyan;}};const Vec3 viewForward=normalized(state.camera.lookTarget-state.camera.pos),viewRight=normalized(cross3(viewForward,{0,1,0})),viewUp=cross3(viewRight,viewForward);const float tanHalf=std::tan(state.camera.verticalFovDegrees*PI/360.0f),aspect=static_cast<float>(width_)/std::max(1,height_);const auto drawSignal=[&](const PlayerState& player){if(player.commSignal<1||player.commSignal>4||player.commSignalTimer<=0.0f)return;const Vec3 world=player.pos+Vec3{0,1.05f,0},delta=world-state.camera.pos;const float depth=dot3(delta,viewForward);if(depth<=0.18f||depth>24.0f)return;const float nx=dot3(delta,viewRight)/(depth*tanHalf*aspect),ny=dot3(delta,viewUp)/(depth*tanHalf);if(std::abs(nx)>1.08f||std::abs(ny)>1.08f)return;const char* label=labelFor(player.commSignal);const VisualColor c=colorFor(player.commSignal);const float sx=(nx*0.5f+0.5f)*width_,sy=(0.5f-ny*0.5f)*height_,fade=clampf(player.commSignalTimer/0.35f,0.0f,1.0f),scale=clampf(9.0f/depth,1.15f,2.15f),tw=std::strlen(label)*6.0f*scale,pw=tw+18.0f*scale,ph=13.0f*scale,pulse=0.5f+0.5f*std::sin(state.time*8.0f);quad(sx-pw*0.5f,sy-ph*0.5f,pw,ph,VisualIdentity::DeepPlum.r*0.12f,VisualIdentity::DeepPlum.g*0.12f,VisualIdentity::DeepPlum.b*0.12f,0.52f*fade);quad(sx-pw*0.5f,sy-ph*0.5f,pw,1.4f*scale,c.r,c.g,c.b,(0.58f+0.18f*pulse)*fade);text(label,sx-tw*0.5f,sy-3.5f*scale,scale,c.r,c.g,c.b,0.96f*fade);};drawSignal(state.player);for(const auto& peer:state.multiplayer.peers)if(peer.active)drawSignal(peer.player);}
    if(std::max(state.hud.headshotPulse,state.hud.criticalHitPulse)>0.001f){const float charge=clampf(state.hud.headshotKillCharge,0,1),pulse=std::max(state.hud.headshotPulse,state.hud.criticalHitPulse),eased=pulse*pulse,w=static_cast<float>(width_),h=static_cast<float>(height_),breath=0.5f+0.5f*std::sin(state.time*2.4f),mist=12.0f+charge*9.0f+state.hud.perfectPulse*4.0f;const Vec3 magenta{VisualIdentity::ElectricMagenta.r,VisualIdentity::ElectricMagenta.g,VisualIdentity::ElectricMagenta.b};const Vec3 violet{0.58f,0.34f,0.92f};const Vec3 cyan{VisualIdentity::ElectricCyan.r,VisualIdentity::ElectricCyan.g,VisualIdentity::ElectricCyan.b};const Vec3 core=mix3(violet,magenta,0.72f),accent=mix3(cyan,magenta,0.46f);const float veil=eased*(0.035f+charge*0.070f),wisp=pulse*(0.075f+charge*0.105f),spark=pulse*(0.10f+charge*0.14f);quad(0,0,w,mist,core.x,core.y,core.z,veil);quad(0,h-mist,w,mist,accent.x,accent.y,accent.z,veil);quad(0,0,mist,h,accent.x,accent.y,accent.z,veil*0.90f);quad(w-mist,0,mist,h,core.x,core.y,core.z,veil*0.90f);for(int i=0;i<3;++i){const float phase=state.time*(0.55f+i*0.17f)+i*2.1f,drift=0.5f+0.5f*std::sin(phase),len=w*(0.22f+0.10f*i+0.08f*breath),thick=1.2f+i*1.1f+charge*1.4f,alpha=wisp*(0.72f-0.14f*i),x=clampf(drift*(w+len)-len,0.0f,w-len);const Vec3 c=i==1?accent:core;quad(x,2.0f+i*4.0f,len,thick,c.x,c.y,c.z,alpha);quad(w-x-len,h-3.0f-i*4.4f,len,thick,c.x,c.y,c.z,alpha*0.82f);const float y=clampf((0.5f+0.5f*std::sin(phase*0.81f+1.7f))*(h+len)-len,0.0f,h-len);quad(2.0f+i*4.0f,y,thick,len,c.x,c.y,c.z,alpha*0.70f);quad(w-3.0f-i*4.4f,h-y-len,thick,len,c.x,c.y,c.z,alpha*0.64f);}const float corner=clampf(std::min(w,h)*0.10f,34.0f,84.0f);quad(0,0,corner,2.0f,core.x,core.y,core.z,spark);quad(0,0,2.0f,corner,accent.x,accent.y,accent.z,spark*0.85f);quad(w-corner,h-2.0f,corner,2.0f,accent.x,accent.y,accent.z,spark*0.75f);quad(w-2.0f,h-corner,2.0f,corner,core.x,core.y,core.z,spark*0.65f);}

    if(state.hud.energyTicker[0]&&state.time<state.hud.energyTickerUntil){const std::string ticker=state.hud.energyTicker.data();const float scale=1.35f,tw=ticker.size()*6*scale,pw=std::max(118.0f,tw+16.0f),px=(width_-pw)*0.5f;const int type=state.hud.energyTickerType;const VisualColor tickerColor=type==1?VisualIdentity::Copper:(type==0?VisualIdentity::SignalGreen:VisualIdentity::ElectricCyan);quad(px,72,pw,18,0,0,0,0.54f);quad(px,72,pw,1,tickerColor.r,tickerColor.g,tickerColor.b,0.72f);text(ticker,(width_-tw)*0.5f,77,scale,tickerColor.r,tickerColor.g,tickerColor.b);}
    if(state.player.grabbedByTarget>=0){const std::string hint="WIGGLE  A  D";const float s=1.7f;text(hint,(width_-hint.size()*6*s)*0.5f,height_*0.69f,s,1.0f,0.82f,0.68f,0.94f);}
    if(state.player.downed){const std::string hint="SIGNAL DOWN  "+std::to_string(static_cast<int>(std::ceil(state.player.bleedoutTimer)));const float s=1.8f;text(hint,(width_-hint.size()*6*s)*0.5f,height_*0.55f,s,1.0f,0.48f,0.42f,0.96f);}
    if(state.player.inSecretRoom){const std::string hint=state.secretTv.broken?"NO SIGNAL":"SIGNAL "+std::to_string(state.secretTv.signal)+"   SHOOT TO DONATE";const float s=1.35f;text(hint,(width_-hint.size()*6*s)*0.5f,54,s,0.72f,0.94f,0.96f,0.88f);}

    // One persistent reticle rotor owns four independently translated arms.
    const float cx=width_*0.5f, cy=height_*0.5f;
    const float spread=state.hud.crosshairSpreadPixels,arm=14.0f,thick=3.0f;
    const float angle=state.hud.crosshairRotationDegrees*PI/180.0f;
    const bool joining=state.hud.shootJoinTimer>0.0f;
    const float rr=joining?1.0f:0.498f,rg=joining?1.0f:0.906f,rb=1.0f;
    const float reticleAlpha=0.98f*clampf(state.hud.crosshairOpacity,0.0f,1.0f);
    const auto rotateCenter=[&](float x,float y){return Vec3{cx+x*std::cos(angle)-y*std::sin(angle),cy+x*std::sin(angle)+y*std::cos(angle),0};};
    Vec3 center=rotateCenter(0,-spread); rotatedQuad(center.x,center.y,thick,arm,angle,rr,rg,rb,reticleAlpha);
    center=rotateCenter(0,spread); rotatedQuad(center.x,center.y,thick,arm,angle,rr,rg,rb,reticleAlpha);
    center=rotateCenter(-spread,0); rotatedQuad(center.x,center.y,arm,thick,angle,rr,rg,rb,reticleAlpha);
    center=rotateCenter(spread,0); rotatedQuad(center.x,center.y,arm,thick,angle,rr,rg,rb,reticleAlpha);

    if(state.upgradeMenu.active){
        quad(0,0,static_cast<float>(width_),static_cast<float>(height_),0.0f,0.012f,0.018f,0.54f);
        glPushMatrix();glScalef(menuUiScale,menuUiScale,1.0f);
        const float pw=std::min(680.0f,menuCanvasW-24.0f),ph=300.0f,px=(menuCanvasW-pw)*0.5f,py=(menuCanvasH-ph)*0.5f;
        const float arrival=clampf(state.upgradeMenu.presentationTime/0.42f,0.0f,1.0f);
        const float reveal=arrival*arrival*(3.0f-2.0f*arrival);
        quad(px,py,pw,ph,0.008f,0.022f,0.030f,0.70f*reveal);
        const float mosaicGap=2.0f,progressionCellW=(pw-36.0f-mosaicGap*24.0f)/25.0f;
        const int revealedCells=static_cast<int>(std::ceil(reveal*25.0f));
        for(int i=0;i<revealedCells;++i){const VisualColor c=VisualIdentity::DataMosaicPalette[i];quad(px+18.0f+i*(progressionCellW+mosaicGap),py+7.0f,progressionCellW,5.0f,c.r,c.g,c.b,0.36f+0.42f*reveal);}
        text("ROOM "+std::to_string(state.roomIndex)+" / RECOMPILE",px+18,py+20,1.55f,0.82f,0.97f,1.0f,0.96f*reveal);
        const bool remoteChoice=state.multiplayer.enabled&&!state.multiplayer.authoritativeHost;
        text(remoteChoice?"HOST HOLDS RULE AUTHORITY":"RUN MUTATION / SELECT ONE",px+18,py+46,1.08f,0.62f,0.88f,0.94f,0.88f*reveal);
        const float cellW=(pw-24.0f)/3.0f;const std::string labels[3]={"SHOT","LUNGE","ATTACK"};
        const auto choice=[&](int item,float top,float height){
            const int track=item%3;const bool permanent=item>=3,selected=state.hud.menuSelection==item;
            const int level=permanent?state.progression.permanent.levels[track]:state.progression.run.temporaryLevels[track];
            const bool available=!permanent||(state.progression.permanent.tokens>0&&level<5);
            const float response=selected?clampf(state.cinematic.textInteraction,0,1):0.0f;
            const float cx=px+12+track*cellW+(cellW-4)*0.5f,cy=py+top+height*0.5f;
            const float tilt=selected&&!permanent?std::sin(state.upgradeMenu.presentationTime*2.5f+item*1.7f)*0.018f:0.0f;
            const VisualColor focus=permanent?VisualIdentity::AcidChartreuse:VisualIdentity::ElectricCyan;
            const VisualColor dim=VisualIdentity::MetallicTeal;
            const VisualColor color=available?focus:dim;
            rotatedQuad(cx,cy,cellW-6,height,tilt,color.r,color.g,color.b,(selected?0.15f+response*0.05f:0.035f)*reveal);
            rotatedQuad(cx,cy+height*0.5f-1,cellW-22,2,tilt,color.r,color.g,color.b,(selected?0.82f:0.24f)*reveal);
            const std::string& label=labels[track];const float labelScale=1.75f+response*0.06f;
            text(label,cx-label.size()*6*labelScale*0.5f,cy-18.0f,labelScale,selected?0.94f:0.72f,selected?1.0f:0.88f,1.0f,(selected?0.98f:0.72f)*reveal);
            const std::string detail=permanent?("PERM "+std::to_string(level)+"/5"):("RUN "+std::to_string(level)+" / NEXT "+std::to_string(std::min(12,level+1)));
            const float detailScale=0.92f;
            text(detail,cx-detail.size()*6*detailScale*0.5f,cy+12.0f,detailScale,color.r,color.g,color.b,(available?0.88f:0.48f)*reveal);
        };
        for(int i=0;i<3;++i)choice(i,66,76);
        text("PERMANENT MEMORY / TOKENS "+std::to_string(state.progression.permanent.tokens),px+18,py+158,1.08f,VisualIdentity::AcidChartreuse.r,VisualIdentity::AcidChartreuse.g,VisualIdentity::AcidChartreuse.b,0.86f*reveal);
        for(int i=3;i<6;++i)choice(i,184,66);
        text(remoteChoice?"WAITING FOR HOST":"ARROWS TUNE   ENTER / A LOCK",px+18,py+276,0.94f,0.58f,0.82f,0.88f,0.72f*reveal);
    } else if(state.uiPaused&&state.multiplayer.enabled){
        glPushMatrix();glScalef(menuUiScale,menuUiScale,1.0f);
        const float pw=360.0f,ph=116.0f,px=menuCanvasW-pw-12.0f,py=48.0f;
        quad(px,py,pw,ph,0.01f,0.03f,0.04f,0.16f);quad(px,py,pw,1,1,1,1,0.55f);quad(px,py+ph-1,pw,1,1,1,1,0.40f);quad(px,py,1,ph,1,1,1,0.42f);quad(px+pw-1,py,1,ph,1,1,1,0.42f);
        text("PAUSED",px+12,py+12,2.0f);const float pauseTilt=std::sin(state.time*2.4f)*0.018f,pulse=clampf(state.cinematic.textInteraction,0,1),buttonScale=2.45f+pulse*0.06f;rotatedQuad(px+pw*0.5f,py+63,pw-24,58,pauseTilt,0.16f,0.86f,1.0f,0.18f);text("RESUME",px+pw*0.5f-6*6*buttonScale*0.5f,py+54-3.5f*buttonScale,buttonScale,0.92f,1.0f,1.0f);
    }
    if(state.upgradeMenu.active||state.uiPaused)glPopMatrix();

    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
}

void DesktopRenderer::drawDoorDataMosh(const GameState& state) const {
    if(!datamoshTexture_)glGenTextures(1,&datamoshTexture_);
    glBindTexture(GL_TEXTURE_2D,datamoshTexture_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    if(!state.doorTransition.active||state.doorTransition.progress<=0.018f){
        if(!datamoshFrameReady_||datamoshWidth_!=width_||datamoshHeight_!=height_){glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,width_,height_,0);datamoshWidth_=width_;datamoshHeight_=height_;datamoshFrameReady_=true;}
        else glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,width_,height_);
        return;
    }
    if(!datamoshFrameReady_)return;
    const float strength=clampf(state.doorTransition.progress,0,1),alpha=clampf(0.34f+strength*0.56f,0.34f,0.90f);
    const float mvX=clampf(state.doorTransition.frameMotion.x*width_*0.045f,-20.0f,20.0f)*(0.42f+strength*0.92f);
    const float mvY=clampf(-state.doorTransition.frameMotion.z*height_*0.030f,-24.0f,24.0f)*(0.42f+strength*0.92f);
    glDisable(GL_LIGHTING);glDisable(GL_DEPTH_TEST);glEnable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width_,0,height_,-1,1);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    for(int pass=5;pass>=1;--pass){const float k=pass/5.0f;glColor4f(1,1,1,(0.07f+0.09f*k)*strength);glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(mvX*k,mvY*k);glTexCoord2f(1,0);glVertex2f(width_+mvX*k,mvY*k);glTexCoord2f(1,1);glVertex2f(width_+mvX*k,height_+mvY*k);glTexCoord2f(0,1);glVertex2f(mvX*k,height_+mvY*k);glEnd();}
    if(state.localSettings.particles){
        // The captured room remains whole at the threshold,
        // then its sampled cells peel through stable radial/depth trajectories.
        constexpr int columns=48,rows=27;
        const float release=1.0f-strength;
        const float eased=1.0f-(1.0f-release)*(1.0f-release);
        const float cellW=static_cast<float>(width_)/columns,cellH=static_cast<float>(height_)/rows;
        const auto noise=[](unsigned int value){value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;return static_cast<float>(value&0xffffu)/65535.0f;};
        glBegin(GL_QUADS);
        for(int row=0;row<rows;++row)for(int column=0;column<columns;++column){
            const unsigned int id=static_cast<unsigned int>(row*columns+column+1);
            const float x0=column*cellW,y0=row*cellH;
            const float centerX=x0+cellW*0.5f-width_*0.5f,centerY=y0+cellH*0.5f-height_*0.5f;
            const float invLength=1.0f/std::max(1.0f,std::sqrt(centerX*centerX+centerY*centerY));
            const float angle=(noise(id*17u)-0.5f)*1.1f;
            const float radialX=centerX*invLength,radialY=centerY*invLength;
            const float tangentX=-radialY,tangentY=radialX;
            const float travel=(42.0f+noise(id*31u)*150.0f)*eased;
            const float depth=(noise(id*47u)-0.5f)*0.72f;
            const float scale=clampf(1.0f+depth*eased,0.52f,1.42f);
            const float dx=(radialX*std::cos(angle)+tangentX*std::sin(angle))*travel+mvX*(0.35f+noise(id*61u));
            const float dy=(radialY*std::cos(angle)+tangentY*std::sin(angle))*travel+mvY*(0.35f+noise(id*73u));
            const float halfW=cellW*0.52f*scale,halfH=cellH*0.52f*scale,cx=x0+cellW*0.5f+dx,cy=y0+cellH*0.5f+dy;
            const float u0=static_cast<float>(column)/columns,u1=static_cast<float>(column+1)/columns;
            const float v0=static_cast<float>(row)/rows,v1=static_cast<float>(row+1)/rows;
            glColor4f(1,1,1,strength*(0.72f+0.24f*noise(id*89u)));
            glTexCoord2f(u0,v0);glVertex2f(cx-halfW,cy-halfH);glTexCoord2f(u1,v0);glVertex2f(cx+halfW,cy-halfH);glTexCoord2f(u1,v1);glVertex2f(cx+halfW,cy+halfH);glTexCoord2f(u0,v1);glVertex2f(cx-halfW,cy+halfH);
        }
        glEnd();
    }else{
        glColor4f(1,1,1,alpha);glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(mvX,mvY);glTexCoord2f(1,0);glVertex2f(width_+mvX,mvY);glTexCoord2f(1,1);glVertex2f(width_+mvX,height_+mvY);glTexCoord2f(0,1);glVertex2f(mvX,height_+mvY);glEnd();
    }
    glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_BLEND);glDisable(GL_TEXTURE_2D);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);
}

void DesktopRenderer::draw(const GameState& state,const DeveloperCodecState* codec) const {
    ++fpsFrames;const auto now=std::chrono::steady_clock::now();const float elapsed=std::chrono::duration<float>(now-fpsWindowStart).count();if(elapsed>=0.5f){displayedFps=fpsFrames/elapsed;fpsFrames=0;fpsWindowStart=now;}
    const auto atmosphere=resolvedAtmosphere(state);
    const auto roomPlan=room_environment::roomPlan(state.roomSeed,state.roomIndex);
    const auto lightRig=room_lighting::roomLightRig(roomPlan.setting,roomPlan.form);
    const float objectiveProgress=state.requiredSouls>0?static_cast<float>(state.depositedSouls)/static_cast<float>(state.requiredSouls):0.0f;
    const auto lightingResponse=scene_lighting_response::resolve({state.vacuum.power,state.energy.dischargePositionAmount,state.environmentVisual.latestShotAge,state.hud.criticalHitPulse,objectiveProgress,state.roomClear});
    glClearColor(atmosphere.background.r,atmosphere.background.g,atmosphere.background.b,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    applyCamera(state, static_cast<float>(width_)/static_cast<float>(height_));
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT2); glEnable(GL_COLOR_MATERIAL);
    const auto& lighting=render_contract::DesktopSceneLighting;
    const GLfloat ambient[]={atmosphere.ambient.r,atmosphere.ambient.g,atmosphere.ambient.b,1.0f}; glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);
    const GLfloat sunDiffuse[]={atmosphere.sun.r,atmosphere.sun.g,atmosphere.sun.b,1.0f};
    const GLfloat sunPos[]={
        lighting.sun.direction.x,
        lighting.sun.direction.y,
        lighting.sun.direction.z,
        0.0f
    };
    const bool localPrimary=lightRig.primarySource==room_lighting::PrimaryLightSource::CeilingFixtures;
    if(localPrimary){glDisable(GL_LIGHT0);glDisable(GL_LIGHT1);}else{glEnable(GL_LIGHT0);glEnable(GL_LIGHT1);glLightfv(GL_LIGHT0,GL_DIFFUSE,sunDiffuse);glLightfv(GL_LIGHT0,GL_POSITION,sunPos);}
    const GLfloat fillDiffuse[]={atmosphere.fill.r,atmosphere.fill.g,atmosphere.fill.b,1.0f}, fillPos[]={lighting.fill.direction.x,lighting.fill.direction.y,lighting.fill.direction.z,0.0f};
    if(!localPrimary){glLightfv(GL_LIGHT1,GL_DIFFUSE,fillDiffuse); glLightfv(GL_LIGHT1,GL_POSITION,fillPos);}
    const GLfloat phoneDiffuse[]={atmosphere.phone.r*lightingResponse.phoneLightScale+lightingResponse.actionLight*0.08f+lightingResponse.criticalLight*0.28f,atmosphere.phone.g*lightingResponse.phoneLightScale+lightingResponse.actionLight*0.18f+lightingResponse.criticalLight*0.08f,atmosphere.phone.b*lightingResponse.phoneLightScale+lightingResponse.actionLight*0.24f+lightingResponse.criticalLight*0.24f,1.0f};
    const GLfloat phoneLightPos[]={state.phoneTransform.screenCenter.x,state.phoneTransform.screenCenter.y,state.phoneTransform.screenCenter.z,1.0f};
    glLightfv(GL_LIGHT2,GL_DIFFUSE,phoneDiffuse);glLightfv(GL_LIGHT2,GL_POSITION,phoneLightPos);glLightf(GL_LIGHT2,GL_CONSTANT_ATTENUATION,1.0f);glLightf(GL_LIGHT2,GL_LINEAR_ATTENUATION,1.6f);
    const float lightTileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(int i=0;i<3;++i){const GLenum light=GL_LIGHT3+i;if(i>=lightRig.localLightCount){glDisable(light);continue;}const auto& local=lightRig.localLights[i];const GLfloat diffuse[]={local.color.r*local.intensity,local.color.g*local.intensity,local.color.b*local.intensity,1.0f};const GLfloat position[]={local.localPosition.x,local.localPosition.y,lightTileOrigin+local.localPosition.z,1.0f};glEnable(light);glLightfv(light,GL_DIFFUSE,diffuse);glLightfv(light,GL_POSITION,position);glLightf(light,GL_CONSTANT_ATTENUATION,0.65f);glLightf(light,GL_LINEAR_ATTENUATION,0.05f);glLightf(light,GL_QUADRATIC_ATTENUATION,4.0f/(local.radius*local.radius));}
    if(lightingResponse.shotLight>0.001f){const GLfloat shotDiffuse[]={1.15f*lightingResponse.shotLight,0.82f*lightingResponse.shotLight,0.55f*lightingResponse.shotLight,1.0f},shotPosition[]={state.environmentVisual.latestShotOrigin.x,state.environmentVisual.latestShotOrigin.y+0.2f,state.environmentVisual.latestShotOrigin.z,1.0f};glEnable(GL_LIGHT6);glLightfv(GL_LIGHT6,GL_DIFFUSE,shotDiffuse);glLightfv(GL_LIGHT6,GL_POSITION,shotPosition);glLightf(GL_LIGHT6,GL_CONSTANT_ATTENUATION,0.72f);glLightf(GL_LIGHT6,GL_LINEAR_ATTENUATION,0.22f);glLightf(GL_LIGHT6,GL_QUADRATIC_ATTENUATION,0.08f);}else glDisable(GL_LIGHT6);
    if(lightingResponse.exitGlow>0.01f){const GLfloat exitDiffuse[]={0.34f*lightingResponse.exitGlow,0.72f*lightingResponse.exitGlow,0.68f*lightingResponse.exitGlow,1.0f},exitPosition[]={0.0f,2.2f,lightTileOrigin-ROOM_DEPTH*0.5f+0.8f,1.0f};glEnable(GL_LIGHT7);glLightfv(GL_LIGHT7,GL_DIFFUSE,exitDiffuse);glLightfv(GL_LIGHT7,GL_POSITION,exitPosition);glLightf(GL_LIGHT7,GL_CONSTANT_ATTENUATION,0.8f);glLightf(GL_LIGHT7,GL_LINEAR_ATTENUATION,0.11f);glLightf(GL_LIGHT7,GL_QUADRATIC_ATTENUATION,0.035f);}else glDisable(GL_LIGHT7);
    glEnable(GL_FOG);const GLfloat fogColor[]={atmosphere.fog.r,atmosphere.fog.g,atmosphere.fog.b,1.0f};glFogfv(GL_FOG_COLOR,fogColor);glFogi(GL_FOG_MODE,GL_EXP2);glFogf(GL_FOG_DENSITY,atmosphere.fogDensity);
    glEnable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_LIGHTING); glEnable(GL_NORMALIZE);
    const bool cheapVisuals=state.localSettings.graphicsPreset<=0;
    const auto actorVisible=[&](const Vec3& position){const Vec3 delta=position-state.camera.pos;const float maxDist=cheapVisuals?38.0f:55.0f;return lengthSq(delta)<maxDist*maxDist&&dot3(delta,state.camera.forward)>-8.0f;};
    for(int tile=state.topology.currentTileIndex-ROOM_VISUAL_HORIZON;tile<=state.topology.currentTileIndex+ROOM_VISUAL_HORIZON;++tile)drawRoomTile(state,tile,lightingResponse);

    // The secret room is deliberately disconnected from the repeating corridor:
    // a tiny, cheap collection of boxes makes it feel like found backstage space.
    if(state.secretTv.available){
        const float knock=clampf(state.secretTv.knockPulse,0.0f,1.0f);
        const float breathe=0.04f+0.035f*std::sin(state.time*2.1f);
        const float push=knock*(0.10f+0.018f*std::sin(state.time*41.0f));
        const float alpha=(state.secretTv.broken?0.12f:0.25f)+knock*0.28f;
        const Vec3 wallCenter=state.secretTv.entrancePos+Vec3{0.0f,1.22f,0.0f};
        const Vec3 pushedCenter=wallCenter+state.secretTv.entranceNormal*push;
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
        drawBox(pushedCenter,{0.08f+push*0.42f,2.30f+breathe+knock*0.10f,2.20f+breathe+knock*0.08f},0,0,0,VisualIdentity::TvMembrane.r,VisualIdentity::TvMembrane.g,VisualIdentity::TvMembrane.b,alpha);
        if(knock>0.025f)drawBox(pushedCenter+state.secretTv.entranceNormal*(0.07f+push*0.5f),{0.03f,1.82f+knock*0.24f,1.72f+knock*0.20f},0,0,0,VisualIdentity::ElectricCyan.r,VisualIdentity::ElectricCyan.g,VisualIdentity::ElectricCyan.b,0.18f*knock);
        glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    }
    if(state.player.inSecretRoom){
        drawBox({40.4f,-0.04f,0},{7.4f,0.08f,6.4f},0,0,0,VisualIdentity::SecretFloor.r,VisualIdentity::SecretFloor.g,VisualIdentity::SecretFloor.b);
        drawBox({36.7f,2.4f,0},{0.12f,4.8f,6.4f},0,0,0,VisualIdentity::SecretWall.r,VisualIdentity::SecretWall.g,VisualIdentity::SecretWall.b);
        drawBox({40.4f,2.4f,-3.2f},{7.4f,4.8f,0.12f},0,0,0,VisualIdentity::SecretWall.r,VisualIdentity::SecretWall.g,VisualIdentity::SecretWall.b);
        drawBox({40.4f,2.4f,3.2f},{7.4f,4.8f,0.12f},0,0,0,VisualIdentity::SecretWall.r,VisualIdentity::SecretWall.g,VisualIdentity::SecretWall.b);
        drawBox({42.25f,0.70f,0},{1.75f,1.35f,0.82f},0,-1.5708f,0,VisualIdentity::SecretBlack.r,VisualIdentity::SecretBlack.g,VisualIdentity::SecretBlack.b);
        float phoneProximity=0.0f;const Vec3 tvPosition{41.82f,0.78f,0};
        const auto includePhone=[&](const PlayerState& player,bool active){if(!active||!player.inSecretRoom)return;const float distance=length(player.pos-tvPosition);phoneProximity=std::max(phoneProximity,1.0f-clampf(distance/6.0f,0.0f,1.0f));};
        includePhone(state.player,true);if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)includePhone(peer.player,peer.active);
        drawSecretTvScreen(state,phoneProximity);
        drawBox({41.35f,0.18f,-0.80f},{1.8f,0.055f,0.055f},0,0.18f,0,VisualIdentity::SecretCable.r,VisualIdentity::SecretCable.g,VisualIdentity::SecretCable.b);
        drawBox({41.45f,0.16f,0.76f},{2.1f,0.045f,0.045f},0,-0.22f,0,VisualIdentity::SecretCable.r,VisualIdentity::SecretCable.g,VisualIdentity::SecretCable.b);
    }

    // A pause menu overlays the live world, so its frozen actors still own their
    // shadows. Only presentations that replace gameplay suppress world shadows.
    const bool menuPresentation=(((!state.started&&!state.dead)||state.dead||state.cinematic.introActive)&&!state.multiplayer.enabled&&!state.upgradeMenu.active);
    const auto shadowQuality=render_contract::shadowQualityFor(state.localSettings.graphicsPreset,state.localSettings.shadows,true);
    if(shadowQuality==render_contract::ShadowQuality::Cheap){
    glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
    if(!menuPresentation&&!state.camera.firstPerson&&state.player.alive)drawGroundShadow(state.phoneTransform.position,0.25f,0.18f,0.95f,0.20f);
    if(!menuPresentation&&state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.playerId!=state.multiplayer.localPlayerId&&peer.player.alive)drawGroundShadow(peer.player.pos,0.25f,0.18f,0.95f,0.20f);
    if(!menuPresentation){
    const float shadowTileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(int offset=-1;offset<=1;++offset)for(auto target:state.targets)if(target.alive){target.pos.z=shadowTileOrigin+static_cast<float>(offset)*ROOM_DEPTH+(target.pos.z-std::floor((target.pos.z+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);if(!actorVisible(target.pos))continue;if(!target.slurpable){drawGroundShadow(target.pos,0.30f*target.scale,0.22f*target.scale,1.1f*target.scale,0.13f);drawGroundShadow(target.pos,0.19f*target.scale,0.12f*target.scale,0.08f*target.scale,0.16f+target.visualReaction.commitment*0.05f);}else if(target.soulVisual.visible&&target.soulCubeAmount>0.001f)drawGroundShadow(target.pos,0.26f*target.scale,0.26f*target.scale,0.72f*target.scale,0.16f);}
    for(const auto& flower:state.flowers)if(flower.active)drawGroundShadow({flower.pos.x,flower.pos.y,flower.pos.z+shadowTileOrigin},0.27f,0.27f,0.72f,0.16f);
    for(const auto& bullet:state.bullets)if(bullet.alive){const float radius=0.40f*(bullet.brute?1.7f:1.0f);drawGroundShadow(bullet.pos,radius,radius,radius*2.0f,0.14f);}
    // Static room geometry already provides the receiving floor and its own
    // lighting. Projecting every collider (including room-scale walls) onto the
    // floor created large overlapping black sheets unrelated to visible casters.
    }
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_LIGHTING);}
    else if(shadowQuality==render_contract::ShadowQuality::Directional&&!menuPresentation){
        const Vec3& sun=render_contract::DesktopSceneLighting.sun.direction;
        const float shadowMatrix[16]={1,0,0,0,-sun.x/sun.y,0,-sun.z/sun.y,0,0,0,1,0,0.006f,0.012f,0.005f,1};
        glStencilMask(0xff);glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);glEnable(GL_STENCIL_TEST);glStencilFunc(GL_ALWAYS,1,0xff);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
        glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glDepthMask(GL_FALSE);glDisable(GL_DEPTH_TEST);glPushMatrix();glMultMatrixf(shadowMatrix);
        if(!state.camera.firstPerson&&state.player.alive){if(phoneShadowList_)drawStaticModel(phoneShadowList_,state.phoneTransform.position,state.phoneVisual.bodyScale,state.phoneTransform.orientation);else drawBox(state.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},state.phoneTransform.orientation,0,0,0);}
        if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.playerId!=state.multiplayer.localPlayerId&&peer.player.alive){if(phoneShadowList_)drawStaticModel(phoneShadowList_,peer.phoneTransform.position,peer.phoneVisual.bodyScale,peer.phoneTransform.orientation);else drawBox(peer.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},peer.phoneTransform.orientation,0,0,0);}
        const float shadowTileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
        const auto roomSetting=room_environment::roomPlan(state.roomSeed,state.roomIndex).setting;
        for(int offset=-1;offset<=1;++offset)for(auto target:state.targets)if(target.alive){target.pos.z=shadowTileOrigin+static_cast<float>(offset)*ROOM_DEPTH+(target.pos.z-std::floor((target.pos.z+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);if(!actorVisible(target.pos))continue;if(!target.slurpable){if(humanModel_.valid())drawHumanModel(target,state.time,roomSetting,true);else drawProceduralHumanDesktop(target,state.time,0,0,0);}else if(target.soulVisual.visible&&target.soulCubeAmount>0.001f){const auto& sv=target.soulVisual;const float cube=0.72f*0.78f*target.scale*sv.morphScale;drawBox(target.pos+Vec3{0,0.57f+sv.verticalOffset,0},{cube*sv.scale.x,cube*sv.scale.y,cube*sv.scale.z},0,sv.rotationY,0,0,0,0);}}
        for(const auto& flower:state.flowers)if(flower.active)drawBox({flower.pos.x,flower.pos.y,flower.pos.z+shadowTileOrigin},{0.54f,0.22f,0.54f},0,flower.rotationY,0,0,0,0);
        for(const auto& bullet:state.bullets)if(bullet.alive){const float size=0.72f*1.12f*(bullet.brute?1.7f:1.0f);drawBox(bullet.pos,{size,size,size},bullet.spin*1.2f,bullet.spin*1.7f,bullet.spin*0.9f,0,0,0);}
        glPopMatrix();glEnable(GL_DEPTH_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glStencilMask(0);glStencilFunc(GL_EQUAL,1,0xff);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
        glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);drawBox({0,0.014f,shadowTileOrigin},{ROOM_WIDTH,0.008f,ROOM_DEPTH*3.0f},0,0,0,0.012f,0.018f,0.022f,0.24f);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_LIGHTING);glDisable(GL_STENCIL_TEST);
    }

    if (state.phoneVisual.visible) {
        const Vec3 phonePos=state.phoneTransform.position;
        const auto& pv=state.phoneVisual;
        const Quat phoneOrientation=state.phoneTransform.orientation;
        if(phoneModelList_) drawStaticModel(phoneModelList_,phonePos,pv.bodyScale,phoneOrientation);
        else drawBox(phonePos,{PHONE_BODY_WIDTH*pv.bodyScale.x,PHONE_BODY_HEIGHT*pv.bodyScale.y,PHONE_BODY_DEPTH},phoneOrientation,VisualIdentity::PhoneBody.r,VisualIdentity::PhoneBody.g,VisualIdentity::PhoneBody.b);
        const float glow=std::min(1.0f,0.45f+pv.screenGlow*0.36f);
        drawBox(state.phoneTransform.screenCenter,{PHONE_SCREEN_WIDTH*pv.screenScale.x,PHONE_SCREEN_HEIGHT*pv.screenScale.y,PHONE_SCREEN_DEPTH},phoneOrientation,VisualIdentity::PhoneEmission.r*glow,VisualIdentity::PhoneEmission.g*glow,VisualIdentity::PhoneEmission.b*glow);
        if(state.phoneDisplay.mode!=PhoneDisplayMode::Off&&state.phoneDisplay.mode!=PhoneDisplayMode::Death)
            drawPhoneDisplayTexture(state);
    }
    if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.playerId!=state.multiplayer.localPlayerId&&peer.player.alive){const auto& pv=peer.phoneVisual;if(phoneModelList_)drawStaticModel(phoneModelList_,peer.phoneTransform.position,pv.bodyScale,peer.phoneTransform.orientation);else drawBox(peer.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},peer.phoneTransform.orientation,0.32f,0.86f,1.0f);drawBox(peer.phoneTransform.screenCenter,{PHONE_SCREEN_WIDTH,PHONE_SCREEN_HEIGHT,PHONE_SCREEN_DEPTH},peer.phoneTransform.orientation,0.05f,0.55f,0.78f);}

    const MeleeVisualState& melee=state.meleeVisual;
    if(melee.visualTimer>0.0f && !melee.locomotionLunge){
        const float t=1.0f-clampf(melee.visualTimer/std::max(0.001f,melee.visualDuration),0.0f,1.0f);
        const float fade=1.0f-t, hitBoost=melee.visualHit?1.25f:0.72f;
        glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        const Quat slashQ=quatAxisAngle({0,1,0},state.player.yaw)*quatAxisAngle({1,0,0},PI*0.5f)*quatAxisAngle({0,0,1},-PI*0.28f+t*PI*1.15f);
        fxRibbon(melee.origin+melee.direction*(0.66f+0.22f*t),slashQ,{(0.75f+t*0.72f)*hitBoost,(0.75f+t*0.72f)*hitBoost,1},0,PI*1.35f,24,0.494f,0.546f,VisualIdentity::ElectricMagenta.r,VisualIdentity::ElectricMagenta.g,VisualIdentity::ElectricMagenta.b,fade*0.66f);
        const Vec3 delta=melee.impact-melee.origin; const float len=std::max(0.3f,length(delta));
        const Vec3 mid=melee.origin+delta*0.46f; const float yaw=std::atan2(delta.x,delta.z); const float pitch=-std::asin(clampf(delta.y/std::max(len,0.001f),-1.0f,1.0f));
        const Quat streakQ=quatAxisAngle({0,1,0},yaw)*quatAxisAngle({1,0,0},pitch);
        fxStreak(mid,streakQ,len*(0.70f+std::sin(t*PI)*0.18f),0.11f*(1+std::sin(t*PI)*1.2f),VisualIdentity::ElectricCyan.r,VisualIdentity::ElectricCyan.g,VisualIdentity::ElectricCyan.b,fade*0.44f);
        fxRibbon(melee.impact,{}, {(0.45f+t*1.45f)*hitBoost,(0.45f+t*1.45f)*hitBoost,1},0,PI*2,24,0.318f,0.362f,VisualIdentity::AcidChartreuse.r,VisualIdentity::AcidChartreuse.g,VisualIdentity::AcidChartreuse.b,melee.visualHit?fade*0.82f:fade*0.24f);
        glDisable(GL_BLEND); glEnable(GL_LIGHTING);
    }

    struct TranslucentSoulDraw { Vec3 center; Vec3 scale; float rotationY; VisualColor color; float opacity; float distanceSquared; };
    std::array<TranslucentSoulDraw,TARGET_COUNT*3> translucentSouls{};
    int translucentSoulCount=0;
    const float tileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(int offset=-1;offset<=1;++offset)for (auto target:state.targets) if (target.alive) {
        Vec3 p=target.pos; p.z=tileOrigin+static_cast<float>(offset)*ROOM_DEPTH+(target.pos.z-std::floor((target.pos.z+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);
        if(!actorVisible(p))continue;
        const float mirrorShift=p.z-target.pos.z;
        target.tetherAnchor.z+=mirrorShift;target.tetherDestination.z+=mirrorShift;target.latchPoint.z+=mirrorShift;
        target.pos = p;
        if (!target.slurpable) {
            const auto setting=room_environment::roomPlan(state.roomSeed,state.roomIndex).setting;const VisualColor damageColor=humanDamageSurfaceColor(VisualIdentity::NormalEnemy,setting,target.armor,target.brute?4.0f:2.0f,target.slurpable,target.hitFlash);
            if(humanModel_.valid())drawHumanModel(target,state.time,setting);else drawProceduralHumanDesktop(target,state.time,damageColor.r,damageColor.g,damageColor.b);
        }
        if (target.slurpable && target.soulCubeAmount > 0.001f) {
            const auto& sv=target.soulVisual;
            if (!sv.visible) continue;
            const Vec3 soulCenter=p+Vec3{0,0.57f+sv.verticalOffset,0};
            drawSoulFlesh(target,soulCenter);
            const float cube=0.72f*0.78f*target.scale*sv.morphScale;
            const Vec3 delta=soulCenter-state.camera.pos;
            translucentSouls[translucentSoulCount++]={soulCenter,{cube*sv.scale.x,cube*sv.scale.y,cube*sv.scale.z},sv.rotationY,sv.color,sv.shellOpacity,delta.x*delta.x+delta.y*delta.y+delta.z*delta.z};
        }
    }
    std::sort(translucentSouls.begin(),translucentSouls.begin()+translucentSoulCount,[](const auto& a,const auto& b){return a.distanceSquared>b.distanceSquared;});
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
    // Draw one camera-facing surface of each convex shell. With culling
    // disabled, several cube faces compound alpha and produce false opacity.
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    for(int i=0;i<translucentSoulCount;++i){const auto& soul=translucentSouls[i];drawBox(soul.center,soul.scale,0,soul.rotationY,0,soul.color.r,soul.color.g,soul.color.b,soul.opacity);}
    glDisable(GL_CULL_FACE); glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    for(int offset=-ROOM_VISUAL_HORIZON;offset<=ROOM_VISUAL_HORIZON;++offset)for (int captureIndex=0;captureIndex<state.requiredSouls;++captureIndex) {
        const auto& capture=state.captures[captureIndex];
        Vec3 p=capture.pos; p.z+=tileOrigin+static_cast<float>(offset)*ROOM_DEPTH;
        drawBox(p+Vec3{0,0,-0.04f},{0.72f,0.72f,0.06f},0,0,0,VisualIdentity::MetallicTeal.r*0.74f,VisualIdentity::MetallicTeal.g*0.74f,VisualIdentity::MetallicTeal.b*0.74f);
        drawBox(p,{0.52f,0.52f,0.08f},0,0,0,0.02f,0.03f,0.04f);
        if(capture.filled) drawBox(p+Vec3{0,0,0.12f},{0.36f,0.36f,0.36f},state.time*1.5f,state.time*2.0f,state.time,VisualIdentity::SoulBase.r,VisualIdentity::SoulBase.g,VisualIdentity::SoulBase.b);
    }
    for(const auto& flower:state.flowers) if(flower.active){
        for(int offset=-ROOM_VISUAL_HORIZON;offset<=ROOM_VISUAL_HORIZON;++offset){
            const Vec3 center{flower.pos.x,flower.pos.y,flower.pos.z+static_cast<float>(state.topology.currentTileIndex+offset)*ROOM_DEPTH};
            constexpr char flowerGlyphs[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            const unsigned int phase=static_cast<unsigned int>(state.time*3.0f),positionHash=static_cast<unsigned int>(std::abs(flower.pos.x)*97.0f+std::abs(flower.pos.z)*193.0f)+static_cast<unsigned int>(state.roomIndex)*17u;
            unsigned int symbolHash=(phase+positionHash)*1664525u+1013904223u;
            const auto rows=bitmapGlyph(flowerGlyphs[(symbolHash>>16)%36u]);
            const Vec3 toCamera=normalized(state.camera.pos-center);const Vec3 glyphRight=normalized(cross3({0,1,0},toCamera));const Vec3 glyphUp=normalized(cross3(toCamera,glyphRight));const float glyphYaw=std::atan2(toCamera.x,toCamera.z);
            for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1u<<(4-col))){const Vec3 pixel=center+glyphRight*((static_cast<float>(col)-2.0f)*0.072f)+glyphUp*((3.0f-static_cast<float>(row))*0.072f);drawBox(pixel,{0.058f,0.058f,0.028f},0,glyphYaw,0,0.94f,1.0f,0.86f);}
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
            glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
            drawBox(center,{0.72f,0.72f,0.72f},0,flower.rotationY,0,VisualIdentity::Flower.r,VisualIdentity::Flower.g,VisualIdentity::Flower.b,0.36f);
            glDisable(GL_CULL_FACE);glDepthMask(GL_TRUE);glDisable(GL_BLEND);
        }
    }
    for (const auto& bullet:state.bullets) if (bullet.alive) {
        const float size=0.72f*1.12f*(bullet.brute?1.7f:1.0f);
        drawBox(bullet.pos,{size,size,size},bullet.spin*1.2f,bullet.spin*1.7f,bullet.spin*0.9f,VisualIdentity::SoulBase.r,VisualIdentity::SoulBase.g,VisualIdentity::SoulBase.b,0.68f);
    }
    if(state.localSettings.particles)for(const auto& particle:state.particles) if(particle.life>0.0f) {
        const float t=particle.maxLife>0.0f?clampf(particle.life/particle.maxLife,0.0f,1.0f):0.0f;
        const float size=particle.size*t;
        const VisualColor color=particleMaterialColor(particle.material,room_environment::roomPlan(state.roomSeed,state.roomIndex).setting,t);
        drawBox(particle.pos,{size,size,size},particle.life*8.0f,particle.life*4.0f,particle.life*6.0f,color.r,color.g,color.b,(particle.material==ParticleMaterial::Environment?0.82f*t:0.9f));
    }
    if(state.localSettings.particles||state.localSettings.portalWindow)drawDoorDataMosh(state);
    if(codec&&codec->showColliders){
        glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);glLineWidth(2.0f);
        const float z0=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
        for(int i=0;i<state.debug.colliderCount;++i){const auto& c=state.roomColliders[i];drawBox({c.center.x,c.center.y,z0+c.center.z},{c.width,c.height,c.depth},0,0,0,VisualIdentity::ElectricMagenta.r,VisualIdentity::ElectricMagenta.g,VisualIdentity::ElectricMagenta.b);}
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glEnable(GL_CULL_FACE);glEnable(GL_LIGHTING);
    }
    if(hudVisible_)drawHud(state);
    if(codec&&codec->open)drawDeveloperCodec(*codec);
    glFlush();
}

void DesktopRenderer::drawDeveloperCodec(const DeveloperCodecState& codec) const{
    glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width_,height_,0,-1,1);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    const auto quad=[](float x,float y,float w,float h,float r,float g,float b,float a){glColor4f(r,g,b,a);glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();};
    const auto text=[&](const std::string& value,float x,float y,float scale,float r,float g,float b,float a){float pen=x;for(char c:value){if(c==' '){pen+=6*scale;continue;}const auto rows=bitmapGlyph(c);for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1u<<(4-col)))quad(pen+col*scale,y+row*scale,scale,scale,r,g,b,a);pen+=6*scale;}};
    const float w=std::min(860.0f,static_cast<float>(width_)-32.0f),h=250.0f,x=(width_-w)*0.5f,y=height_-h-22.0f;
    quad(x,y,w,h,0.005f,0.012f,0.016f,0.92f);quad(x,y,w,2,VisualIdentity::ElectricCyan.r,VisualIdentity::ElectricCyan.g,VisualIdentity::ElectricCyan.b,0.90f);
    text("DEVELOPER CODEC  LOCAL / ALLOWLISTED",x+14,y+12,1.25f,0.72f,0.96f,1.0f,0.95f);
    float lineY=y+38.0f;for(int i=0;i<codec.outputCount;++i,lineY+=17.0f)text(codec.output[i],x+14,lineY,1.05f,0.78f,0.90f,0.92f,0.92f);
    quad(x+10,y+h-38,w-20,26,0.01f,0.02f,0.025f,0.96f);text("> "+codec.input+"_",x+18,y+h-31,1.18f,VisualIdentity::AcidChartreuse.r,VisualIdentity::AcidChartreuse.g,VisualIdentity::AcidChartreuse.b,1.0f);
    glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);
}
