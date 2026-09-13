#pragma once

#include <array>
#include <algorithm>
#include <cmath>

#include "Math.hpp"

namespace rolling_vehicle {

constexpr int WheelCount = 4;

struct Config {
    float halfWidth = 0.43f;
    float halfLength = 0.62f;
    float wheelBase = 1.02f;
    float wheelRadius = 0.12f;
    float rideHeight = 0.16f;
    float maxForwardSpeed = 7.0f;
    float maxReverseSpeed = 3.0f;
    float engineAcceleration = 7.5f;
    float reverseAcceleration = 4.2f;
    float brakeAcceleration = 12.0f;
    float rollingResistance = 0.72f;
    float aerodynamicDrag = 0.055f;
    float lateralGrip = 8.5f;
    float yawResponse = 8.0f;
    float maxYawRate = 2.25f;
    float gravity = 9.81f;
};

inline constexpr Config CartConfig{};

struct Input { float throttle=0.0f; float steer=0.0f; float brake=0.0f; };
struct Probe { bool supported=false; float height=0.0f; Vec3 normal{0,1,0}; };

struct State {
    Vec3 pos{};
    Vec3 vel{};
    float yaw=0.0f;
    float yawRate=0.0f;
    bool active=false;
    bool occupied=false;
    int driverPlayerId=-1;
    int contactCount=0;
    float supportHeight=0.0f;
    Vec3 supportNormal{0,1,0};
    std::array<float,WheelCount> wheelSpin{};
    std::array<float,WheelCount> casterYaw{};
    std::array<float,WheelCount> contactCompression{};
};

inline Vec3 forward(float yaw){return {std::sin(yaw),0.0f,-std::cos(yaw)};}
inline Vec3 right(float yaw){return {std::cos(yaw),0.0f,std::sin(yaw)};}
inline Vec3 wheelAnchor(int index,const Config& c=CartConfig){return {(index&1)?c.halfWidth:-c.halfWidth,0.0f,index<2?-c.halfLength:c.halfLength};}
inline Vec3 worldAnchor(const State& s,int index,const Config& c=CartConfig){const Vec3 a=wheelAnchor(index,c);return s.pos+right(s.yaw)*a.x-forward(s.yaw)*a.z;}
inline float signedApproach(float value,float target,float amount){const float d=target-value;if(std::abs(d)<=amount)return target;return value+(d>0.0f?amount:-amount);}
inline float maxSteer(float speed){const float t=clampf(speed/8.0f,0.0f,1.0f);return (42.0f+(15.0f-42.0f)*t)*(DB_PI/180.0f);}

inline bool reduceSupport(const std::array<Probe,WheelCount>& probes,float& height,Vec3& normal,int& count){
    std::array<float,WheelCount> h{}; normal={}; count=0;
    for(const auto& p:probes)if(p.supported){h[count++]=p.height;normal+=p.normal;}
    if(count<2){normal={0,1,0};return false;}
    std::sort(h.begin(),h.begin()+count);
    if(count==4)height=(h[1]+h[2])*0.5f;
    else if(count==3)height=h[1];
    else height=std::abs(h[1]-h[0])<=0.65f?(h[0]+h[1])*0.5f:std::min(h[0],h[1]);
    normal=normalized(normal);if(normal.y<0.2f)normal={0,1,0};return true;
}

inline void update(State& s,const Input& raw,const std::array<Probe,WheelCount>& probes,float dt,const Config& c=CartConfig){
    if(!s.active||dt<=0.0f)return;dt=clampf(dt,0.0f,1.0f/30.0f);
    Input in{clampf(raw.throttle,-1,1),clampf(raw.steer,-1,1),clampf(raw.brake,0,1)};
    float support=0;Vec3 normal;int contacts=0;const bool grounded=reduceSupport(probes,support,normal,contacts);
    s.contactCount=contacts;s.supportNormal=normal;
    if(grounded){s.supportHeight=support;s.pos.y=support+c.rideHeight;s.vel.y=0;}
    else {s.vel.y=std::max(-12.0f,s.vel.y-c.gravity*dt);s.pos.y+=s.vel.y*dt;}
    Vec3 f=forward(s.yaw),r=right(s.yaw);float longitudinal=dot3(s.vel,f),lateral=dot3(s.vel,r);
    float acceleration=0.0f;
    if(in.brake>0.0f)acceleration=-std::copysign(c.brakeAcceleration*in.brake,longitudinal==0.0f?1.0f:longitudinal);
    else if(in.throttle>0.0f)acceleration=longitudinal<-0.12f?c.brakeAcceleration:c.engineAcceleration*in.throttle;
    else if(in.throttle<0.0f)acceleration=longitudinal>0.12f?-c.brakeAcceleration:c.reverseAcceleration*in.throttle;
    if(std::abs(in.throttle)<0.001f&&in.brake<0.001f)acceleration-=std::copysign(std::min(c.rollingResistance,std::abs(longitudinal)/dt),longitudinal);
    acceleration-=c.aerodynamicDrag*longitudinal*std::abs(longitudinal);
    if(grounded){const Vec3 downhill={normal.x*normal.y*c.gravity,0,normal.z*normal.y*c.gravity};acceleration+=dot3(downhill,f);lateral+=dot3(downhill,r)*dt;}
    longitudinal=clampf(longitudinal+clampf(acceleration,-c.brakeAcceleration,c.brakeAcceleration)*dt,-c.maxReverseSpeed,c.maxForwardSpeed);
    lateral*=std::exp(-c.lateralGrip*(static_cast<float>(contacts)/WheelCount)*dt);
    const float targetYaw=grounded?clampf(longitudinal*std::tan(in.steer*maxSteer(std::abs(longitudinal)))/c.wheelBase,-c.maxYawRate,c.maxYawRate):0.0f;
    s.yawRate+= (targetYaw-s.yawRate)*(1.0f-std::exp(-c.yawResponse*dt));s.yawRate=clampf(s.yawRate,-c.maxYawRate,c.maxYawRate);s.yaw+=s.yawRate*dt;
    f=forward(s.yaw);r=right(s.yaw);s.vel.x=f.x*longitudinal+r.x*lateral;s.vel.z=f.z*longitudinal+r.z*lateral;s.pos.x+=s.vel.x*dt;s.pos.z+=s.vel.z*dt;
    for(int i=0;i<WheelCount;++i){const Vec3 a=wheelAnchor(i,c);const Vec3 pointVelocity=s.vel+r*(s.yawRate*(-a.z))+f*(s.yawRate*a.x);const float speed=horizontalLength(pointVelocity);if(speed>0.08f){const float desired=std::atan2(dot3(pointVelocity,r),dot3(pointVelocity,f));const float delta=std::atan2(std::sin(desired-s.casterYaw[i]),std::cos(desired-s.casterYaw[i]));s.casterYaw[i]+=delta*(1.0f-std::exp(-12.0f*dt));}s.wheelSpin[i]+=dot3(pointVelocity,f)/c.wheelRadius*dt;s.contactCompression[i]=probes[i].supported?clampf((support-probes[i].height+0.12f)/0.24f,0,1):0;}
}

} // namespace rolling_vehicle
