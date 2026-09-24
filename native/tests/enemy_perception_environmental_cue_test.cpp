#include "../game/gameplay/EnemyPerception.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace gameplay;
int main(){
    // Body movement is directionless evidence: faster nearby motion is more
    // noticeable, distance and rain reduce it, and standing still is silent.
    const float still=movementAwarenessStrength(1.0f,0.0f,1.0f);
    const float walk=movementAwarenessStrength(1.0f,2.0f,1.0f);
    const float run=movementAwarenessStrength(1.0f,6.0f,1.0f);
    const float distantRun=movementAwarenessStrength(5.5f,6.0f,1.0f);
    const float rainyRun=movementAwarenessStrength(1.0f,6.0f,environmentalCueTransmission(1.0f,true));
    assert(still==0.0f&&run>walk&&walk>0.0f);
    assert(distantRun<run&&rainyRun<run);
    const float softSurfaceRun=movementAwarenessStrength(1.0f,6.0f,1.0f,0.76f);
    const float airborneRun=movementAwarenessStrength(1.0f,6.0f,1.0f,1.0f,0.0f);
    assert(softSurfaceRun<run);
    assert(airborneRun==0.0f);

    EnemyPerceptionState state{};
    EnemyPerceptionInput in{};
    in.observerPosition={0,1.6f,0}; in.bodyYaw=0; in.dt=1.0f/60.0f;
    in.sampled=false; in.visibility=0.0f; in.vagueAwareness=0.7f;
    in.environmentalCuePosition={6.0f,1.0f,-3.0f};
    in.environmentalCueStrength=0.72f;
    const auto cue=updateEnemyPerception(in,state);
    assert(cue.hasSpatialBelief);
    assert(!cue.confirmed);
    assert(cue.confidence>0.08f && cue.confidence<0.35f);
    assert(cue.uncertainty>=0.62f);
    assert(std::abs(cue.believedPosition.x-6.0f)<0.001f);
    assert(std::abs(cue.believedPosition.z+3.0f)<0.001f);
    // A cue must never manufacture target velocity or visual confirmation.
    assert(std::abs(cue.perceivedVelocity.x)<0.0001f && std::abs(cue.perceivedVelocity.z)<0.0001f);
    std::printf("ENEMY_PERCEPTION_ENVIRONMENTAL_CUE_OK confidence=%.3f uncertainty=%.3f\n",cue.confidence,cue.uncertainty);
}
