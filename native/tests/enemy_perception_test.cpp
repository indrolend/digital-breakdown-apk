#include "gameplay/EnemyPerception.hpp"
#include "FacetedRock.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace {
constexpr float Dt=1.0f/60.0f;
bool finiteVec(const Vec3& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}

gameplay::EnemyPerceptionOutput observe(gameplay::EnemyPerceptionState& state,const Vec3& target,float visibility,
                                        const Vec3& velocity={},float bodyYaw=0.0f,bool sampled=true){
    gameplay::EnemyPerceptionInput input{};
    input.observerPosition={0.0f,1.65f,0.0f};input.targetPosition=target;input.targetVelocity=velocity;
    input.bodyYaw=bodyYaw;input.visibility=visibility;input.sampled=sampled;input.dt=Dt;input.individuality=0.17f;
    return gameplay::updateEnemyPerception(input,state);
}
}

int main(){
    // Forward sight acquires; an opaque blocker does not.
    gameplay::EnemyPerceptionState clear{},blocked{};
    const auto seen=observe(clear,{0.0f,1.0f,-6.0f},0.75f);
    const auto unseen=observe(blocked,{0.0f,1.0f,-6.0f},0.0f);
    assert(seen.confirmed&&seen.hasSpatialBelief&&!unseen.confirmed&&!unseen.hasSpatialBelief);

    float entry=0.0f;
    assert(gameplay::perceptionSegmentHitsBox({0,1,0},{0,1,-8},{-0.3f,0,-4.3f},{0.3f,3,-3.7f},entry)); // trunk
    assert(gameplay::perceptionSegmentHitsBox({0,1,0},{0,1,-8},{-0.08f,0.8f,-2.1f},{0.08f,1.2f,-1.9f},entry)); // small prop
    assert(!gameplay::perceptionSegmentHitsBox({0,1,0},{3,1,-8},{-0.3f,0,-4.3f},{0.3f,3,-3.7f},entry));

    // Perception consumes bounds derived from the same seeded/yawed mesh that
    // presentation and physical support use. The crown extends above the old
    // center-based half-height box and must still occlude.
    using namespace early_browser_visuals;
    const EnvironmentPropSpec tallRock{EnvironmentPrimitive::Rock,EnvironmentRole::Landmark,{0.0f,0.0f,-4.0f},{2.0f,2.0f,1.4f},0.0f,0};
    const auto tallMesh=faceted_rock::makeMesh(tallRock,91,7,2);
    const auto tallBounds=faceted_rock::meshBounds(tallMesh);
    const float upperSightY=tallBounds.maximum.y-0.01f;
    assert(upperSightY>tallRock.center.y+tallRock.size.y*0.5f);
    assert(gameplay::perceptionSegmentHitsBox({-3.0f,upperSightY,-4.0f},{3.0f,upperSightY,-4.0f},tallBounds.minimum,tallBounds.maximum,entry));

    const EnvironmentPropSpec rotatedRock{EnvironmentPrimitive::Rock,EnvironmentRole::Mass,{0.0f,0.0f,-4.0f},{4.0f,1.4f,1.0f},DB_PI*0.5f,0};
    const auto rotatedMesh=faceted_rock::makeMesh(rotatedRock,91,7,2);
    const auto rotatedBounds=faceted_rock::meshBounds(rotatedMesh);
    float vertexMinX=rotatedMesh.positions[0],vertexMaxX=rotatedMesh.positions[0],vertexMinZ=rotatedMesh.positions[2],vertexMaxZ=rotatedMesh.positions[2];
    for(int vertex=1;vertex<rotatedMesh.vertexCount;++vertex){vertexMinX=std::min(vertexMinX,rotatedMesh.positions[vertex*3]);vertexMaxX=std::max(vertexMaxX,rotatedMesh.positions[vertex*3]);vertexMinZ=std::min(vertexMinZ,rotatedMesh.positions[vertex*3+2]);vertexMaxZ=std::max(vertexMaxZ,rotatedMesh.positions[vertex*3+2]);}
    assert(rotatedBounds.minimum.x==vertexMinX&&rotatedBounds.maximum.x==vertexMaxX&&rotatedBounds.minimum.z==vertexMinZ&&rotatedBounds.maximum.z==vertexMaxZ);
    const float rotatedY=(rotatedBounds.minimum.y+rotatedBounds.maximum.y)*0.5f;
    const float rotatedZ=(rotatedBounds.minimum.z+rotatedBounds.maximum.z)*0.5f;
    assert(gameplay::perceptionSegmentHitsBox({rotatedBounds.minimum.x-1.0f,rotatedY,rotatedZ},{rotatedBounds.maximum.x+1.0f,rotatedY,rotatedZ},rotatedBounds.minimum,rotatedBounds.maximum,entry));
    assert(!gameplay::perceptionSegmentHitsBox({rotatedBounds.minimum.x-1.0f,rotatedBounds.maximum.y+0.5f,rotatedZ},{rotatedBounds.maximum.x+1.0f,rotatedBounds.maximum.y+0.5f,rotatedZ},rotatedBounds.minimum,rotatedBounds.maximum,entry));
    const float opaque=gameplay::visualAcquisitionStrength(5.0f,1.0f,0.0f,0.0f);
    const float foliage=gameplay::visualAcquisitionStrength(5.0f,1.0f,0.0f,0.55f);
    const float open=gameplay::visualAcquisitionStrength(5.0f,1.0f,0.0f,1.0f);
    assert(opaque==0.0f&&foliage>opaque&&foliage<open);

    const float forward=gameplay::visualAcquisitionStrength(5.0f,1.0f,0.0f,1.0f);
    const float peripheralStill=gameplay::visualAcquisitionStrength(5.0f,0.0f,0.0f,1.0f);
    const float peripheralMoving=gameplay::visualAcquisitionStrength(5.0f,0.0f,5.0f,1.0f);
    assert(forward>peripheralStill&&peripheralMoving>peripheralStill);

    // Loss retains the last confirmed point, never reads a live hidden target,
    // and becomes less certain until reacquisition refreshes it.
    const Vec3 remembered=seen.believedPosition;
    float priorConfidence=seen.confidence,priorUncertainty=seen.uncertainty;
    gameplay::EnemyPerceptionOutput lost{};
    float minimumSearchYaw=1.18f,maximumSearchYaw=-1.18f;
    for(int frame=0;frame<90;++frame){
        lost=observe(clear,{20.0f,1.0f,15.0f},0.0f,{9,0,0},0.0f,frame==0);
        minimumSearchYaw=std::min(minimumSearchYaw,lost.headYaw);
        maximumSearchYaw=std::max(maximumSearchYaw,lost.headYaw);
    }
    assert(lost.hasSpatialBelief&&!lost.confirmed);
    assert(lost.believedPosition.x==remembered.x&&lost.believedPosition.z==remembered.z);
    assert(lost.confidence<priorConfidence&&lost.uncertainty>priorUncertainty);
    assert(maximumSearchYaw-minimumSearchYaw>0.03f); // continuous search around remembered region
    const auto reacquired=observe(clear,{-2.0f,1.0f,-4.0f},0.9f);
    assert(reacquired.confirmed&&reacquired.believedPosition.x==-2.0f&&reacquired.uncertainty<lost.uncertainty);

    // Looking leads independently of travel/body yaw and remains anatomical.
    gameplay::EnemyPerceptionState side{};gameplay::EnemyPerceptionOutput sideLook{};
    for(int frame=0;frame<60;++frame)sideLook=observe(side,{5.0f,1.0f,-2.0f},0.7f);
    assert(std::abs(sideLook.headYaw)>0.25f&&std::abs(sideLook.headYaw)<=1.18f);
    gameplay::EnemyPerceptionInput fallen{};fallen.observerPosition={0,0.4f,0};fallen.targetPosition={0,1,-3};fallen.visibility=0.8f;fallen.sampled=true;fallen.dt=Dt;fallen.bodyPitch=1.1f;fallen.bodyRoll=0.8f;
    const auto fallenLook=gameplay::updateEnemyPerception(fallen,side);
    assert(std::abs(fallenLook.headYaw)<=1.18f&&std::abs(fallenLook.headPitch)<=0.48f);

    // Directionless ally/nearby awareness may move attention but cannot create coordinates.
    gameplay::EnemyPerceptionState aware{};gameplay::EnemyPerceptionInput awareness{};
    awareness.observerPosition={0,1,0};awareness.targetPosition={99,1,99};awareness.vagueAwareness=1.0f;awareness.dt=Dt;
    for(int frame=0;frame<120;++frame)gameplay::updateEnemyPerception(awareness,aware);
    assert(aware.confidence==0.0f&&aware.lastSeenPosition.x==0.0f&&std::abs(aware.headYaw)>0.01f);

    // Adversarial persistent/input values recover and identical histories match.
    // With no spatial belief, the existing search clock must keep gaze alive.
    // It may scan, but it must never manufacture target confidence or position.
    gameplay::EnemyPerceptionState searching{};
    gameplay::EnemyPerceptionInput searchInput{};searchInput.observerPosition={0,1,0};searchInput.bodyYaw=0;searchInput.individuality=0.31f;searchInput.dt=0.05f;
    float firstSearchYaw=0.0f,lastSearchYaw=0.0f;
    for(int frame=0;frame<80;++frame){const auto scan=gameplay::updateEnemyPerception(searchInput,searching);if(frame==0)firstSearchYaw=scan.headYaw;lastSearchYaw=scan.headYaw;assert(!scan.hasSpatialBelief&&scan.confidence==0.0f);}
    assert(std::abs(lastSearchYaw-firstSearchYaw)>0.20f);

    // Directionless environmental awareness must increase search urgency without
    // manufacturing spatial evidence. The existing search clock is the authority.
    gameplay::EnemyPerceptionState calmScan{},alertScan{};
    gameplay::EnemyPerceptionInput scanUrgency{};scanUrgency.observerPosition={0,1,0};scanUrgency.dt=0.05f;scanUrgency.individuality=0.21f;
    for(int frame=0;frame<20;++frame)gameplay::updateEnemyPerception(scanUrgency,calmScan);
    scanUrgency.vagueAwareness=0.85f;gameplay::EnemyPerceptionOutput alertOut{};
    for(int frame=0;frame<20;++frame)alertOut=gameplay::updateEnemyPerception(scanUrgency,alertScan);
    assert(alertScan.searchPhase>calmScan.searchPhase+0.45f);
    assert(!alertOut.hasSpatialBelief&&alertOut.confidence==0.0f);

    gameplay::EnemyPerceptionState poison{};poison.lastSeenPosition={NAN,INFINITY,-INFINITY};poison.lastSeenVelocity=poison.lastSeenPosition;poison.confidence=NAN;poison.uncertainty=INFINITY;poison.headYaw=NAN;poison.headPitch=INFINITY;poison.searchPhase=NAN;
    gameplay::EnemyPerceptionInput bad{};bad.observerPosition=poison.lastSeenPosition;bad.targetPosition=poison.lastSeenPosition;bad.targetVelocity=poison.lastSeenPosition;bad.bodyYaw=NAN;bad.visibility=INFINITY;bad.dt=INFINITY;bad.sampled=true;
    const auto safe=gameplay::updateEnemyPerception(bad,poison);
    assert(finiteVec(safe.believedPosition)&&finiteVec(safe.headDirection)&&std::isfinite(safe.confidence)&&std::isfinite(safe.uncertainty));
    assert(safe.confidence>=0&&safe.confidence<=1&&safe.uncertainty>=0&&safe.uncertainty<=1);
    gameplay::EnemyPerceptionState first{},second{};
    for(int frame=0;frame<600;++frame){const bool sample=(frame%13)==0;const float v=(frame/90)%2?0.0f:0.64f;const Vec3 p{std::sin(frame*0.03f)*4.0f,1.0f,-5.0f};const auto a=observe(first,p,v,{1,0,0},0.2f,sample);const auto b=observe(second,p,v,{1,0,0},0.2f,sample);assert(a.headYaw==b.headYaw&&a.confidence==b.confidence&&a.believedPosition.x==b.believedPosition.x);}
    return 0;
}
