#include "DesktopRenderer.hpp"
#include "HumanVisual.hpp"
#include "BitmapFont.hpp"

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
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
float displayedFps=60.0f;
int fpsFrames=0;
auto fpsWindowStart=std::chrono::steady_clock::now();
constexpr float ROOM_WIDTH = 30.0f;
constexpr float ROOM_DEPTH = 42.0f;
constexpr int ROOM_VISUAL_HORIZON = 2;
constexpr float ROOM_WALL_HEIGHT = 7.2f;
constexpr float GROUND_Y = 0.08f;
constexpr float PI = 3.14159265358979323846f;

Vec3 gradedSceneColor(float r,float g,float b) {
    const float luma=r*0.2126f+g*0.7152f+b*0.0722f;
    r=luma+(r-luma)*1.10f;g=luma+(g-luma)*1.10f;b=luma+(b-luma)*1.10f;
    return {clampf((r-0.5f)*1.06f+0.5f,0.0f,1.0f),clampf((g-0.5f)*1.06f+0.5f,0.0f,1.0f),clampf((b-0.5f)*1.06f+0.5f,0.0f,1.0f)};
}
void gradedColor(float r,float g,float b,float a=1.0f){const Vec3 color=gradedSceneColor(r,g,b);glColor4f(color.x,color.y,color.z,a);}

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
    StaticModelData phone;
    StaticModelData flower;
    const bool phoneLoaded=phone.load((root/"phone.dbmesh").string());
    const bool flowerLoaded=flower.load((root/"flower.dbmesh").string());
    const bool humanLoaded=humanModel_.load((root/"human.dbhuman").string());
    phoneModelList_=phoneLoaded?compileStaticModel(phone):0; phoneShadowList_=phoneLoaded?compileStaticModel(phone,true):0;
    flowerModelList_=flowerLoaded?compileStaticModel(flower):0; flowerShadowList_=flowerLoaded?compileStaticModel(flower,true):0;
    std::printf("Pass 7 models: phone=%s flower=%s human=%s\n",phoneModelList_?"loaded":"fallback",flowerModelList_?"loaded":"fallback",humanLoaded?"loaded":"fallback");
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
    const auto floatingText=[&](const std::string& value,float x,float y,float scale,float r,float g,float b,float a=0.96f){const float pulse=clampf(state.cinematic.textInteraction,0.0f,1.0f),tracking=pulse*0.24f*std::min(scale,2.0f),wave=0.16f+pulse*0.18f;float pen=x-tracking*std::max(0.0f,(static_cast<float>(value.size())-1.0f)*0.5f);for(std::size_t i=0;i<value.size();++i){const float offset=std::sin(state.time*1.45f+static_cast<float>(i)*0.42f)*wave;const std::string glyph(1,value[i]);text(glyph,pen+1.0f,y+offset+1.0f,scale,0.0f,0.0f,0.0f,0.48f*a);text(glyph,pen,y+offset,scale,r,g,b,a);pen+=6.0f*scale+tracking;}};
    const auto rainbow=[&](float hue){hue-=std::floor(hue);const float x=hue*6.0f,i=std::floor(x),f=x-i,q=1.0f-f;switch(static_cast<int>(i)%6){case 0:return Vec3{1,f,0};case 1:return Vec3{q,1,0};case 2:return Vec3{0,1,f};case 3:return Vec3{0,q,1};case 4:return Vec3{f,0,1};default:return Vec3{1,0,q};}};
    const auto floatingPaletteText=[&](const std::string& value,float x,float y,float scale,float a=0.96f){const float pulse=clampf(state.cinematic.textInteraction,0.0f,1.0f),tracking=pulse*0.24f*std::min(scale,2.0f),wave=0.16f+pulse*0.18f;float pen=x-tracking*std::max(0.0f,(static_cast<float>(value.size())-1.0f)*0.5f);for(std::size_t i=0;i<value.size();++i){const float offset=std::sin(state.time*1.45f+static_cast<float>(i)*0.42f)*wave;const Vec3 color=rainbow(state.time*0.026f+static_cast<float>(i)*0.115f);const std::string glyph(1,value[i]);text(glyph,pen+1.0f,y+offset+1.0f,scale,0.0f,0.0f,0.0f,0.48f*a);text(glyph,pen,y+offset,scale,0.55f+color.x*0.42f,0.65f+color.y*0.34f,0.72f+color.z*0.28f,a);pen+=6.0f*scale+tracking;}};

    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0.0,static_cast<double>(width_),static_cast<double>(height_),0.0,-1.0,1.0);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    const float menuUiScale=clampf(std::min(static_cast<float>(width_)/1280.0f,static_cast<float>(height_)/720.0f),0.55f,1.8f);
    const float menuCanvasW=static_cast<float>(width_)/menuUiScale,menuCanvasH=static_cast<float>(height_)/menuUiScale;
    if(state.localSettings.fpsCounter){const std::string fps="FPS "+std::to_string(static_cast<int>(std::round(displayedFps)));text(fps,width_-fps.size()*7.2f-12,68,1.2f,0.72f,1.0f,0.90f);}

    if(state.cinematic.introActive){
        glPushMatrix();glScalef(menuUiScale,menuUiScale,1.0f);
        const float alpha=1.0f-smoothStep01(clampf(state.cinematic.introElapsed/0.42f,0.0f,1.0f));
        const float panelW=std::min(520.0f,menuCanvasW*0.72f),panelH=250.0f,panelX=(menuCanvasW-panelW)*0.5f,panelY=(menuCanvasH-panelH)*0.5f+20.0f;
        const std::string status="DATA";const float statusScale=3.0f,statusW=status.size()*6*statusScale;
        floatingPaletteText(status,panelX+(panelW-statusW)*0.5f,panelY+48,statusScale,0.96f*alpha);
        const std::string start="START";const float startScale=2.0f,startW=start.size()*6*startScale;
        floatingText(start,panelX+(panelW-startW)*0.5f,panelY+118,startScale,0.75f,0.96f,1.0f,alpha);
        glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);return;
    }

    const bool pausedSolo=state.started&&state.uiPaused&&!state.multiplayer.enabled&&!state.upgradeMenu.active;
    if(!state.started||pausedSolo) {
        glPushMatrix();glScalef(menuUiScale,menuUiScale,1.0f);
        const float panelW=std::min(520.0f,menuCanvasW*0.72f);
        const bool controlsPage=state.localSettings.menuPage==LocalMenuPage::Controls;
        const float panelH=controlsPage?560.0f:430.0f;
        const float panelX=(menuCanvasW-panelW)*0.5f;
        const float panelY=(menuCanvasH-panelH)*0.5f+(controlsPage?0.0f:24.0f);
        const float menuAlpha=state.dead?smoothStep01(clampf(state.cinematic.deathElapsed/0.45f,0.0f,1.0f)):1.0f;
        const std::string status=state.dead?"":pausedSolo&&state.localSettings.menuPage==LocalMenuPage::Main?"PAUSED":state.localSettings.menuPage==LocalMenuPage::Main?"DATA":state.localSettings.menuPage==LocalMenuPage::Online?"ONLINE":state.localSettings.menuPage==LocalMenuPage::JoinCode?"ENTER CODE":state.localSettings.menuPage==LocalMenuPage::Settings?"SETTINGS":state.localSettings.menuPage==LocalMenuPage::Controls?"CONTROLS":state.localSettings.menuPage==LocalMenuPage::Audio?"AUDIO":"GRAPHICS";
        const float statusScale=3.0f,statusW=status.size()*6*statusScale;
        if(!status.empty()){const bool title=state.localSettings.menuPage==LocalMenuPage::Main&&!pausedSolo;if(title)floatingPaletteText(status,panelX+(panelW-statusW)*0.5f,panelY+48,statusScale,0.96f*menuAlpha);else floatingText(status,panelX+(panelW-statusW)*0.5f,panelY+48,statusScale,0.92f,0.97f,1.0f,0.96f*menuAlpha);}
        const std::string networkStatus=state.multiplayer.status[0]?state.multiplayer.status.data():"";
        const std::string room=state.multiplayer.roomCode[0]?state.multiplayer.roomCode.data():"";
        const float buttonW=std::min(360.0f,panelW-48.0f);const auto menuText=[&](int item,const std::string& label,float y,float rowH=44.0f,float scale=2.25f){const bool selected=state.hud.menuSelection==item;const float breath=selected?(0.5f+0.5f*std::sin(state.time*1.18f+item*0.11f)):0.0f,cx=panelX+panelW*0.5f;rotatedQuad(cx,y+rowH*0.5f,buttonW,rowH,0,selected?0.12f:0.02f,selected?0.72f:0.08f,selected?0.82f:0.11f,(selected?0.12f+breath*0.025f:0.075f)*menuAlpha);rotatedQuad(cx,y+rowH-1,buttonW-24,1,0,0.62f,0.96f,1.0f,(selected?0.48f+breath*0.10f:0.22f)*menuAlpha);const float tw=label.size()*6.0f*scale;floatingText(label,cx-tw*0.5f,y+(rowH-7*scale)*0.5f,scale,selected?0.96f:0.75f,selected?1.0f:0.96f,1.0f,menuAlpha);};
        std::vector<std::string> items;
        if(state.dead)items={"RESTART","EXIT"};
        else if(pausedSolo&&state.localSettings.menuPage==LocalMenuPage::Main)items={"RESUME","CONTROLS","AUDIO","GRAPHICS","EXIT RUN"};
        else if(state.localSettings.menuPage==LocalMenuPage::Main)items={"SOLO","ONLINE","SETTINGS","EXIT"};
        else if(state.localSettings.menuPage==LocalMenuPage::Online)items={"HOST","JOIN","BACK"};
        else if(state.localSettings.menuPage==LocalMenuPage::JoinCode)items={};
        else if(state.localSettings.menuPage==LocalMenuPage::Settings)items={"CONTROLS","AUDIO","GRAPHICS","BACK"};
        else if(state.localSettings.menuPage==LocalMenuPage::Controls){const char* actions[]={"FORWARD","BACK","LEFT","RIGHT","RUN","JUMP","LUNGE","SHOOT","CAMERA","ALT"};const auto keyName=[](int key){if(key>=65&&key<=90)return std::string(1,static_cast<char>(key));if(key>=48&&key<=57)return std::string(1,static_cast<char>(key));if(key==32)return std::string("SPACE");if(key==340)return std::string("SHIFT");return std::string("KEY ")+std::to_string(key);};for(int i=0;i<10;++i)items.push_back(std::string(actions[i])+"   "+keyName(state.localSettings.keyboardBindings[i]));items.push_back("MOUSE LOOK  "+std::to_string(static_cast<int>(std::round(state.localSettings.mouseLookSensitivity*100)))+"%");items.push_back("PAD LOOK  "+std::to_string(static_cast<int>(std::round(state.localSettings.controllerLookSensitivity*100)))+"%");items.push_back("DEFAULTS");items.push_back("BACK");}
        else if(state.localSettings.menuPage==LocalMenuPage::Audio){items={"MUSIC  "+std::to_string(static_cast<int>(std::round(state.localSettings.musicVolume*100)))+"%","SFX  "+std::to_string(static_cast<int>(std::round(state.localSettings.sfxVolume*100)))+"%",state.localSettings.musicMuted?"MUSIC ON":"MUSIC MUTE",state.localSettings.sfxMuted?"SFX ON":"SFX MUTE","BACK"};}
        else {const char* presets[]={"LEGACY","NORMAL","PRETTY"};items={std::string("PRESET  ")+presets[std::max(0,std::min(2,state.localSettings.graphicsPreset))],state.localSettings.shadows?"SHADOWS ON":"SHADOWS OFF",state.localSettings.particles?"PARTICLES ON":"PARTICLES OFF",state.localSettings.fpsCounter?"FPS ON":"FPS OFF","BACK"};}
        for(int i=0;i<static_cast<int>(items.size());++i)menuText(i,items[i],panelY+82+i*(controlsPage?32.0f:52.0f),controlsPage?27.0f:44.0f,controlsPage?1.35f:2.25f);
        if(state.localSettings.menuPage==LocalMenuPage::JoinCode){std::string typed;for(int i=0;i<6;++i){typed+=i<static_cast<int>(room.size())?room[i]:'_';if(i<5)typed+=' ';}const float scale=3.0f,tw=typed.size()*6*scale;floatingText(typed,panelX+(panelW-tw)*0.5f,panelY+142,scale,0.88f,1.0f,1.0f,menuAlpha);floatingText("ESC BACK",panelX+panelW*0.5f-48,panelY+224,1.2f,0.62f,0.90f,0.94f,0.72f*menuAlpha);}
        if(controlsPage&&state.localSettings.rebindingAction>=0){const std::string prompt="PRESS NEW KEY   ESC CANCEL";const float tw=prompt.size()*6.0f*1.15f;floatingText(prompt,panelX+(panelW-tw)*0.5f,panelY+520,1.15f,0.82f,1.0f,1.0f,menuAlpha);}
        if(!networkStatus.empty()&&state.localSettings.menuPage==LocalMenuPage::Online){const float tw=networkStatus.size()*6.0f*1.2f;floatingText(networkStatus,panelX+(panelW-tw)*0.5f,panelY+274,1.2f,0.65f,1.0f,0.88f*menuAlpha);}
        glPopMatrix();glMatrixMode(GL_MODELVIEW); glPopMatrix();
        glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
        return;
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

    // Stored-soul window. The browser exposes 18 visual cells even though storage is larger.
    quad(12,74,120,82,0.005f,0.012f,0.016f,0.72f);
    quad(12,74,120,1,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.72f); quad(12,155,120,1,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.72f);
    quad(12,74,1,82,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.72f); quad(131,74,1,82,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.72f);
    const int filledSoulPixels=state.hud.storedSouls<=0?0:std::max(1,static_cast<int>(std::ceil(state.hud.storedSouls/30.0f*18.0f)));
    const Vec3 hudRight{std::cos(state.camera.yaw),0,-std::sin(state.camera.yaw)}, hudForward{-std::sin(state.camera.yaw),0,-std::cos(state.camera.yaw)};
    const float lateral=state.player.vel.x*hudRight.x+state.player.vel.z*hudRight.z,forward=state.player.vel.x*hudForward.x+state.player.vel.z*hudForward.z;
    const float inertiaX=clampf(-lateral*1.35f,-7.0f,7.0f);
    const float inertiaY=clampf(forward*0.75f-state.player.jumpVel*0.12f,-5.0f,5.0f);
    for(int i=0;i<filledSoulPixels;++i) {
        const float angle=i/18.0f*PI*2.0f,ring=0.38f+((i*7)%10)/26.0f;
        const float sx=34.0f*ring*std::cos(angle)+((i*13)%7-3),sy=14.0f*ring*std::sin(angle)+((i*17)%5-2);
        const float dx=((i*11)%9-4)*0.9f,dy=((i*19)%7-3)*0.8f;
        const float drift=0.5f+0.5f*std::sin(state.time*(1.35f+i%6*0.08f)-i*0.41f);
        const float x=67.0f+sx+dx*drift+inertiaX, y=119.0f+sy+dy*drift+inertiaY;
        const float hue=static_cast<float>((i*5)%18)/18.0f;const Vec3 soulHue=rainbow(hue+state.time*0.025f);
        quad(x,y,6.0f,6.0f,soulHue.x,soulHue.y,soulHue.z,0.88f);
    }
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
    if(state.hud.headshotPulse>0.001f){const float charge=clampf(state.hud.headshotKillCharge,0,1),a=state.hud.headshotPulse*(0.18f+charge*0.34f),p=state.hud.perfectPulse,w=static_cast<float>(width_),h=static_cast<float>(height_),edge=2+p*2+charge*2;const Vec3 color=rainbow(0.51f+charge*0.40f+state.progression.run.accuracyStacks*0.012f);quad(0,0,w,edge,color.x,color.y,color.z,a);quad(0,h-edge,w,edge,color.x,color.y,color.z,a);quad(0,0,edge,h,color.x,color.y,color.z,a);quad(w-edge,0,edge,h,color.x,color.y,color.z,a);}

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
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
        drawBox({14.73f-push,3.38f,4.80f},{0.07f+push*0.42f,1.38f+breathe+knock*0.10f,1.34f+breathe+knock*0.08f},0,0,0,Pass7Visual::TvMembrane.r,Pass7Visual::TvMembrane.g,Pass7Visual::TvMembrane.b,alpha);
        if(knock>0.025f)drawBox({14.66f-push*1.5f,3.38f,4.80f},{0.025f,1.10f+knock*0.24f,1.04f+knock*0.20f},0,0,0,Pass7Visual::ElectricCyan.r,Pass7Visual::ElectricCyan.g,Pass7Visual::ElectricCyan.b,0.18f*knock);
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
        const float fullness=clampf(static_cast<float>(state.secretTv.signal)/24.0f,0.0f,1.0f);
        if(state.secretTv.broken){
            const float staticValue=0.18f+0.16f*std::abs(std::sin(state.time*47.0f+state.secretTv.signal*1.7f));
            drawBox({41.82f,0.78f,0},{0.035f,0.86f,1.22f},0,-1.5708f,0,staticValue,staticValue+0.035f,staticValue+0.045f);
        }else{
            const float cellY=0.86f/TvGifWall::Rows,cellZ=1.22f/TvGifWall::Columns;
            for(int row=0;row<TvGifWall::Rows;++row)for(int col=0;col<TvGifWall::Columns;++col){
                const auto color=tvGifWall_.sample(col,row,state.time,state.secretTv.signal);
                const float magnetic=phoneProximity*(0.010f+0.018f*(1.0f-fullness)),phase=state.time*5.1f+row*0.83f+col*0.29f;
                const float yWarp=std::sin(phase)*magnetic,zWarp=std::sin(phase*0.63f+row)*magnetic*1.6f;
                const float clarity=0.62f+0.38f*fullness,flicker=1.0f-(1.0f-fullness)*phoneProximity*(0.05f+0.05f*std::sin(state.time*17.0f+row));
                drawBox({41.805f,0.78f-0.43f+cellY*(row+0.5f)+yWarp,-0.61f+cellZ*(col+0.5f)+zWarp},{0.038f,cellY*1.04f,cellZ*1.04f},0,-1.5708f,0,color.r*clarity*flicker,color.g*clarity*flicker,color.b*clarity*flicker);
            }
        }
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
