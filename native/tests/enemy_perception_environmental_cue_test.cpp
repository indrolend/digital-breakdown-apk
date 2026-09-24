#include "../game/gameplay/EnemyPerception.hpp"
#include "../game/gameplay/PlayerContactEvidence.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace gameplay;
int main(){
    // Body movement is directionless evidence: faster nearby motion is more
    // noticeable, distance and rain reduce it, and standing still is silent.
    const float still=movementEvidenceAwarenessStrength(1.0f,playerContactEvidence({},true,0.0f).movement,1.0f);
    const float walk=movementEvidenceAwarenessStrength(1.0f,playerContactEvidence({2.0f,0.0f,0.0f},true,0.0f).movement,1.0f);
    const float run=movementEvidenceAwarenessStrength(1.0f,playerContactEvidence({6.0f,0.0f,0.0f},true,0.0f).movement,1.0f);
    const float distantRun=movementEvidenceAwarenessStrength(5.5f,playerContactEvidence({6.0f,0.0f,0.0f},true,0.0f).movement,1.0f);
    const float rainyRun=movementEvidenceAwarenessStrength(1.0f,playerContactEvidence({6.0f,0.0f,0.0f},true,0.0f).movement,environmentalCueTransmission(1.0f,true));
    assert(still==0.0f&&run>walk&&walk>0.0f);
    assert(distantRun<run&&rainyRun<run);
    const float softSurfaceRun=movementEvidenceAwarenessStrength(1.0f,playerContactEvidence({6.0f,0.0f,0.0f},true,0.0f).movement,1.0f,0.76f);
    const float airborneRun=movementEvidenceAwarenessStrength(1.0f,playerContactEvidence({6.0f,0.0f,0.0f},false,0.0f).movement,1.0f);
    assert(softSurfaceRun<run);
    assert(airborneRun==0.0f);
    const float weakEvidence=movementEvidenceAwarenessStrength(1.0f,0.18f,1.0f,1.0f);
    const float strongEvidence=movementEvidenceAwarenessStrength(1.0f,0.82f,1.0f,1.0f);
    assert(strongEvidence>weakEvidence&&weakEvidence>0.0f);

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
    const Vec3 approximate=approximateEnvironmentalEvidencePosition({6.0f,1.0f,-3.0f},0.4f,0.3f);
    assert(std::abs(approximate.x-6.0f)>0.01f||std::abs(approximate.z+3.0f)>0.01f);
    std::printf("ENEMY_PERCEPTION_ENVIRONMENTAL_CUE_OK confidence=%.3f uncertainty=%.3f\n",cue.confidence,cue.uncertainty);
}
