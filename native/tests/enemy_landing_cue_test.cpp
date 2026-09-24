#include <cassert>
#include <cstdio>
#include "../game/gameplay/EnemyPerception.hpp"
#include "../game/gameplay/PlayerContactEvidence.hpp"
int main(){
    using gameplay::landingEvidenceAwarenessStrength;
    using gameplay::playerContactEvidence;
    assert(landingEvidenceAwarenessStrength(1.0f,playerContactEvidence({},true,1.0f).landing,1.0f,1.0f)==0.0f);
    const float hard=landingEvidenceAwarenessStrength(1.0f,playerContactEvidence({},true,6.0f).landing,1.0f,1.0f);
    const float soft=landingEvidenceAwarenessStrength(1.0f,playerContactEvidence({},true,2.5f).landing,1.0f,1.0f);
    assert(hard>soft&&soft>0.0f);
    assert(landingEvidenceAwarenessStrength(8.0f,playerContactEvidence({},true,6.0f).landing,1.0f,1.0f)==0.0f);
    assert(landingEvidenceAwarenessStrength(1.0f,playerContactEvidence({},true,6.0f).landing,0.78f,0.72f)<hard);
    std::puts("ENEMY_LANDING_CUE_OK bounded impact cue respects distance weather and surface transmission");
}
