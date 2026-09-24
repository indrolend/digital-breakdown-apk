#include "gameplay/PlayerContactEvidence.hpp"
#include "gameplay/EnemyPerception.hpp"

#include <cassert>
#include <cstdio>

int main() {
    using namespace gameplay;
    const auto still=playerContactEvidence({},true,0.0f);
    const auto deliberate=playerContactEvidence({0.7f,0.0f,0.0f},true,0.0f);
    const auto walking=playerContactEvidence({2.4f,0.0f,0.0f},true,0.0f);
    const auto sprinting=playerContactEvidence({7.0f,0.0f,0.0f},true,0.0f);
    const auto airborne=playerContactEvidence({7.0f,0.0f,0.0f},false,0.0f);
    const auto landing=playerContactEvidence({},true,6.4f);
    assert(still.movement==0.0f);
    assert(deliberate.movement>0.0f&&deliberate.movement<walking.movement);
    assert(walking.movement<sprinting.movement);
    assert(airborne.movement==0.0f);
    assert(landing.landing>0.0f&&landing.landing<=1.0f);

    const float nearWalk=movementEvidenceAwarenessStrength(1.0f,walking.movement,1.0f,1.0f);
    const float nearSprint=movementEvidenceAwarenessStrength(1.0f,sprinting.movement,1.0f,1.0f);
    const float softSprint=movementEvidenceAwarenessStrength(1.0f,sprinting.movement,1.0f,0.72f);
    const float rainySprint=movementEvidenceAwarenessStrength(1.0f,sprinting.movement,0.78f,1.0f);
    assert(nearSprint>nearWalk&&nearWalk>0.0f);
    assert(softSprint<nearSprint&&rainySprint<nearSprint);
    const Vec3 source{4.0f,0.08f,-2.0f};
    const Vec3 weak=approximateEnvironmentalEvidencePosition(source,0.37f,0.18f);
    const Vec3 strong=approximateEnvironmentalEvidencePosition(source,0.37f,0.85f);
    assert(horizontalLength(weak-source)>horizontalLength(strong-source));
    assert(horizontalLength(strong-source)>0.05f);
    std::puts("PLAYER_CONTACT_EVIDENCE_OK still deliberate walk sprint airborne landing material weather uncertainty");
}
