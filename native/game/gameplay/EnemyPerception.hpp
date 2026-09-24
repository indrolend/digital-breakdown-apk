#pragma once

#include <algorithm>
#include <cmath>

#include "Math.hpp"

namespace gameplay {

struct EnemyPerceptionState {
    Vec3 lastSeenPosition{};
    Vec3 lastSeenVelocity{};
    float confidence = 0.0f;
    float uncertainty = 1.0f;
    float headYaw = 0.0f;
    float headPitch = 0.0f;
    float searchPhase = 0.0f;
    bool confirmed = false;
};

struct EnemyPerceptionInput {
    Vec3 observerPosition{};
    Vec3 targetPosition{};       // Read only when sampled and visibly supported.
    Vec3 targetVelocity{};       // Read only when sampled and visibly supported.
    float bodyYaw = 0.0f;
    float bodyPitch = 0.0f;
    float bodyRoll = 0.0f;
    float visibility = 0.0f;     // 0 hard-blocked, (0,1) attenuated, 1 clear.
    float vagueAwareness = 0.0f; // Directionless suspicion only.
    Vec3 environmentalCuePosition{}; // Imprecise event location, never player truth.
    float environmentalCueStrength = 0.0f;
    float roomMemoryPressure = 0.0f; // Existing room history: persistence, never evidence.
    float individuality = 0.0f;
    float dt = 0.0f;
    bool sampled = false;
};

struct EnemyPerceptionOutput {
    Vec3 believedPosition{};
    Vec3 perceivedVelocity{};
    Vec3 headDirection{0.0f,0.0f,-1.0f};
    float confidence = 0.0f;
    float uncertainty = 1.0f;
    float headYaw = 0.0f;
    float headPitch = 0.0f;
    bool confirmed = false;
    bool hasSpatialBelief = false;
};

inline float finitePerceptionValue(float value,float fallback=0.0f){return std::isfinite(value)?value:fallback;}
inline Vec3 finitePerceptionVector(const Vec3& value){
    return {finitePerceptionValue(value.x),finitePerceptionValue(value.y),finitePerceptionValue(value.z)};
}
inline float wrapPerceptionAngle(float angle){
    angle=finitePerceptionValue(angle);
    while(angle>DB_PI)angle-=DB_PI*2.0f;
    while(angle<-DB_PI)angle+=DB_PI*2.0f;
    return angle;
}

// Returns the segment entry fraction for an axis-aligned volume. The caller
// decides whether that volume is opaque or merely attenuating.
inline bool perceptionSegmentHitsBox(const Vec3& start,const Vec3& end,const Vec3& minimum,const Vec3& maximum,float& entry){
    const Vec3 a=finitePerceptionVector(start),b=finitePerceptionVector(end),delta=b-a;
    float nearT=0.0f,farT=1.0f;
    const float origins[3]={a.x,a.y,a.z},directions[3]={delta.x,delta.y,delta.z};
    const float minima[3]={minimum.x,minimum.y,minimum.z},maxima[3]={maximum.x,maximum.y,maximum.z};
    for(int axis=0;axis<3;++axis){
        if(std::abs(directions[axis])<0.00001f){if(origins[axis]<minima[axis]||origins[axis]>maxima[axis])return false;continue;}
        float lo=(minima[axis]-origins[axis])/directions[axis],hi=(maxima[axis]-origins[axis])/directions[axis];
        if(lo>hi)std::swap(lo,hi);
        nearT=std::max(nearT,lo);farT=std::min(farT,hi);
        if(nearT>farT)return false;
    }
    entry=nearT;
    return farT>=0.0f&&nearT<=1.0f;
}

inline float environmentalCueTransmission(float rain,bool exposed){
    if(!exposed)return 1.0f;
    return 1.0f-std::max(0.0f,std::min(1.0f,finitePerceptionValue(rain)))*0.22f;
}

inline float movementAwarenessStrength(float distance,float speed,float transmission,float surfaceTransmission=1.0f,float groundContact=1.0f){
    distance=std::max(0.0f,finitePerceptionValue(distance,1000.0f));
    speed=std::max(0.0f,std::min(12.0f,finitePerceptionValue(speed)));
    transmission=std::max(0.0f,std::min(1.0f,finitePerceptionValue(transmission)));
    surfaceTransmission=std::max(0.0f,std::min(1.0f,finitePerceptionValue(surfaceTransmission,1.0f)));
    groundContact=std::max(0.0f,std::min(1.0f,finitePerceptionValue(groundContact,1.0f)));
    // Nearby grounded body motion is audible/tactile evidence, not a coordinate. Slow
    // movement remains subtle while committed running carries farther.
    const float motion=std::min(1.0f,speed/6.0f);
    const float range=2.2f+motion*3.8f;
    return motion*(1.0f-std::min(1.0f,distance/range))*0.52f*transmission*surfaceTransmission*groundContact;
}

inline float landingAwarenessStrength(float distance,float impactSpeed,float transmission,float surfaceTransmission=1.0f){
    distance=std::max(0.0f,finitePerceptionValue(distance,1000.0f));
    impactSpeed=std::max(0.0f,std::min(12.0f,finitePerceptionValue(impactSpeed)));
    transmission=std::max(0.0f,std::min(1.0f,finitePerceptionValue(transmission)));
    surfaceTransmission=std::max(0.0f,std::min(1.0f,finitePerceptionValue(surfaceTransmission,1.0f)));
    // Landing is a short physical event: harder impacts travel farther through the
    // same floor/weather authority as grounded movement. It is evidence at the
    // contact point, never authoritative player tracking.
    const float impact=std::max(0.0f,std::min(1.0f,(impactSpeed-1.4f)/5.0f));
    const float range=2.0f+impact*5.0f;
    return impact*(1.0f-std::min(1.0f,distance/range))*0.68f*transmission*surfaceTransmission;
}

inline float physicalHerdCueStrength(float distance,float activity,float physicalDisruption,float transmission){
    distance=std::max(0.0f,finitePerceptionValue(distance,1000.0f));
    activity=std::max(0.0f,std::min(1.0f,finitePerceptionValue(activity)));
    physicalDisruption=std::max(0.0f,std::min(1.0f,finitePerceptionValue(physicalDisruption)));
    transmission=std::max(0.0f,std::min(1.0f,finitePerceptionValue(transmission)));
    const float contactAlarm=physicalDisruption*0.42f;
    const float source=std::max(activity,contactAlarm);
    return source*(1.0f-std::min(1.0f,distance/7.0f))*transmission;
}

inline float visualAcquisitionStrength(float distance,float forwardDot,float targetSpeed,float transmission){
    distance=std::max(0.0f,finitePerceptionValue(distance,1000.0f));
    forwardDot=std::max(-1.0f,std::min(1.0f,finitePerceptionValue(forwardDot,-1.0f)));
    targetSpeed=std::max(0.0f,std::min(12.0f,finitePerceptionValue(targetSpeed)));
    transmission=std::max(0.0f,std::min(1.0f,finitePerceptionValue(transmission)));
    if(transmission<=0.0f)return 0.0f;
    const float forwardRange=18.0f,peripheralRange=7.5f,rearRange=3.0f;
    const float angular=forwardDot>0.35f?1.0f:(forwardDot>-0.45f?0.48f:0.12f);
    const float motionAttraction=(forwardDot<0.35f)?std::min(0.32f,targetSpeed*0.055f):0.0f;
    const float range=forwardDot>0.35f?forwardRange:(forwardDot>-0.45f?peripheralRange:rearRange);
    const float distanceFactor=std::max(0.0f,1.0f-distance/range);
    return std::max(0.0f,std::min(1.0f,(angular+motionAttraction)*distanceFactor*transmission));
}

inline EnemyPerceptionOutput updateEnemyPerception(const EnemyPerceptionInput& raw,EnemyPerceptionState& state){
    const float dt=std::max(0.0f,std::min(0.1f,finitePerceptionValue(raw.dt)));
    state.lastSeenPosition=finitePerceptionVector(state.lastSeenPosition);
    state.lastSeenVelocity=finitePerceptionVector(state.lastSeenVelocity);
    state.confidence=std::max(0.0f,std::min(1.0f,finitePerceptionValue(state.confidence)));
    state.uncertainty=std::max(0.0f,std::min(1.0f,finitePerceptionValue(state.uncertainty,1.0f)));
    state.headYaw=std::max(-1.18f,std::min(1.18f,finitePerceptionValue(state.headYaw)));
    state.headPitch=std::max(-0.48f,std::min(0.48f,finitePerceptionValue(state.headPitch)));
    state.searchPhase=finitePerceptionValue(state.searchPhase);
    const Vec3 observer=finitePerceptionVector(raw.observerPosition);
    const float visibility=std::max(0.0f,std::min(1.0f,finitePerceptionValue(raw.visibility)));
    const float cueStrength=std::max(0.0f,std::min(1.0f,finitePerceptionValue(raw.environmentalCueStrength)));
    const float memoryPressure=std::max(0.0f,std::min(1.0f,finitePerceptionValue(raw.roomMemoryPressure)));
    const float awareness=std::max(0.0f,std::min(1.0f,finitePerceptionValue(raw.vagueAwareness)));
    const Vec3 cuePosition=finitePerceptionVector(raw.environmentalCuePosition);
    if(raw.sampled){
        if(visibility>=0.18f){
            state.lastSeenPosition=finitePerceptionVector(raw.targetPosition);
            state.lastSeenVelocity=finitePerceptionVector(raw.targetVelocity);
            state.confidence=std::max(state.confidence,visibility);
            state.uncertainty=std::max(0.0f,1.0f-visibility);
            state.confirmed=visibility>=0.26f;
        }else state.confirmed=false;
    }
    if(!state.confirmed&&cueStrength>0.08f){
        // World events compete with short memory instead of teleporting attention
        // every frame. A clearly stronger cue can relocate investigation; weaker
        // cues only tug the existing belief and increase uncertainty. This keeps
        // herd agitation and phone actions readable as imperfect evidence.
        const float cueConfidence=cueStrength*0.34f;
        const bool establishedBelief=state.confidence>0.035f;
        if(!establishedBelief||cueConfidence>state.confidence*1.22f){
            state.lastSeenPosition=cuePosition;
        }else{
            const float pull=std::min(0.28f,cueConfidence/(state.confidence+0.001f)*0.12f);
            state.lastSeenPosition=state.lastSeenPosition+(cuePosition-state.lastSeenPosition)*pull;
        }
        state.lastSeenVelocity={};
        state.confidence=std::max(state.confidence,cueConfidence);
        state.uncertainty=std::max(state.uncertainty,establishedBelief?0.68f:0.62f);
    }
    if(!state.confirmed){
        // A room that has stayed dangerous longer makes animals less willing to
        // dismiss an unresolved event. Pressure changes forgetting, not evidence:
        // it cannot create a belief, improve its precision, or confirm the player.
        const float forgettingScale=1.0f-memoryPressure*0.42f;
        state.confidence=std::max(0.0f,state.confidence-dt*(0.16f+state.uncertainty*0.10f)*forgettingScale);
        state.uncertainty=std::min(1.0f,state.uncertainty+dt*0.13f);
        state.lastSeenVelocity=state.lastSeenVelocity*std::max(0.0f,1.0f-dt*2.5f);
        // Directionless sound/proximity does not create a target, but it makes an
        // uncertain animal search more urgently. Reuse the existing search clock
        // so the response is visible through gaze without bypassing perception.
        state.searchPhase=std::fmod(state.searchPhase+dt*(1.4f+state.uncertainty*2.3f+awareness*0.65f),DB_PI*2.0f);
    }else{
        state.confidence=std::min(1.0f,state.confidence+dt*0.35f);
        state.uncertainty=std::max(0.0f,state.uncertainty-dt*0.45f);
    }
    const bool hasBelief=state.confidence>0.035f;
    Vec3 interest{};
    if(hasBelief)interest=state.lastSeenPosition-observer;
    else {
        // Lost animals already own a bounded search clock. Use that authority for
        // directionless scanning too; the previous expression depended only on
        // uncertainty/confidence and could settle into an almost fixed stare once
        // memory expired. This changes presentation/perception orientation only:
        // it creates no spatial belief and grants locomotion no hidden target.
        const float phase=state.searchPhase+finitePerceptionValue(raw.individuality)*0.73f;
        interest={std::sin(phase),0.0f,-std::cos(phase)};
    }
    const float horizontal=std::sqrt(interest.x*interest.x+interest.z*interest.z);
    float desiredYaw=state.headYaw,desiredPitch=0.0f;
    if(horizontal>0.001f){
        const float worldYaw=std::atan2(-interest.x,-interest.z);
        desiredYaw=wrapPerceptionAngle(worldYaw-finitePerceptionValue(raw.bodyYaw));
        desiredPitch=-std::atan2(interest.y,horizontal)-finitePerceptionValue(raw.bodyPitch)*0.35f;
    }
    if(hasBelief&&!state.confirmed){
        const float nearby=1.0f-std::min(1.0f,horizontal/4.0f);
        // The same room history that keeps an unresolved belief alive also makes
        // that uncertainty physically readable as vigilance. It changes only
        // head-search amplitude: no extra evidence, precision, or locomotion.
        const float vigilance=1.0f+memoryPressure*0.32f;
        desiredYaw+=std::sin(state.searchPhase+finitePerceptionValue(raw.individuality)*1.7f)*(0.18f+nearby*0.52f)*state.uncertainty*vigilance;
        desiredPitch+=std::sin(state.searchPhase*0.63f+1.1f)*0.10f*state.uncertainty*(1.0f+memoryPressure*0.18f);
    }
    // Directionless nearby awareness increases the urge to scan, never the
    // spatial precision of the remembered target.
    if(!hasBelief&&awareness>0.0f)desiredYaw+=std::sin((state.uncertainty+1.0f)*4.7f+raw.individuality)*0.55f*awareness;
    desiredYaw=std::max(-1.18f,std::min(1.18f,desiredYaw-finitePerceptionValue(raw.bodyRoll)*0.20f));
    desiredPitch=std::max(-0.48f,std::min(0.48f,desiredPitch));
    const float headFollow=1.0f-std::exp(-dt*(state.confirmed?9.0f:4.0f));
    state.headYaw+=wrapPerceptionAngle(desiredYaw-state.headYaw)*headFollow;
    state.headPitch+=(desiredPitch-state.headPitch)*headFollow;
    const float worldHeadYaw=finitePerceptionValue(raw.bodyYaw)+state.headYaw;
    EnemyPerceptionOutput out;
    out.believedPosition=state.lastSeenPosition;
    out.perceivedVelocity=state.confirmed?state.lastSeenVelocity:Vec3{};
    out.headDirection={-std::sin(worldHeadYaw)*std::cos(state.headPitch),-std::sin(state.headPitch),-std::cos(worldHeadYaw)*std::cos(state.headPitch)};
    out.confidence=state.confidence;out.uncertainty=state.uncertainty;
    out.headYaw=state.headYaw;out.headPitch=state.headPitch;
    out.confirmed=state.confirmed;out.hasSpatialBelief=hasBelief;
    return out;
}

} // namespace gameplay
