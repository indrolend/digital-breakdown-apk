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
#include <cstdio>
#include <string>

namespace {
constexpr float ROOM_WIDTH = 30.0f;
constexpr float ROOM_DEPTH = 42.0f;
constexpr int ROOM_VISUAL_HORIZON = 2;
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
        if(shadow) glColor4f(0.012f,0.018f,0.022f,0.28f); else glColor4fv(batch.color);
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
    glScalef(s.x, s.y, s.z); glColor4f(r,g,b,a); cube(); glPopMatrix();
}

void fxRibbon(const Vec3& p,const Quat& q,const Vec3& scale,float start,float sweep,int segments,float inner,float outer,float r,float g,float b,float a){
    const float matrix[16]={1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y+q.z*q.w),2*(q.x*q.z-q.y*q.w),0,2*(q.x*q.y-q.z*q.w),1-2*(q.x*q.x+q.z*q.z),2*(q.y*q.z+q.x*q.w),0,2*(q.x*q.z+q.y*q.w),2*(q.y*q.z-q.x*q.w),1-2*(q.x*q.x+q.y*q.y),0,0,0,0,1};
    glPushMatrix(); glTranslatef(p.x,p.y,p.z); glMultMatrixf(matrix); glScalef(scale.x,scale.y,scale.z); glColor4f(r,g,b,a);
    glBegin(GL_TRIANGLE_STRIP); for(int i=0;i<=segments;++i){const float angle=start+sweep*static_cast<float>(i)/segments; const float c=std::cos(angle),s=std::sin(angle); glVertex3f(c*outer,s*outer,0); glVertex3f(c*inner,s*inner,0);} glEnd(); glPopMatrix();
}

void fxStreak(const Vec3& p,const Quat& q,float length,float width,float r,float g,float b,float a){
    constexpr int segments=12; const float matrix[16]={1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y+q.z*q.w),2*(q.x*q.z-q.y*q.w),0,2*(q.x*q.y-q.z*q.w),1-2*(q.x*q.x+q.z*q.z),2*(q.y*q.z+q.x*q.w),0,2*(q.x*q.z+q.y*q.w),2*(q.y*q.z-q.x*q.w),1-2*(q.x*q.x+q.y*q.y),0,0,0,0,1};
    glPushMatrix(); glTranslatef(p.x,p.y,p.z); glMultMatrixf(matrix); glColor4f(r,g,b,a); glBegin(GL_TRIANGLE_STRIP);
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
    glColor3f(r,g,b); cube(); glPopMatrix();
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
    const Quat rootQ=quaternionFromEulerXYZ(target.attackTimer>0?-strike*(0.18f+low*0.06f)+windup*0.10f:0,target.visualYaw+PI,target.attackTimer>0?side*(strike*0.30f-windup*0.20f):0);
    const Vec3 root{target.pos.x,target.attackTimer>0?std::sin(attackT*PI)*0.035f*low:0,target.pos.z};
    const float matrix[16]={1-2*(rootQ.y*rootQ.y+rootQ.z*rootQ.z),2*(rootQ.x*rootQ.y+rootQ.z*rootQ.w),2*(rootQ.x*rootQ.z-rootQ.y*rootQ.w),0,2*(rootQ.x*rootQ.y-rootQ.z*rootQ.w),1-2*(rootQ.x*rootQ.x+rootQ.z*rootQ.z),2*(rootQ.y*rootQ.z+rootQ.x*rootQ.w),0,2*(rootQ.x*rootQ.z+rootQ.y*rootQ.w),2*(rootQ.y*rootQ.z-rootQ.x*rootQ.w),1-2*(rootQ.x*rootQ.x+rootQ.y*rootQ.y),0,0,0,0,1};
    glPushMatrix();glTranslatef(root.x,root.y,root.z);glMultMatrixf(matrix);glScalef(pose.scale,pose.scale,pose.scale);if(shadow)glColor4f(0.012f,0.018f,0.022f,0.28f);else glColor4fv(humanModel_.color);glBegin(GL_TRIANGLES);
    const float thinning=humanShellThinningAmount(target.armor,target.brute?4.0f:2.0f,target.slurpable);
    for(std::size_t i=0;i+8<humanVertices_.size();i+=9){if(humanShellTriangleMissing(i/9,thinning))continue;const Vec3 a{humanVertices_[i],humanVertices_[i+1],humanVertices_[i+2]},b{humanVertices_[i+3],humanVertices_[i+4],humanVertices_[i+5]},c{humanVertices_[i+6],humanVertices_[i+7],humanVertices_[i+8]},n=normalized(cross3(b-a,c-a));glNormal3f(n.x,n.y,n.z);glVertex3f(a.x,a.y,a.z);glVertex3f(b.x,b.y,b.z);glVertex3f(c.x,c.y,c.z);}glEnd();glPopMatrix();
}

void DesktopRenderer::drawSoulFlesh(const TargetState& target,const Vec3& center){
    auto index=[](int x,int y,int z){return x+y*3+z*9;};
    auto emitQuad=[&](int ia,int ib,int ic,int id){const Vec3 a=center+target.latticeSurfacePos[ia],b=center+target.latticeSurfacePos[ib],c=center+target.latticeSurfacePos[ic],d=center+target.latticeSurfacePos[id];Vec3 n=normalized(cross3(b-a,c-a));glNormal3f(n.x,n.y,n.z);glVertex3f(a.x,a.y,a.z);glVertex3f(b.x,b.y,b.z);glVertex3f(c.x,c.y,c.z);n=normalized(cross3(c-a,d-a));glNormal3f(n.x,n.y,n.z);glVertex3f(a.x,a.y,a.z);glVertex3f(c.x,c.y,c.z);glVertex3f(d.x,d.y,d.z);};
    glColor3f(224.0f/255.0f,160.0f/255.0f,143.0f/255.0f);glBegin(GL_TRIANGLES);
    for(int y=0;y<2;++y)for(int z=0;z<2;++z){emitQuad(index(0,y,z),index(0,y+1,z),index(0,y+1,z+1),index(0,y,z+1));emitQuad(index(2,y,z),index(2,y,z+1),index(2,y+1,z+1),index(2,y+1,z));}
    for(int x=0;x<2;++x)for(int z=0;z<2;++z){emitQuad(index(x,0,z),index(x,0,z+1),index(x+1,0,z+1),index(x+1,0,z));emitQuad(index(x,2,z),index(x+1,2,z),index(x+1,2,z+1),index(x,2,z+1));}
    for(int x=0;x<2;++x)for(int y=0;y<2;++y){emitQuad(index(x,y,0),index(x+1,y,0),index(x+1,y+1,0),index(x,y+1,0));emitQuad(index(x,y,2),index(x,y+1,2),index(x+1,y+1,2),index(x+1,y,2));}
    glEnd();
    if(target.tetherVisible){const Vec3 endpoint=target.tetherAnchor;const Vec3 destination=target.tetherDestination;const Vec3 delta=destination-endpoint;const float len=length(delta);if(len>0.001f){const Vec3 mid=endpoint+delta*0.5f;const float yaw=std::atan2(delta.x,delta.z),pitch=-std::asin(clampf(delta.y/len,-1,1));glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);drawBox(mid,{0.13f*target.tetherWidth,0.13f*target.tetherWidth,std::min(len,4.5f)},pitch,yaw,0,1.0f,183.0f/255.0f,166.0f/255.0f,0.34f);glDepthMask(GL_TRUE);glDisable(GL_BLEND);}}
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
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); perspective(Pass7Visual::CameraVerticalFovDegrees, aspect, Pass7Visual::CameraNearPlane, Pass7Visual::CameraFarPlane);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); lookAt(state.camera.pos, state.camera.lookTarget, {0,1,0});
}

void DesktopRenderer::drawHud(const GameState& state) const {
    const auto quad=[](float x,float y,float w,float h,float r,float g,float b,float a) {
        glColor4f(r,g,b,a);
        glBegin(GL_QUADS);
        glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
        glEnd();
    };
    const auto rotatedQuad=[](float cx,float cy,float w,float h,float angle,float r,float g,float b,float a) {
        const float c=std::cos(angle),s=std::sin(angle),hx=w*0.5f,hy=h*0.5f;
        const Vec3 corners[4]={{-hx,-hy,0},{hx,-hy,0},{hx,hy,0},{-hx,hy,0}};
        glColor4f(r,g,b,a); glBegin(GL_QUADS);
        for(const Vec3& p:corners) glVertex2f(cx+p.x*c-p.y*s,cy+p.x*s+p.y*c);
        glEnd();
    };
    const auto text=[&](const std::string& value,float x,float y,float scale,float r=1.0f,float g=1.0f,float b=1.0f,float a=0.94f){
        float pen=x;for(char c:value){if(c==' '){pen+=6*scale;continue;}const auto rows=bitmapGlyph(c);for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1u<<(4-col)))quad(pen+col*scale,y+row*scale,scale,scale,r,g,b,a);pen+=6*scale;}
    };
    const auto floatingText=[&](const std::string& value,float x,float y,float scale,float r,float g,float b,float a=0.96f){const float pulse=clampf(state.cinematic.textInteraction,0.0f,1.0f),tracking=pulse*0.9f*std::min(scale,2.0f),wave=0.48f+pulse*0.78f;float pen=x-tracking*std::max(0.0f,(static_cast<float>(value.size())-1.0f)*0.5f);for(std::size_t i=0;i<value.size();++i){const float offset=std::sin(state.time*4.2f+static_cast<float>(i)*0.68f)*wave;const std::string glyph(1,value[i]);text(glyph,pen+2.0f,y+offset+2.0f,scale,0.0f,0.0f,0.0f,0.72f*a);text(glyph,pen,y+offset,scale,r,g,b,a);pen+=6.0f*scale+tracking;}};

    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0.0,static_cast<double>(width_),static_cast<double>(height_),0.0,-1.0,1.0);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

    if(state.cinematic.introActive){
        const float alpha=1.0f-smoothStep01(clampf(state.cinematic.introElapsed/0.42f,0.0f,1.0f));
        const float panelW=std::min(520.0f,static_cast<float>(width_)*0.72f),panelH=250.0f,panelX=(static_cast<float>(width_)-panelW)*0.5f,panelY=(static_cast<float>(height_)-panelH)*0.5f;
        const std::string status="READY";const float statusScale=3.0f,statusW=status.size()*6*statusScale;
        floatingText(status,panelX+(panelW-statusW)*0.5f,panelY+48,statusScale,0.92f,0.97f,1.0f,0.96f*alpha);
        floatingText("ENTER  SOLO",panelX+34,panelY+105,1.55f,0.75f,0.96f,1.0f,alpha);
        floatingText("H  HOST ONLINE",panelX+34,panelY+132,1.55f,0.75f,0.96f,1.0f,alpha);
        floatingText("J  JOIN ROOM",panelX+34,panelY+159,1.55f,0.75f,0.96f,1.0f,alpha);
        glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);return;
    }

    if(!state.started) {
        const float panelW=std::min(520.0f,static_cast<float>(width_)*0.72f);
        const float panelH=250.0f;
        const float panelX=(static_cast<float>(width_)-panelW)*0.5f;
        const float panelY=(static_cast<float>(height_)-panelH)*0.5f;
        const float menuAlpha=state.dead?smoothStep01(clampf(state.cinematic.deathElapsed/0.45f,0.0f,1.0f)):1.0f;
        const std::string status=state.dead?"AGAIN?":"READY";
        const float statusScale=3.0f,statusW=status.size()*6*statusScale;
        floatingText(status,panelX+(panelW-statusW)*0.5f,panelY+48,statusScale,0.92f,0.97f,1.0f,0.96f*menuAlpha);
        const std::string networkStatus=state.multiplayer.status[0]?state.multiplayer.status.data():"";
        const std::string room=state.multiplayer.roomCode[0]?state.multiplayer.roomCode.data():"";
        floatingText(state.dead?"ENTER / CLICK  RESTART":"ENTER  SOLO",panelX+34,panelY+105,1.55f,0.75f,0.96f,1.0f,menuAlpha);
        floatingText(state.dead?"ESC  EXIT":"H  HOST ONLINE",panelX+34,panelY+132,1.55f,0.75f,0.96f,1.0f,menuAlpha);
        if(!state.dead)floatingText("J  JOIN ROOM",panelX+34,panelY+159,1.55f,0.75f,0.96f,1.0f,menuAlpha);
        if(!networkStatus.empty())floatingText(networkStatus,panelX+34,panelY+196,1.35f,0.65f,1.0f,0.88f*menuAlpha);
        if(!room.empty())floatingText("CODE "+room,panelX+panelW-190,panelY+196,1.55f,1.0f,1.0f,menuAlpha);
        glMatrixMode(GL_MODELVIEW); glPopMatrix();
        glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
        return;
    }

    if(state.multiplayer.enabled){const std::string code=state.multiplayer.roomCode.data();const std::string net=state.multiplayer.status.data();const float w=std::max(150.0f,static_cast<float>(std::max(code.size()+5,net.size()))*7.2f+16.0f),x=width_-w-12.0f;quad(x,12,w,44,0.005f,0.012f,0.016f,0.72f);text(code.empty()?net:"ROOM "+code,x+8,20,1.35f,0.55f,0.95f,1.0f);if(!code.empty())text(net,x+8,38,1.0f,0.75f,0.90f,0.95f);}

    // Browser goal strip: five compact top-center receptacles.
    const int goalCount=std::max(1,state.hud.requiredGoals);
    const float goalSize=22.0f, goalGap=8.0f;
    const float goalsWidth=goalCount*goalSize+(goalCount-1)*goalGap;
    const float goalsX=(static_cast<float>(width_)-goalsWidth)*0.5f;
    for(int i=0;i<goalCount;++i) {
        const float x=goalsX+i*(goalSize+goalGap);
        quad(x,18,goalSize,goalSize,0.92f,0.97f,1.0f,0.92f);
        const bool filled=i<state.hud.filledGoals;
        quad(x+2,20,goalSize-4,goalSize-4,filled?0.18f:0.01f,filled?0.88f:0.02f,filled?1.0f:0.025f,filled?0.95f:0.88f);
    }

    // Stored-soul window. The browser exposes 18 visual cells even though storage is larger.
    quad(12,74,120,82,0.005f,0.012f,0.016f,0.72f);
    quad(12,74,120,1,0.50f,0.91f,1.0f,0.72f); quad(12,155,120,1,0.50f,0.91f,1.0f,0.72f);
    quad(12,74,1,82,0.50f,0.91f,1.0f,0.72f); quad(131,74,1,82,0.50f,0.91f,1.0f,0.72f);
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
        quad(x,y,6.0f,6.0f,0.80f,1.0f,1.0f,0.92f);
    }
    text("SOULS "+std::to_string(state.hud.storedSouls),20,80,1.2f,0.78f,0.94f,1.0f,0.82f);

    text("ROOM: "+std::to_string(state.roomIndex),12,170,1.5f);
    text("GOALS: "+std::to_string(state.hud.filledGoals)+"/"+std::to_string(state.hud.requiredGoals),12,187,1.5f);
    text(state.roomClear?"DOOR: OPEN":"DOOR: LOOP",12,204,1.5f,state.roomClear?0.72f:1.0f,1.0f,state.roomClear?0.74f:1.0f);
    text("TOKENS: "+std::to_string(state.progression.permanent.tokens),12,221,1.25f,0.82f,1.0f,0.91f);
    if(state.progression.run.accuracyStacks>0){char accuracy[32]{};std::snprintf(accuracy,sizeof(accuracy),"ACCURACY X%.2F",state.progression.run.accuracyMultiplier);text(accuracy,12,304,1.15f,0.72f,1.0f,0.86f);}
    if(state.hud.headshotPulse>0.001f){const float a=state.hud.headshotPulse*0.36f,p=state.hud.perfectPulse,w=static_cast<float>(width_),h=static_cast<float>(height_);quad(0,0,w,2+p*2,0.55f+p*0.45f,0.92f,1.0f,a);quad(0,h-2-p*2,w,2+p*2,0.55f+p*0.45f,0.92f,1.0f,a);quad(0,0,2+p*2,h,0.55f+p*0.45f,0.92f,1.0f,a);quad(w-2-p*2,0,2+p*2,h,0.55f+p*0.45f,0.92f,1.0f,a);}

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
    if(state.hud.energyTicker[0]&&state.time<state.hud.energyTickerUntil){const std::string ticker=state.hud.energyTicker.data();const float scale=1.35f,tw=ticker.size()*6*scale,pw=std::max(118.0f,tw+16.0f),px=(width_-pw)*0.5f;const int type=state.hud.energyTickerType;quad(px,72,pw,18,0,0,0,0.54f);quad(px,72,pw,1,type==1?1.0f:type==0?0.58f:1.0f,type==1?0.47f:type==0?1.0f:1.0f,type==1?0.35f:type==0?0.86f:1.0f,0.72f);text(ticker,(width_-tw)*0.5f,77,scale,type==1?1.0f:0.82f,type==1?0.69f:1.0f,type==1?0.62f:0.91f);}

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
        const float pw=std::min(620.0f,static_cast<float>(width_)-24.0f),ph=250.0f,px=(width_-pw)*0.5f,py=(height_-ph)*0.5f;
        quad(px,py,pw,ph,0,0,0,0.88f);text("ROUND "+std::to_string(state.roomIndex),px+18,py+16,2.0f);text("CHOOSE ONE RUN UPGRADE",px+18,py+42,1.25f,0.72f,1.0f,0.86f);
        text("1 SHOT",px+26,py+82,1.5f);text("2 LUNGE",px+220,py+82,1.5f);text("3 ATTACK",px+420,py+82,1.5f);
        text("PERMANENT SHOP  TOKENS "+std::to_string(state.progression.permanent.tokens),px+18,py+136,1.2f,0.82f,1.0f,0.91f);
        text("4 SHOT "+std::to_string(state.progression.permanent.levels[0])+"/5",px+26,py+174,1.25f);text("5 LUNGE "+std::to_string(state.progression.permanent.levels[1])+"/5",px+220,py+174,1.25f);text("6 ATTACK "+std::to_string(state.progression.permanent.levels[2])+"/5",px+420,py+174,1.25f);
        text("PERMANENT COST: 1 TOKEN",px+18,py+218,1.1f,0.72f,0.90f,1.0f);
    } else if(state.uiPaused){
        const float pw=320.0f,ph=230.0f,px=static_cast<float>(width_)-pw-12.0f,py=48.0f;
        quad(px,py,pw,ph,0,0,0,0.82f);quad(px,py,pw,1,1,1,1,0.55f);quad(px,py+ph-1,pw,1,1,1,1,0.55f);quad(px,py,1,ph,1,1,1,0.55f);quad(px+pw-1,py,1,ph,1,1,1,0.55f);
        text("PAUSED",px+12,py+12,2.0f);text("CLICK OR TAB TO RESUME",px+12,py+36,1.25f,0.75f,0.94f,1.0f);
        text("WASD  MOVE",px+12,py+72,1.25f);text("SHIFT RUN",px+12,py+90,1.25f);text("SPACE JUMP",px+12,py+108,1.25f);text("MOUSE CAMERA",px+12,py+126,1.25f);text("LEFT VACUUM",px+12,py+144,1.25f);text("RIGHT/F ATTACK",px+12,py+162,1.25f);text("Q SHOOT  C CAMERA",px+12,py+180,1.25f);
    }

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

    // Directional planar shadows: project each caster's real geometry along the
    // browser sun vector onto the floor. Silhouette, length and motion therefore
    // come from object vertices, height and light direction rather than blobs.
    const float shadowMatrix[16]={1,0,0,0,-0.5f,0,-25.0f/60.0f,0,0,0,1,0,0.006f,0.012f,0.005f,1};
    glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glPushMatrix();glMultMatrixf(shadowMatrix);
    if(!state.camera.firstPerson){if(phoneShadowList_)drawStaticModel(phoneShadowList_,state.phoneTransform.position,state.phoneVisual.bodyScale,state.phoneTransform.orientation);else drawBox(state.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},state.phoneTransform.orientation,0.012f,0.018f,0.022f);}
    if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.playerId!=state.multiplayer.localPlayerId&&peer.player.alive){if(phoneShadowList_)drawStaticModel(phoneShadowList_,peer.phoneTransform.position,peer.phoneVisual.bodyScale,peer.phoneTransform.orientation);else drawBox(peer.phoneTransform.position,{PHONE_BODY_WIDTH,PHONE_BODY_HEIGHT,PHONE_BODY_DEPTH},peer.phoneTransform.orientation,0.012f,0.018f,0.022f);}
    const float shadowTileOrigin=static_cast<float>(state.topology.currentTileIndex)*ROOM_DEPTH;
    for(int offset=-1;offset<=1;++offset)for(auto target:state.targets)if(target.alive){target.pos.z=shadowTileOrigin+static_cast<float>(offset)*ROOM_DEPTH+(target.pos.z-std::floor((target.pos.z+ROOM_DEPTH*0.5f)/ROOM_DEPTH)*ROOM_DEPTH);if(!target.slurpable){if(humanModel_.valid())drawHumanModel(target,state.time,true);}if(target.slurpable&&target.soulVisual.visible&&target.soulCubeAmount>0.001f){const Vec3 center=target.pos+Vec3{0,0.57f+target.soulVisual.verticalOffset,0};const float cubeSize=0.72f*0.78f*target.scale*target.soulVisual.morphScale;drawBox(center,{cubeSize*target.soulVisual.scale.x,cubeSize*target.soulVisual.scale.y,cubeSize*target.soulVisual.scale.z},0,target.soulVisual.rotationY,0,0.012f,0.018f,0.022f,0.28f);}}
    for(const auto& flower:state.flowers)if(flower.active){const Vec3 center{flower.pos.x,flower.pos.y,flower.pos.z+shadowTileOrigin};if(flowerShadowList_)drawStaticModel(flowerShadowList_,center,{1,1,1},quatAxisAngle({0,1,0},flower.rotationY));}
    for(const auto& bullet:state.bullets)if(bullet.alive){const float size=0.72f*1.12f*(bullet.brute?1.7f:1.0f);drawBox(bullet.pos,{size,size,size},bullet.spin*1.2f,bullet.spin*1.7f,bullet.spin*0.9f,0.012f,0.018f,0.022f,0.24f);}
    for(int i=0;i<state.debug.colliderCount;++i){const auto& c=state.roomColliders[i];drawBox({c.center.x,c.center.y,shadowTileOrigin+c.center.z},{c.width,c.height,c.depth},0,0,0,0.012f,0.018f,0.022f,0.20f);}
    glPopMatrix();glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_LIGHTING);

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
        fxRibbon(melee.origin+melee.direction*(0.66f+0.22f*t),slashQ,{(0.75f+t*0.72f)*hitBoost,(0.75f+t*0.72f)*hitBoost,1},0,PI*1.35f,24,0.494f,0.546f,0.56f,0.97f,1,fade*0.72f);
        const Vec3 delta=melee.impact-melee.origin; const float len=std::max(0.3f,length(delta));
        const Vec3 mid=melee.origin+delta*0.46f; const float yaw=std::atan2(delta.x,delta.z); const float pitch=-std::asin(clampf(delta.y/std::max(len,0.001f),-1.0f,1.0f));
        const Quat streakQ=quatAxisAngle({0,1,0},yaw)*quatAxisAngle({1,0,0},pitch);
        fxStreak(mid,streakQ,len*(0.70f+std::sin(t*PI)*0.18f),0.11f*(1+std::sin(t*PI)*1.2f),0.33f,0.84f,1,fade*0.42f);
        fxRibbon(melee.impact,{}, {(0.45f+t*1.45f)*hitBoost,(0.45f+t*1.45f)*hitBoost,1},0,PI*2,24,0.318f,0.362f,1,1,1,melee.visualHit?fade*0.9f:fade*0.26f);
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
        drawBox(p+Vec3{0,0,-0.04f},{0.72f,0.72f,0.06f},0,0,0,0.36f,0.42f,0.46f);
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
    for(const auto& particle:state.particles) if(particle.life>0.0f) {
        const float t=particle.maxLife>0.0f?clampf(particle.life/particle.maxLife,0.0f,1.0f):0.0f;
        const float size=particle.size*t;
        if(particle.kind==1)drawBox(particle.pos,{size,size,size},particle.life*8.0f,particle.life*4.0f,particle.life*6.0f,0.16f,0.39f,0.42f,0.82f*t);
        else drawBox(particle.pos,{size,size,size},particle.life*8.0f,particle.life*4.0f,particle.life*6.0f,1.0f,0.267f,0.267f,0.9f);
    }
    drawDoorDataMosh(state);
    drawHud(state);
    glFlush();
}
