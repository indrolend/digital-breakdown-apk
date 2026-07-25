#include "DesktopRenderer.hpp"
#include "HumanVisual.hpp"
#include "BitmapFont.hpp"
#include "PhoneMenuLayout.hpp"

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
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
float displayedFps=60.0f;
int fpsFrames=0;
auto fpsWindowStart=std::chrono::steady_clock::now();
constexpr float ROOM_WIDTH = 30.0f;
constexpr float ROOM_DEPTH = 42.0f;
constexpr int ROOM_VISUAL_HORIZON = 2;
constexpr float ROOM_WALL_HEIGHT = 7.2f;
constexpr float PI = 3.14159265358979323846f;
constexpr int MENU_FONT_FIRST_CHAR = 32;
constexpr int MENU_FONT_CHAR_COUNT = 95;
constexpr int MENU_FONT_ATLAS_SIZE = 1024;
constexpr float MENU_FONT_BAKE_PX = 64.0f;

struct MenuFontAtlas {
    GLuint texture = 0;
    bool ready = false;
    std::array<stbtt_bakedchar, MENU_FONT_CHAR_COUNT> glyphs{};

    bool load(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        std::vector<unsigned char> fontBytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (fontBytes.empty()) return false;
        std::vector<unsigned char> atlasPixels(MENU_FONT_ATLAS_SIZE * MENU_FONT_ATLAS_SIZE, 0);
        const int bakeResult = stbtt_BakeFontBitmap(
            fontBytes.data(),
            0,
            MENU_FONT_BAKE_PX,
            atlasPixels.data(),
            MENU_FONT_ATLAS_SIZE,
            MENU_FONT_ATLAS_SIZE,
            MENU_FONT_FIRST_CHAR,
            MENU_FONT_CHAR_COUNT,
            glyphs.data());
        if (bakeResult <= 0) return false;
        if (!texture) glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, MENU_FONT_ATLAS_SIZE, MENU_FONT_ATLAS_SIZE, 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlasPixels.data());
        ready = texture != 0;
        return ready;
    }
};

MenuFontAtlas menuRegularFont;
MenuFontAtlas menuSemiboldFont;

Vec3 gradedSceneColor(float r,float g,float b) {
    const float luma=r*0.2126f+g*0.7152f+b*0.0722f;
    r=luma+(r-luma)*1.10f;g=luma+(g-luma)*1.10f;b=luma+(b-luma)*1.10f;
    return {clampf((r-0.5f)*1.06f+0.5f,0.0f,1.0f),clampf((g-0.5f)*1.06f+0.5f,0.0f,1.0f),clampf((b-0.5f)*1.06f+0.5f,0.0f,1.0f)};
}
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
        const bool translucent=batch.color[3]<0.995f;if(translucent){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);}
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
    StaticModelData flower;
    const bool phoneLoaded=phone.load((root/"phone.dbmesh").string());
    const bool flowerLoaded=flower.load((root/"flower.dbmesh").string());
    const bool humanLoaded=humanModel_.load((root/"human.dbhuman").string());
    phoneModelList_=phoneLoaded?compileStaticModel(phone):0; phoneShadowList_=phoneLoaded?compileStaticModel(phone,true):0;
    flowerModelList_=flowerLoaded?compileStaticModel(flower):0; flowerShadowList_=flowerLoaded?compileStaticModel(flower,true):0;
    std::printf("Pass 7 models: phone=%s flower=%s human=%s menuFont=%s/%s\n",phoneModelList_?"loaded":"fallback",flowerModelList_?"loaded":"fallback",humanLoaded?"loaded":"fallback",menuRegularLoaded?"regular":"bitmap",menuSemiboldLoaded?"semibold":"bitmap");
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

void phoneScreenQuad(const PhoneTransformState& phone,float x,float y,float w,float h,float r,float g,float b,float a){
    const Vec3 c=phone.screenCenter+phone.screenNormal*0.020f+phone.screenRight*x+phone.screenUp*y;
    const Vec3 rx=phone.screenRight*(w*0.5f),uy=phone.screenUp*(h*0.5f);
    gradedColor(r,g,b,a);
    glBegin(GL_QUADS);
    glVertex3f((c-rx-uy).x,(c-rx-uy).y,(c-rx-uy).z);
    glVertex3f((c+rx-uy).x,(c+rx-uy).y,(c+rx-uy).z);
    glVertex3f((c+rx+uy).x,(c+rx+uy).y,(c+rx+uy).z);
    glVertex3f((c-rx+uy).x,(c-rx+uy).y,(c-rx+uy).z);
    glEnd();
}

float bitmapPhoneTextWidth(const std::string& value,float scale){return static_cast<float>(value.size())*6.0f*scale;}

void phoneScreenBitmapText(const PhoneTransformState& phone,const std::string& value,float x,float y,float scale,float r,float g,float b,float a){
    float pen=x;
    for(char c:value){
        if(c==' '){pen+=6.0f*scale;continue;}
        const auto rows=bitmapGlyph(c);
        for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1u<<(4-col))){
            const float px=pen+static_cast<float>(col)*scale;
            const float py=y-static_cast<float>(row)*scale;
            phoneScreenQuad(phone,px,py,scale*0.82f,scale*0.82f,r,g,b,a);
        }
        pen+=6.0f*scale;
    }
}

const MenuFontAtlas& menuFont(bool semibold) {
    return semibold && menuSemiboldFont.ready ? menuSemiboldFont : menuRegularFont;
}

float phoneTextWidth(const std::string& value,float scale,bool semibold=false){
    const MenuFontAtlas& font = menuFont(semibold);
    if (!font.ready) return bitmapPhoneTextWidth(value, scale);
    float widthPx = 0.0f;
    for (unsigned char c : value) {
        if (c < MENU_FONT_FIRST_CHAR || c >= MENU_FONT_FIRST_CHAR + MENU_FONT_CHAR_COUNT) {
            widthPx += MENU_FONT_BAKE_PX * 0.32f;
            continue;
        }
        widthPx += font.glyphs[c - MENU_FONT_FIRST_CHAR].xadvance;
    }
    return widthPx * ((scale * 7.0f) / MENU_FONT_BAKE_PX);
}

void phoneScreenTexturedQuad(const PhoneTransformState& phone,float x0,float y0,float x1,float y1,float s0,float t0,float s1,float t1){
    const Vec3 a=phone.screenCenter+phone.screenNormal*0.022f+phone.screenRight*x0+phone.screenUp*y1;
    const Vec3 b=phone.screenCenter+phone.screenNormal*0.022f+phone.screenRight*x1+phone.screenUp*y1;
    const Vec3 c=phone.screenCenter+phone.screenNormal*0.022f+phone.screenRight*x1+phone.screenUp*y0;
    const Vec3 d=phone.screenCenter+phone.screenNormal*0.022f+phone.screenRight*x0+phone.screenUp*y0;
    glBegin(GL_QUADS);
    glTexCoord2f(s0,t1); glVertex3f(a.x,a.y,a.z);
    glTexCoord2f(s1,t1); glVertex3f(b.x,b.y,b.z);
    glTexCoord2f(s1,t0); glVertex3f(c.x,c.y,c.z);
    glTexCoord2f(s0,t0); glVertex3f(d.x,d.y,d.z);
    glEnd();
}

void phoneScreenText(const PhoneTransformState& phone,const std::string& value,float x,float y,float scale,float r,float g,float b,float a,bool semibold=false){
    const MenuFontAtlas& font = menuFont(semibold);
    if (!font.ready) {
        phoneScreenBitmapText(phone, value, x, y, scale, r, g, b, a);
        return;
    }
    const float worldPerPx = (scale * 7.0f) / MENU_FONT_BAKE_PX;
    float penX = 0.0f;
    float penY = 0.0f;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, font.texture);
    glColor4f(r,g,b,a);
    for (unsigned char c : value) {
        if (c < MENU_FONT_FIRST_CHAR || c >= MENU_FONT_FIRST_CHAR + MENU_FONT_CHAR_COUNT) {
            penX += MENU_FONT_BAKE_PX * 0.32f;
            continue;
        }
        stbtt_aligned_quad q{};
        stbtt_GetBakedQuad(font.glyphs.data(), MENU_FONT_ATLAS_SIZE, MENU_FONT_ATLAS_SIZE, c - MENU_FONT_FIRST_CHAR, &penX, &penY, &q, 1);
        const float x0 = x + q.x0 * worldPerPx;
        const float x1 = x + q.x1 * worldPerPx;
        const float y0 = y - q.y0 * worldPerPx;
        const float y1 = y - q.y1 * worldPerPx;
        phoneScreenTexturedQuad(phone, x0, y0, x1, y1, q.s0, q.t0, q.s1, q.t1);
    }
    glDisable(GL_TEXTURE_2D);
}

void phoneScreenPaletteText(const PhoneTransformState& phone,const std::string& value,float x,float y,float scale,float time,float a){
    const auto rainbow=[](float hue){hue-=std::floor(hue);const float k=hue*6.0f,i=std::floor(k),f=k-i,q=1.0f-f;switch(static_cast<int>(i)%6){case 0:return Vec3{1,f,0};case 1:return Vec3{q,1,0};case 2:return Vec3{0,1,f};case 3:return Vec3{0,q,1};case 4:return Vec3{f,0,1};default:return Vec3{1,0,q};}};
    float pen=x;
    for(std::size_t i=0;i<value.size();++i){
        const char c=value[i];
        if(c==' '){pen+=6.0f*scale;continue;}
        const Vec3 color=rainbow(time*0.026f+static_cast<float>(i)*0.115f);
        const std::string letter(1,c);
        phoneScreenText(phone,letter,pen,y+std::sin(time*1.45f+static_cast<float>(i)*0.42f)*scale*0.10f,scale,0.55f+color.x*0.42f,0.65f+color.y*0.34f,0.72f+color.z*0.28f,a,true);
        pen+=phoneTextWidth(letter,scale,true)+scale*0.10f;
    }
}

void drawPhoneMenuSurface(const GameState& state){
    const bool pausedSolo=state.started&&state.uiPaused&&!state.multiplayer.enabled&&!state.upgradeMenu.active;
    const bool menuVisible=state.cinematic.introActive||!state.started||pausedSolo;
    if(!menuVisible||state.camera.firstPerson)return;
    const PhoneTransformState& phone=state.phoneTransform;
    const PhoneMenuLayout layout=makePhoneMenuLayout(state);
    const float screenW=layout.screenW;
    const float alpha=state.cinematic.introActive?1.0f-smoothStep01(clampf(state.cinematic.introElapsed/0.42f,0.0f,1.0f)):1.0f;

    glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
    if(!layout.title.empty()){
        const float tw=phoneTextWidth(layout.title,layout.titleScale,true);
        if(layout.paletteTitle)phoneScreenPaletteText(phone,layout.title,-tw*0.5f,layout.titleY,layout.titleScale,state.time,0.96f*alpha);
        else phoneScreenText(phone,layout.title,-tw*0.5f,layout.titleY,layout.titleScale,0.90f,0.97f,1.0f,0.96f*alpha,true);
    }
    if(layout.joinCode){
        const std::string room=state.multiplayer.roomCode.data();std::string typed;for(int i=0;i<6;++i){typed+=i<static_cast<int>(room.size())?room[i]:'_';if(i<5)typed+=' ';}
        const float scale=screenW*0.020f,tw=phoneTextWidth(typed,scale,true);phoneScreenText(phone,typed,-tw*0.5f,layout.content.y,scale,0.88f,1.0f,1.0f,0.94f*alpha,true);
    }
    for(int i=0;i<layout.rowCount;++i){
        const PhoneMenuRow& row=layout.rows[i];
        const bool selected=row.selectable&&state.hud.menuSelection==row.selectableIndex;
        if(row.kind==PhoneMenuRowKind::Section){
            phoneScreenText(phone,row.label,row.labelX,row.textY,row.scale,Pass7Visual::MetallicTeal.r,Pass7Visual::MetallicTeal.g,Pass7Visual::MetallicTeal.b,0.62f*alpha,true);
            continue;
        }
        const float markerPulse=selected?(0.72f+0.28f*std::sin(state.time*2.2f+i*0.17f)):0.0f;
        const float itemX = row.kind==PhoneMenuRowKind::TwoColumn ? row.labelX : -layout.safe.w*0.22f;
        if(selected){
            phoneScreenQuad(phone,itemX-layout.safe.w*0.085f,row.textY-row.scale*0.36f,layout.screenW*0.010f,layout.screenW*0.010f,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,(0.66f+markerPulse*0.22f)*alpha);
        }
        if(row.kind==PhoneMenuRowKind::TwoColumn){
            const float vw=phoneTextWidth(row.value,row.scale,selected);
            phoneScreenText(phone,row.label,row.labelX,row.textY,row.scale,selected?0.94f:0.70f,selected?1.0f:0.88f,1.0f,selected?0.98f*alpha:0.76f*alpha,selected);
            phoneScreenText(phone,row.value,row.valueRightX-vw,row.textY,row.scale,selected?Pass7Visual::AcidChartreuse.r:Pass7Visual::MetallicTeal.r,selected?Pass7Visual::AcidChartreuse.g:Pass7Visual::MetallicTeal.g,selected?Pass7Visual::AcidChartreuse.b:Pass7Visual::MetallicTeal.b,selected?0.98f*alpha:0.78f*alpha,selected);
        }else{
            phoneScreenText(phone,row.label,itemX,row.textY,row.scale,selected?0.94f:0.70f,selected?1.0f:0.88f,1.0f,selected?0.98f*alpha:0.76f*alpha,selected);
        }
    }
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_LIGHTING);
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
    glColor4f(Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.08f+sheen*0.22f);
    glBegin(GL_QUADS);
    glVertex3f(x-0.002f,cy+halfY*0.82f,cz-halfZ);
    glVertex3f(x-0.002f,cy+halfY*0.82f,cz+halfZ);
    glVertex3f(x-0.002f,cy+halfY,cz+halfZ);
    glVertex3f(x-0.002f,cy+halfY,cz-halfZ);
    glEnd();
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_TEXTURE_2D);glEnable(GL_LIGHTING);
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

void DesktopRenderer::drawHumanModel(const TargetState& target,float time,bool shadow) const {
    humanModel_.skin(target.humanAnimationTime,target.attackTimer,target.attackVariant,humanVertices_);if(humanVertices_.empty())return;
    const bool aliveHuman=!target.slurpable;const HumanVisualPose pose=makeHumanVisualPose(target.visualYaw,target.scale,time,target.visualReaction,aliveHuman);
    const float attackT=target.attackTimer>0?1-clampf(target.attackTimer/HUMAN_SWING_ATTACK_DURATION,0.0f,1.0f):0;
    const float windup=std::sin(clampf(attackT/HUMAN_SWING_COMMIT_PHASE,0.0f,1.0f)*PI*0.5f)*(attackT<HUMAN_SWING_COMMIT_PHASE?1.0f:0.0f),strike=std::sin(clampf((attackT-HUMAN_SWING_COMMIT_PHASE)/(HUMAN_SWING_END_PHASE-HUMAN_SWING_COMMIT_PHASE),0.0f,1.0f)*PI);
    const float side=target.attackVariant%2==0?1.0f:-1.0f,low=target.attackVariant>=2?1.0f:0.0f;
    const float reach=target.attackTimer>0?smoothStep01(clampf((attackT-HUMAN_SWING_COMMIT_PHASE)/(HUMAN_SWING_END_PHASE-HUMAN_SWING_COMMIT_PHASE),0.0f,1.0f)):0.0f;
    const Quat rootQ=quaternionFromEulerXYZ(target.attackTimer>0?windup*0.08f-reach*(0.16f+low*0.05f):0,target.visualYaw+PI,target.attackTimer>0?side*(strike*0.18f-windup*0.24f):0);
    const Vec3 attackForward=lengthSq(target.attackDirection)>0.001f?normalized(target.attackDirection):Vec3{-std::sin(target.visualYaw),0,-std::cos(target.visualYaw)};
    const Vec3 attackLunge=attackForward*(target.attackTimer>0?reach*0.075f*target.scale:0.0f);
    const Vec3 root{target.pos.x+attackLunge.x,target.attackTimer>0?std::sin(attackT*PI)*0.024f*low:0,target.pos.z+attackLunge.z};
    const float matrix[16]={1-2*(rootQ.y*rootQ.y+rootQ.z*rootQ.z),2*(rootQ.x*rootQ.y+rootQ.z*rootQ.w),2*(rootQ.x*rootQ.z-rootQ.y*rootQ.w),0,2*(rootQ.x*rootQ.y-rootQ.z*rootQ.w),1-2*(rootQ.x*rootQ.x+rootQ.z*rootQ.z),2*(rootQ.y*rootQ.z+rootQ.x*rootQ.w),0,2*(rootQ.x*rootQ.z+rootQ.y*rootQ.w),2*(rootQ.y*rootQ.z-rootQ.x*rootQ.w),1-2*(rootQ.x*rootQ.x+rootQ.y*rootQ.y),0,0,0,0,1};
    const bool parryCue=target.attackTimer>0&&attackT>=0.22f&&attackT<=0.46f;const float cue=parryCue?(0.10f+0.05f*std::sin(time*28.0f)):0.0f;const float cueColor[4]={humanModel_.color[0]+(0.55f-humanModel_.color[0])*cue,humanModel_.color[1]+(0.96f-humanModel_.color[1])*cue,humanModel_.color[2]+(1.0f-humanModel_.color[2])*cue,humanModel_.color[3]};
    glPushMatrix();glTranslatef(root.x,root.y,root.z);glMultMatrixf(matrix);glScalef(pose.scale,pose.scale,pose.scale);if(shadow)glColor4f(0.012f,0.018f,0.022f,0.28f);else gradedColor(cueColor[0],cueColor[1],cueColor[2],cueColor[3]);glBegin(GL_TRIANGLES);
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
    if(target.tetherVisible){const Vec3 endpoint=target.tetherAnchor;const Vec3 destination=target.tetherDestination;const Vec3 delta=destination-endpoint;const float len=length(delta);if(len>0.001f){const Vec3 mid=endpoint+delta*0.5f;const float yaw=std::atan2(delta.x,delta.z),pitch=-std::asin(clampf(delta.y/len,-1,1));glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);drawBox(mid,{0.13f*target.tetherWidth,0.13f*target.tetherWidth,std::min(len,4.5f)},pitch,yaw,0,Pass7Visual::Tether.r,Pass7Visual::Tether.g,Pass7Visual::Tether.b,0.34f);glDepthMask(GL_TRUE);glDisable(GL_BLEND);}}
}

void DesktopRenderer::drawRoomTile(const GameState& state, int tileIndex) {
    const float z0 = static_cast<float>(tileIndex) * ROOM_DEPTH;
    const float doorWidth = 5.35f;
    const float doorHeight = 3.95f;
    const float sideW = (ROOM_WIDTH - doorWidth) * 0.5f;
    const float sideX = doorWidth * 0.5f + sideW * 0.5f;
    const float topH = ROOM_WALL_HEIGHT - doorHeight;
    const float topY = doorHeight + topH * 0.5f;
    const float wallR = Pass7Visual::RoomWall.r, wallG = Pass7Visual::RoomWall.g, wallB = Pass7Visual::RoomWall.b;
    drawBox({0,-0.04f,z0},{ROOM_WIDTH,0.08f,ROOM_DEPTH},0,0,0,Pass7Visual::RoomFloor.r,Pass7Visual::RoomFloor.g,Pass7Visual::RoomFloor.b);
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
        drawBox({c.center.x,c.center.y,z0+c.center.z},{c.width,c.height,c.depth},0,0,0,Pass7Visual::RoomObstacle.r,Pass7Visual::RoomObstacle.g,Pass7Visual::RoomObstacle.b);
    }
}

void DesktopRenderer::applyCamera(const GameState& state, float aspect) {
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); perspective(state.camera.verticalFovDegrees, aspect, Pass7Visual::CameraNearPlane, Pass7Visual::CameraFarPlane);
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
    const bool pausedSolo=state.started&&state.uiPaused&&!state.multiplayer.enabled&&!state.upgradeMenu.active;
    if(state.cinematic.introActive||!state.started||pausedSolo){
        glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);return;
    }

    if(state.multiplayer.enabled){const std::string code=state.multiplayer.roomCode.data();const std::string net=state.multiplayer.status.data(),focus=code.empty()?net:code;const float w=std::max(92.0f,static_cast<float>(focus.size())*8.1f+18.0f),x=width_-w-12.0f;quad(x,12,w,30,0.005f,0.012f,0.016f,0.54f);text(focus,x+(w-focus.size()*7.2f)*0.5f,20,1.2f,0.66f,0.96f,1.0f);}

    // Browser goal strip: five compact top-center receptacles.
    const int goalCount=std::max(1,state.hud.requiredGoals);
    const float goalSize=22.0f, goalGap=8.0f;
    const float goalsWidth=goalCount*goalSize+(goalCount-1)*goalGap;
    const float goalsX=(static_cast<float>(width_)-goalsWidth)*0.5f;
    for(int i=0;i<goalCount;++i) {
        const float x=goalsX+i*(goalSize+goalGap);
        quad(x,18,goalSize,goalSize,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.92f);
        const bool filled=i<state.hud.filledGoals;
        quad(x+2,20,goalSize-4,goalSize-4,filled?Pass7Visual::AcidChartreuse.r:0.01f,filled?Pass7Visual::AcidChartreuse.g:0.02f,filled?Pass7Visual::AcidChartreuse.b:0.025f,filled?0.95f:0.88f);
    }

    // Stored-soul mosaic. A quiet fixed square reads as a painted data tile
    // instead of a moving pickup cluster.
    quad(12,74,120,82,0.005f,0.012f,0.016f,0.72f);
    quad(12,74,120,1,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.72f); quad(12,155,120,1,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.72f);
    quad(12,74,1,82,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.72f); quad(131,74,1,82,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.72f);
    constexpr int mosaicColumns=5,mosaicRows=5,mosaicCells=mosaicColumns*mosaicRows;
    const int filledSoulPixels=state.hud.storedSouls<=0?0:std::max(1,static_cast<int>(std::ceil(state.hud.storedSouls/static_cast<float>(PHONE_CAPACITY)*mosaicCells)));
    const bool tvPreview=state.roomIndex==10&&tvGifWall_.available();
    const float tile=9.0f,gap=2.0f,mosaicW=mosaicColumns*tile+(mosaicColumns-1)*gap,mosaicX=12.0f+(120.0f-mosaicW)*0.5f,mosaicY=96.0f;
    for(int i=0;i<mosaicCells;++i){const int col=i%mosaicColumns,row=i/mosaicColumns;VisualColor color=Pass7Visual::DataMosaicPalette[i];if(tvPreview){const auto tv=tvGifWall_.sample(col*(TvGifWall::Columns-1)/(mosaicColumns-1),row*(TvGifWall::Rows-1)/(mosaicRows-1),0.0f,state.secretTv.signal);color={tv.r,tv.g,tv.b};}const bool filled=i<filledSoulPixels;const float x=mosaicX+col*(tile+gap),y=mosaicY+row*(tile+gap),alpha=filled?0.92f:0.20f,shade=filled?1.0f:0.22f;quad(x,y,tile,tile,color.r*shade,color.g*shade,color.b*shade,alpha);if(filled&&i==filledSoulPixels-1){const float pulse=0.5f+0.5f*std::sin(state.time*8.0f);quad(x-1,y-1,tile+2,1,color.r,color.g,color.b,0.28f+0.18f*pulse);quad(x-1,y+tile,tile+2,1,color.r,color.g,color.b,0.18f+0.12f*pulse);}}
    text("SOULS "+std::to_string(state.hud.storedSouls),20,80,1.2f,0.78f,0.94f,1.0f,0.82f);

    text("ROOM: "+std::to_string(state.roomIndex),12,170,1.5f);
    text("GOALS: "+std::to_string(state.hud.filledGoals)+"/"+std::to_string(state.hud.requiredGoals),12,187,1.5f);
    text(state.roomClear?"DOOR: OPEN":"DOOR: LOOP",12,204,1.5f,state.roomClear?0.72f:1.0f,1.0f,state.roomClear?0.74f:1.0f);
    text("TOKENS: "+std::to_string(state.progression.permanent.tokens),12,221,1.25f,0.82f,1.0f,0.91f);
    if(state.progression.run.accuracyStacks>0){char accuracy[32]{};std::snprintf(accuracy,sizeof(accuracy),"ACCURACY X%.2F",state.progression.run.accuracyMultiplier);text(accuracy,12,304,1.15f,Pass7Visual::SignalGreen.r,Pass7Visual::SignalGreen.g,Pass7Visual::SignalGreen.b);}
    if(state.progression.run.headshotRegenTax>0.01f){char tax[32]{};std::snprintf(tax,sizeof(tax),"REGEN -%d%%",static_cast<int>(std::round(state.progression.run.headshotRegenTax*100.0f)));text(tax,12,320,1.05f,1.0f,0.72f,0.62f);}
    if(state.hud.buildLabel[0])text(state.hud.buildLabel.data(),12,336,1.0f,0.58f,0.92f,1.0f,0.82f);
    // The collision-authoritative head center owns a cycling data glyph, so
    // the aim cue cannot drift away from the actual critical volume.
    overlayAlpha=state.hud.critMarkerOpacity;
    {const Vec3 viewForward=normalized(state.camera.lookTarget-state.camera.pos),viewRight=normalized(cross3(viewForward,{0,1,0})),viewUp=cross3(viewRight,viewForward);const float tanHalf=std::tan(state.camera.verticalFovDegrees*PI/360.0f),aspect=static_cast<float>(width_)/std::max(1,height_);constexpr char glyphs[]="01ABCDEFHIKMNPRSTXYZ+-/:";for(int i=0;i<TARGET_COUNT;++i){const TargetState& target=state.targets[i];if(!target.alive||target.slurpable)continue;const float attackT=target.attackTimer>0?1-clampf(target.attackTimer/HUMAN_SWING_ATTACK_DURATION,0,1):-1.0f,attackBob=target.attackTimer>0?std::sin(attackT*PI)*0.035f*(target.attackVariant>=2?1.0f:0.0f):0;const Vec3 world{target.pos.x,(PASS7_HUMAN_VISUAL_SPEC.totalHeight-PASS7_HUMAN_VISUAL_SPEC.headRadius)*target.scale+attackBob,target.pos.z},delta=world-state.camera.pos;const float depth=dot3(delta,viewForward);if(depth<=0.18f||depth>16.0f)continue;const float nx=dot3(delta,viewRight)/(depth*tanHalf*aspect),ny=dot3(delta,viewUp)/(depth*tanHalf);if(std::abs(nx)>1.04f||std::abs(ny)>1.04f)continue;const float armorMax=target.brute?4.0f:2.0f,damage=1.0f-clampf(target.armor/armorMax,0,1);const bool perfectReady=attackT>=0.22f&&attackT<=0.46f;const Vec3 color=rainbow(0.51f+damage*0.38f);const int cycle=(static_cast<int>(state.time*10.0f)+i*7+state.roomIndex*3)%static_cast<int>(sizeof(glyphs)-1);const float perspectiveScale=clampf(8.0f/depth,0.82f,1.55f),marker=(11.0f+damage*4.0f+(perfectReady?3.0f:0))*perspectiveScale,scale=(1.35f+damage*0.28f+(perfectReady?0.18f:0))*perspectiveScale,sx=(nx*0.5f+0.5f)*width_,sy=(0.5f-ny*0.5f)*height_,alpha=0.72f+damage*0.20f+(perfectReady?0.08f:0),spin=state.time*0.9f+i*0.37f;rotatedQuad(sx,sy,marker,marker,PI*0.25f+spin,color.x,color.y,color.z,0.10f+damage*0.08f);rotatedQuad(sx-marker,sy,marker*0.52f,2,spin*0.08f,color.x,color.y,color.z,alpha);rotatedQuad(sx+marker,sy,marker*0.52f,2,spin*0.08f,color.x,color.y,color.z,alpha);rotatedQuad(sx,sy-marker,2,marker*0.52f,spin*0.08f,color.x,color.y,color.z,alpha);rotatedQuad(sx,sy+marker,2,marker*0.52f,spin*0.08f,color.x,color.y,color.z,alpha);text(std::string(1,glyphs[cycle]),sx-2.5f*scale+1,sy-3.5f*scale+1,scale,0,0,0,alpha*0.85f);text(std::string(1,glyphs[cycle]),sx-2.5f*scale,sy-3.5f*scale,scale,color.x,color.y,color.z,alpha);}}
    overlayAlpha=1.0f;
    {const auto labelFor=[](int signal)->const char*{switch(signal){case 1:return "HELP";case 2:return "PING";case 3:return "GROUP";case 4:return "OK";default:return "";}};const auto colorFor=[](int signal)->VisualColor{switch(signal){case 1:return Pass7Visual::ElectricMagenta;case 2:return Pass7Visual::ElectricCyan;case 3:return Pass7Visual::AcidChartreuse;case 4:return Pass7Visual::WarmGold;default:return Pass7Visual::ElectricCyan;}};const Vec3 viewForward=normalized(state.camera.lookTarget-state.camera.pos),viewRight=normalized(cross3(viewForward,{0,1,0})),viewUp=cross3(viewRight,viewForward);const float tanHalf=std::tan(state.camera.verticalFovDegrees*PI/360.0f),aspect=static_cast<float>(width_)/std::max(1,height_);const auto drawSignal=[&](const PlayerState& player){if(player.commSignal<1||player.commSignal>4||player.commSignalTimer<=0.0f)return;const Vec3 world=player.pos+Vec3{0,1.05f,0},delta=world-state.camera.pos;const float depth=dot3(delta,viewForward);if(depth<=0.18f||depth>24.0f)return;const float nx=dot3(delta,viewRight)/(depth*tanHalf*aspect),ny=dot3(delta,viewUp)/(depth*tanHalf);if(std::abs(nx)>1.08f||std::abs(ny)>1.08f)return;const char* label=labelFor(player.commSignal);const VisualColor c=colorFor(player.commSignal);const float sx=(nx*0.5f+0.5f)*width_,sy=(0.5f-ny*0.5f)*height_,fade=clampf(player.commSignalTimer/0.35f,0.0f,1.0f),scale=clampf(9.0f/depth,1.15f,2.15f),tw=std::strlen(label)*6.0f*scale,pw=tw+18.0f*scale,ph=13.0f*scale,pulse=0.5f+0.5f*std::sin(state.time*8.0f);quad(sx-pw*0.5f,sy-ph*0.5f,pw,ph,Pass7Visual::DeepPlum.r*0.12f,Pass7Visual::DeepPlum.g*0.12f,Pass7Visual::DeepPlum.b*0.12f,0.52f*fade);quad(sx-pw*0.5f,sy-ph*0.5f,pw,1.4f*scale,c.r,c.g,c.b,(0.58f+0.18f*pulse)*fade);text(label,sx-tw*0.5f,sy-3.5f*scale,scale,c.r,c.g,c.b,0.96f*fade);};drawSignal(state.player);for(const auto& peer:state.multiplayer.peers)if(peer.active)drawSignal(peer.player);}
    if(state.hud.headshotPulse>0.001f){const float charge=clampf(state.hud.headshotKillCharge,0,1),pulse=state.hud.headshotPulse,eased=pulse*pulse,w=static_cast<float>(width_),h=static_cast<float>(height_),breath=0.5f+0.5f*std::sin(state.time*2.4f),mist=10.0f+charge*8.0f+state.hud.perfectPulse*4.0f;const Vec3 core=rainbow(0.51f+charge*0.40f+state.progression.run.accuracyStacks*0.012f),accent=rainbow(0.68f+charge*0.22f+state.time*0.014f);const float veil=eased*(0.035f+charge*0.070f),wisp=pulse*(0.075f+charge*0.105f),spark=pulse*(0.10f+charge*0.14f);quad(0,0,w,mist,core.x,core.y,core.z,veil);quad(0,h-mist,w,mist,accent.x,accent.y,accent.z,veil);quad(0,0,mist,h,accent.x,accent.y,accent.z,veil*0.90f);quad(w-mist,0,mist,h,core.x,core.y,core.z,veil*0.90f);for(int i=0;i<3;++i){const float phase=state.time*(0.55f+i*0.17f)+i*2.1f,drift=0.5f+0.5f*std::sin(phase),len=w*(0.22f+0.10f*i+0.08f*breath),thick=1.2f+i*1.1f+charge*1.4f,alpha=wisp*(0.72f-0.14f*i),x=clampf(drift*(w+len)-len,0.0f,w-len);const Vec3 c=i==1?accent:core;quad(x,2.0f+i*4.0f,len,thick,c.x,c.y,c.z,alpha);quad(w-x-len,h-3.0f-i*4.4f,len,thick,c.x,c.y,c.z,alpha*0.82f);const float y=clampf((0.5f+0.5f*std::sin(phase*0.81f+1.7f))*(h+len)-len,0.0f,h-len);quad(2.0f+i*4.0f,y,thick,len,c.x,c.y,c.z,alpha*0.70f);quad(w-3.0f-i*4.4f,h-y-len,thick,len,c.x,c.y,c.z,alpha*0.64f);}const float corner=clampf(std::min(w,h)*0.10f,34.0f,84.0f);quad(0,0,corner,2.0f,core.x,core.y,core.z,spark);quad(0,0,2.0f,corner,accent.x,accent.y,accent.z,spark*0.85f);quad(w-corner,h-2.0f,corner,2.0f,accent.x,accent.y,accent.z,spark*0.75f);quad(w-2.0f,h-corner,2.0f,corner,core.x,core.y,core.z,spark*0.65f);}

    // Battery display remains visible while vacuuming without a target; target lock is separate.
    quad(12,236,148,30,0.005f,0.012f,0.016f,0.68f);
    quad(20,249,134,8,0.52f,0.67f,0.72f,0.72f);
    quad(21,250,132,6,0.025f,0.035f,0.04f,0.95f);
    const float battery=clampf(state.hud.batteryFill,0.0f,1.0f);
    quad(21,250,132*battery,6,state.hud.lowBattery?1.0f:0.92f,state.hud.lowBattery?0.18f:0.97f,state.hud.lowBattery?0.12f:1.0f,0.98f);
    text("BATTERY",20,240,1.1f,state.hud.lowBattery?1.0f:1.0f,state.hud.lowBattery?0.34f:1.0f,state.hud.lowBattery?0.34f:1.0f);
    if(state.energy.comboHits>0&&state.time-state.energy.lastComboHitTime<=1.8f){char multiplier[16]{};std::snprintf(multiplier,sizeof(multiplier),"X%.2F",state.energy.comboMultiplier);text(multiplier,112,240,1.0f,0.45f,1.0f,0.78f);}
    if(state.hud.flowerStacks>0 || state.hud.supplementalFill>0.001f){
        quad(12,274,148,24,0.005f,0.012f,0.016f,0.68f);
        quad(21,283,132,6,0.025f,0.035f,0.04f,0.95f);
        quad(21,283,132*clampf(state.hud.supplementalFill,0.0f,1.0f),6,0.35f,1.0f,0.68f,0.98f);
        text("POWER X"+std::to_string(state.hud.flowerStacks),20,276,1.0f,0.65f,1.0f,0.78f);
    }
    if(state.hud.energyTicker[0]&&state.time<state.hud.energyTickerUntil){const std::string ticker=state.hud.energyTicker.data();const float scale=1.35f,tw=ticker.size()*6*scale,pw=std::max(118.0f,tw+16.0f),px=(width_-pw)*0.5f;const int type=state.hud.energyTickerType;const VisualColor tickerColor=type==1?Pass7Visual::Copper:(type==0?Pass7Visual::SignalGreen:Pass7Visual::ElectricCyan);quad(px,72,pw,18,0,0,0,0.54f);quad(px,72,pw,1,tickerColor.r,tickerColor.g,tickerColor.b,0.72f);text(ticker,(width_-tw)*0.5f,77,scale,tickerColor.r,tickerColor.g,tickerColor.b);}
    if(state.player.grabbedByTarget>=0){const std::string hint="WIGGLE  A  D";const float s=1.7f;text(hint,(width_-hint.size()*6*s)*0.5f,height_*0.69f,s,1.0f,0.82f,0.68f,0.94f);}
    if(state.player.downed){const std::string hint="SIGNAL DOWN  "+std::to_string(static_cast<int>(std::ceil(state.player.bleedoutTimer)));const float s=1.8f;text(hint,(width_-hint.size()*6*s)*0.5f,height_*0.55f,s,1.0f,0.48f,0.42f,0.96f);}
    if(state.player.inSecretRoom){const std::string hint=state.secretTv.broken?"NO SIGNAL":"SIGNAL "+std::to_string(state.secretTv.signal)+"   SHOOT TO DONATE";const float s=1.35f;text(hint,(width_-hint.size()*6*s)*0.5f,54,s,0.72f,0.94f,0.96f,0.88f);}

    // Browser reticle: one persistent rotor owns four independently translated arms.
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
        glPushMatrix();glScalef(menuUiScale,menuUiScale,1.0f);
        const float pw=std::min(680.0f,menuCanvasW-24.0f),ph=300.0f,px=(menuCanvasW-pw)*0.5f,py=(menuCanvasH-ph)*0.5f;
        quad(px,py,pw,ph,0.01f,0.03f,0.04f,0.16f);quad(px,py,pw,1,0.62f,0.96f,1,0.62f);quad(px,py+ph-1,pw,1,0.62f,0.96f,1,0.42f);text("ROUND "+std::to_string(state.roomIndex),px+18,py+16,2.0f);text(state.multiplayer.enabled&&!state.multiplayer.authoritativeHost?"HOST IS CHOOSING":"CHOOSE ONE RUN UPGRADE",px+18,py+42,1.25f,0.72f,1.0f,0.86f);
        const float cellW=(pw-24.0f)/3.0f;const std::string labels[3]={"SHOT","LUNGE","ATTACK"};const auto choice=[&](int item,float top,float height){const bool selected=state.hud.menuSelection==item;const float pulse=selected?clampf(state.cinematic.textInteraction,0,1):0,cx=px+12+(item%3)*cellW+(cellW-4)*0.5f,cy=py+top+height*0.5f-pulse*1.5f,tilt=selected?std::sin(state.time*2.5f+item*1.7f)*0.025f:0,scale=2.15f+pulse*0.08f;rotatedQuad(cx,cy,cellW-6,height,tilt,selected?0.16f:0.02f,selected?0.86f:0.08f,selected?1.0f:0.11f,selected?0.20f:0.09f);rotatedQuad(cx,cy+height*0.5f-1,cellW-20,1,tilt,0.66f,0.97f,1.0f,selected?0.76f:0.26f);const std::string& label=labels[item%3];text(label,cx-label.size()*6*scale*0.5f,cy-3.5f*scale,scale,selected?1.0f:0.82f,selected?1.0f:0.94f,1.0f);};
        for(int i=0;i<3;++i)choice(i,66,76);text("PERMANENT  TOKENS "+std::to_string(state.progression.permanent.tokens),px+18,py+158,1.2f,0.82f,1.0f,0.91f);for(int i=3;i<6;++i)choice(i,184,66);
        for(int i=0;i<3;++i){const std::string level=std::to_string(state.progression.permanent.levels[i])+"/5";text(level,px+12+i*cellW+cellW-level.size()*6*0.9f-12,py+256,0.9f,0.66f,0.90f,1.0f);}
        text("COST 1 TOKEN",px+18,py+278,1.05f,0.72f,0.90f,1.0f);text("ESC EXIT",px+pw-82,py+278,1.05f,0.82f,0.94f,1.0f);
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
    if(!state.doorTransition.active||state.doorTransition.progress<=0.018f){glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,width_,height_,0);datamoshFrameReady_=true;return;}
    if(!datamoshFrameReady_)return;
    const float strength=clampf(state.doorTransition.progress,0,1),alpha=clampf(0.34f+strength*0.56f,0.34f,0.90f);
    const float mvX=clampf(state.doorTransition.frameMotion.x*width_*0.045f,-20.0f,20.0f)*(0.42f+strength*0.92f);
    const float mvY=clampf(-state.doorTransition.frameMotion.z*height_*0.030f,-24.0f,24.0f)*(0.42f+strength*0.92f);
    glDisable(GL_LIGHTING);glDisable(GL_DEPTH_TEST);glEnable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width_,0,height_,-1,1);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    for(int pass=5;pass>=1;--pass){const float k=pass/5.0f;glColor4f(1,1,1,(0.10f+0.14f*k)*strength);glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(mvX*k,mvY*k);glTexCoord2f(1,0);glVertex2f(width_+mvX*k,mvY*k);glTexCoord2f(1,1);glVertex2f(width_+mvX*k,height_+mvY*k);glTexCoord2f(0,1);glVertex2f(mvX*k,height_+mvY*k);glEnd();}
    glColor4f(1,1,1,alpha);glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(mvX,mvY);glTexCoord2f(1,0);glVertex2f(width_+mvX,mvY);glTexCoord2f(1,1);glVertex2f(width_+mvX,height_+mvY);glTexCoord2f(0,1);glVertex2f(mvX,height_+mvY);glEnd();
    glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_BLEND);glDisable(GL_TEXTURE_2D);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);
}

void DesktopRenderer::draw(const GameState& state) const {
    ++fpsFrames;const auto now=std::chrono::steady_clock::now();const float elapsed=std::chrono::duration<float>(now-fpsWindowStart).count();if(elapsed>=0.5f){displayedFps=fpsFrames/elapsed;fpsFrames=0;fpsWindowStart=now;}
    glClearColor(Pass7Visual::Background.r,Pass7Visual::Background.g,Pass7Visual::Background.b,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_LIGHT1); glEnable(GL_COLOR_MATERIAL);
    const GLfloat ambient[]={0.30f,0.37f,0.40f,1.0f}; glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);
    const GLfloat sunDiffuse[]={1.0f,1.0f,1.0f,1.0f}, sunPos[]={30.0f,60.0f,25.0f,0.0f};
    glLightfv(GL_LIGHT0,GL_DIFFUSE,sunDiffuse); glLightfv(GL_LIGHT0,GL_POSITION,sunPos);
    const GLfloat fillDiffuse[]={0.20f,0.28f,0.35f,1.0f}, fillPos[]={-20.0f,25.0f,-30.0f,0.0f};
    glLightfv(GL_LIGHT1,GL_DIFFUSE,fillDiffuse); glLightfv(GL_LIGHT1,GL_POSITION,fillPos);
    glEnable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_LIGHTING); glEnable(GL_NORMALIZE);
    applyCamera(state, static_cast<float>(width_)/static_cast<float>(height_));
    for(int tile=state.topology.currentTileIndex-ROOM_VISUAL_HORIZON;tile<=state.topology.currentTileIndex+ROOM_VISUAL_HORIZON;++tile)drawRoomTile(state,tile);

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
        drawBox(pushedCenter,{0.08f+push*0.42f,2.30f+breathe+knock*0.10f,2.20f+breathe+knock*0.08f},0,0,0,Pass7Visual::TvMembrane.r,Pass7Visual::TvMembrane.g,Pass7Visual::TvMembrane.b,alpha);
        if(knock>0.025f)drawBox(pushedCenter+state.secretTv.entranceNormal*(0.07f+push*0.5f),{0.03f,1.82f+knock*0.24f,1.72f+knock*0.20f},0,0,0,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.18f*knock);
        glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    }
    if(state.player.inSecretRoom){
        drawBox({40.4f,-0.04f,0},{7.4f,0.08f,6.4f},0,0,0,Pass7Visual::SecretFloor.r,Pass7Visual::SecretFloor.g,Pass7Visual::SecretFloor.b);
        drawBox({36.7f,2.4f,0},{0.12f,4.8f,6.4f},0,0,0,Pass7Visual::SecretWall.r,Pass7Visual::SecretWall.g,Pass7Visual::SecretWall.b);
        drawBox({40.4f,2.4f,-3.2f},{7.4f,4.8f,0.12f},0,0,0,Pass7Visual::SecretWall.r,Pass7Visual::SecretWall.g,Pass7Visual::SecretWall.b);
        drawBox({40.4f,2.4f,3.2f},{7.4f,4.8f,0.12f},0,0,0,Pass7Visual::SecretWall.r,Pass7Visual::SecretWall.g,Pass7Visual::SecretWall.b);
        drawBox({42.25f,0.70f,0},{1.75f,1.35f,0.82f},0,-1.5708f,0,Pass7Visual::SecretBlack.r,Pass7Visual::SecretBlack.g,Pass7Visual::SecretBlack.b);
        float phoneProximity=0.0f;const Vec3 tvPosition{41.82f,0.78f,0};
        const auto includePhone=[&](const PlayerState& player,bool active){if(!active||!player.inSecretRoom)return;const float distance=length(player.pos-tvPosition);phoneProximity=std::max(phoneProximity,1.0f-clampf(distance/6.0f,0.0f,1.0f));};
        includePhone(state.player,true);if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)includePhone(peer.player,peer.active);
        drawSecretTvScreen(state,phoneProximity);
        drawBox({41.35f,0.18f,-0.80f},{1.8f,0.055f,0.055f},0,0.18f,0,Pass7Visual::SecretCable.r,Pass7Visual::SecretCable.g,Pass7Visual::SecretCable.b);
        drawBox({41.45f,0.16f,0.76f},{2.1f,0.045f,0.045f},0,-0.22f,0,Pass7Visual::SecretCable.r,Pass7Visual::SecretCable.g,Pass7Visual::SecretCable.b);
    }

    // Directional planar shadows: project each caster's real geometry along the
    // browser sun vector onto the floor. Silhouette, length and motion therefore
    // come from object vertices, height and light direction rather than blobs.
    if(state.localSettings.shadows){const float shadowMatrix[16]={1,0,0,0,-0.5f,0,-25.0f/60.0f,0,0,0,1,0,0.006f,0.012f,0.005f,1};
    glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glPushMatrix();glMultMatrixf(shadowMatrix);
    if(!state.camera.firstPerson){if(phoneShadowList_)drawStaticModel(phoneShadowList_,state.phoneTransform.position,state.phoneVisual.bodyScale,state.phoneTransform.orientation);else drawBox(state.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},state.phoneTransform.orientation,0.012f,0.018f,0.022f);}
    if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.playerId!=state.multiplayer.localPlayerId&&peer.player.alive){if(phoneShadowList_)drawStaticModel(phoneShadowList_,peer.phoneTransform.position,peer.phoneVisual.bodyScale,peer.phoneTransform.orientation);else drawBox(peer.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},peer.phoneTransform.orientation,0.012f,0.018f,0.022f);}
    const float shadowTileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(int offset=-1;offset<=1;++offset)for(auto target:state.targets)if(target.alive){target.pos.z=shadowTileOrigin+static_cast<float>(offset)*ROOM_DEPTH+(target.pos.z-std::floor((target.pos.z+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);if(!target.slurpable){if(humanModel_.valid())drawHumanModel(target,state.time,true);}if(target.slurpable&&target.soulVisual.visible&&target.soulCubeAmount>0.001f){const Vec3 center=target.pos+Vec3{0,0.57f+target.soulVisual.verticalOffset,0};const float cubeSize=0.72f*0.78f*target.scale*target.soulVisual.morphScale;drawBox(center,{cubeSize*target.soulVisual.scale.x,cubeSize*target.soulVisual.scale.y,cubeSize*target.soulVisual.scale.z},0,target.soulVisual.rotationY,0,0.012f,0.018f,0.022f,0.28f);}}
    for(const auto& flower:state.flowers)if(flower.active){const Vec3 center{flower.pos.x,flower.pos.y,flower.pos.z+shadowTileOrigin};if(flowerShadowList_)drawStaticModel(flowerShadowList_,center,{1,1,1},quatAxisAngle({0,1,0},flower.rotationY));}
    for(const auto& bullet:state.bullets)if(bullet.alive){const float size=0.72f*1.12f*(bullet.brute?1.7f:1.0f);drawBox(bullet.pos,{size,size,size},bullet.spin*1.2f,bullet.spin*1.7f,bullet.spin*0.9f,0.012f,0.018f,0.022f,0.24f);}
    for(int i=0;i<state.debug.colliderCount;++i){const auto& c=state.roomColliders[i];drawBox({c.center.x,c.center.y,shadowTileOrigin+c.center.z},{c.width,c.height,c.depth},0,0,0,0.012f,0.018f,0.022f,0.20f);}
    glPopMatrix();glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_LIGHTING);}

    if (!state.camera.firstPerson) {
        const Vec3 phonePos=state.phoneTransform.position;
        const auto& pv=state.phoneVisual;
        const Quat phoneOrientation=state.phoneTransform.orientation;
        if(phoneModelList_) drawStaticModel(phoneModelList_,phonePos,pv.bodyScale,phoneOrientation);
        else drawBox(phonePos,{PHONE_BODY_WIDTH*pv.bodyScale.x,PHONE_BODY_HEIGHT*pv.bodyScale.y,PHONE_BODY_DEPTH},phoneOrientation,Pass7Visual::PhoneBody.r,Pass7Visual::PhoneBody.g,Pass7Visual::PhoneBody.b);
        const float glow=std::min(1.0f,0.45f+pv.screenGlow*0.36f);
        drawBox(state.phoneTransform.screenCenter,{PHONE_SCREEN_WIDTH*pv.screenScale.x,PHONE_SCREEN_HEIGHT*pv.screenScale.y,PHONE_SCREEN_DEPTH},phoneOrientation,Pass7Visual::PhoneEmission.r*glow,Pass7Visual::PhoneEmission.g*glow,Pass7Visual::PhoneEmission.b*glow);
        drawPhoneMenuSurface(state);
    }
    if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.playerId!=state.multiplayer.localPlayerId&&peer.player.alive){const auto& pv=peer.phoneVisual;if(phoneModelList_)drawStaticModel(phoneModelList_,peer.phoneTransform.position,pv.bodyScale,peer.phoneTransform.orientation);else drawBox(peer.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},peer.phoneTransform.orientation,0.32f,0.86f,1.0f);drawBox(peer.phoneTransform.screenCenter,{PHONE_SCREEN_WIDTH,PHONE_SCREEN_HEIGHT,PHONE_SCREEN_DEPTH},peer.phoneTransform.orientation,0.05f,0.55f,0.78f);}

    const MeleeVisualState& melee=state.meleeVisual;
    if(melee.visualTimer>0.0f && !melee.locomotionLunge){
        const float t=1.0f-clampf(melee.visualTimer/std::max(0.001f,melee.visualDuration),0.0f,1.0f);
        const float fade=1.0f-t, hitBoost=melee.visualHit?1.25f:0.72f;
        glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        const Quat slashQ=quatAxisAngle({0,1,0},state.player.yaw)*quatAxisAngle({1,0,0},PI*0.5f)*quatAxisAngle({0,0,1},-PI*0.28f+t*PI*1.15f);
        fxRibbon(melee.origin+melee.direction*(0.66f+0.22f*t),slashQ,{(0.75f+t*0.72f)*hitBoost,(0.75f+t*0.72f)*hitBoost,1},0,PI*1.35f,24,0.494f,0.546f,Pass7Visual::ElectricMagenta.r,Pass7Visual::ElectricMagenta.g,Pass7Visual::ElectricMagenta.b,fade*0.66f);
        const Vec3 delta=melee.impact-melee.origin; const float len=std::max(0.3f,length(delta));
        const Vec3 mid=melee.origin+delta*0.46f; const float yaw=std::atan2(delta.x,delta.z); const float pitch=-std::asin(clampf(delta.y/std::max(len,0.001f),-1.0f,1.0f));
        const Quat streakQ=quatAxisAngle({0,1,0},yaw)*quatAxisAngle({1,0,0},pitch);
        fxStreak(mid,streakQ,len*(0.70f+std::sin(t*PI)*0.18f),0.11f*(1+std::sin(t*PI)*1.2f),Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,fade*0.44f);
        fxRibbon(melee.impact,{}, {(0.45f+t*1.45f)*hitBoost,(0.45f+t*1.45f)*hitBoost,1},0,PI*2,24,0.318f,0.362f,Pass7Visual::AcidChartreuse.r,Pass7Visual::AcidChartreuse.g,Pass7Visual::AcidChartreuse.b,melee.visualHit?fade*0.82f:fade*0.24f);
        glDisable(GL_BLEND); glEnable(GL_LIGHTING);
    }

    const float tileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(int offset=-1;offset<=1;++offset)for (auto target:state.targets) if (target.alive) {
        Vec3 p=target.pos; p.z=tileOrigin+static_cast<float>(offset)*ROOM_DEPTH+(target.pos.z-std::floor((target.pos.z+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);
        const float mirrorShift=p.z-target.pos.z;
        target.tetherAnchor.z+=mirrorShift;target.tetherDestination.z+=mirrorShift;target.latchPoint.z+=mirrorShift;
        target.pos = p;
        if (!target.slurpable) {
            if(humanModel_.valid())drawHumanModel(target,state.time);else drawProceduralHumanDesktop(target,state.time,target.slurpable?0.70f:0.14f,1.0f,target.slurpable?0.78f:0.32f);
        }
        if (target.slurpable && target.soulCubeAmount > 0.001f) {
            const auto& sv=target.soulVisual;
            if (!sv.visible) continue;
            const Vec3 soulCenter=p+Vec3{0,0.57f+sv.verticalOffset,0};
            drawSoulFlesh(target,soulCenter);
            const float cube=0.72f*0.78f*target.scale*sv.morphScale;
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
            drawBox(soulCenter,{cube*sv.scale.x,cube*sv.scale.y,cube*sv.scale.z},0,sv.rotationY,0,sv.color.r,sv.color.g,sv.color.b,0.68f);
            glDepthMask(GL_TRUE); glDisable(GL_BLEND);
        }
    }
    for(int offset=-ROOM_VISUAL_HORIZON;offset<=ROOM_VISUAL_HORIZON;++offset)for (int captureIndex=0;captureIndex<state.requiredSouls;++captureIndex) {
        const auto& capture=state.captures[captureIndex];
        Vec3 p=capture.pos; p.z+=tileOrigin+static_cast<float>(offset)*ROOM_DEPTH;
        drawBox(p+Vec3{0,0,-0.04f},{0.72f,0.72f,0.06f},0,0,0,Pass7Visual::MetallicTeal.r*0.74f,Pass7Visual::MetallicTeal.g*0.74f,Pass7Visual::MetallicTeal.b*0.74f);
        drawBox(p,{0.52f,0.52f,0.08f},0,0,0,0.02f,0.03f,0.04f);
        if(capture.filled) drawBox(p+Vec3{0,0,0.12f},{0.36f,0.36f,0.36f},state.time*1.5f,state.time*2.0f,state.time,Pass7Visual::SoulBase.r,Pass7Visual::SoulBase.g,Pass7Visual::SoulBase.b);
    }
    for(const auto& flower:state.flowers) if(flower.active){
        for(int offset=-ROOM_VISUAL_HORIZON;offset<=ROOM_VISUAL_HORIZON;++offset){
            const Vec3 center{flower.pos.x,flower.pos.y,flower.pos.z+static_cast<float>(state.topology.currentTileIndex+offset)*ROOM_DEPTH};
            if(flowerModelList_) drawStaticModel(flowerModelList_,center,{1,1,1},quatAxisAngle({0,1,0},flower.rotationY));
            else {
                drawBox(center,{0.20f,0.20f,0.20f},0,flower.rotationY,0,Pass7Visual::FlowerCore.r,Pass7Visual::FlowerCore.g,Pass7Visual::FlowerCore.b);
                for(int petal=0;petal<5;++petal){
                    const float angle=flower.rotationY+static_cast<float>(petal)*PI*2.0f/5.0f;
                    const Vec3 p=center+Vec3{std::cos(angle)*0.23f,0,std::sin(angle)*0.23f};
                    drawBox(p,{0.30f,0.12f,0.16f},0,-angle,0,Pass7Visual::Flower.r,Pass7Visual::Flower.g,Pass7Visual::Flower.b);
                }
            }
        }
    }
    for (const auto& bullet:state.bullets) if (bullet.alive) {
        const float size=0.72f*1.12f*(bullet.brute?1.7f:1.0f);
        drawBox(bullet.pos,{size,size,size},bullet.spin*1.2f,bullet.spin*1.7f,bullet.spin*0.9f,Pass7Visual::SoulBase.r,Pass7Visual::SoulBase.g,Pass7Visual::SoulBase.b,0.68f);
    }
    if(state.localSettings.particles)for(const auto& particle:state.particles) if(particle.life>0.0f) {
        const float t=particle.maxLife>0.0f?clampf(particle.life/particle.maxLife,0.0f,1.0f):0.0f;
        const float size=particle.size*t;
        if(particle.kind==1)drawBox(particle.pos,{size,size,size},particle.life*8.0f,particle.life*4.0f,particle.life*6.0f,0.16f,0.39f,0.42f,0.82f*t);
        else drawBox(particle.pos,{size,size,size},particle.life*8.0f,particle.life*4.0f,particle.life*6.0f,1.0f,0.267f,0.267f,0.9f);
    }
    drawDoorDataMosh(state);
    drawHud(state);
    glFlush();
}
