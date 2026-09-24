#include <cassert>
#include <cstdio>
#include "../game/gameplay/EnemyPerception.hpp"
int main(){
    using gameplay::landingAwarenessStrength;
    assert(landingAwarenessStrength(1.0f,1.0f,1.0f,1.0f)==0.0f);
    const float hard=landingAwarenessStrength(1.0f,6.0f,1.0f,1.0f);
    const float soft=landingAwarenessStrength(1.0f,2.5f,1.0f,1.0f);
    assert(hard>soft&&soft>0.0f);
    assert(landingAwarenessStrength(8.0f,6.0f,1.0f,1.0f)==0.0f);
    assert(landingAwarenessStrength(1.0f,6.0f,0.78f,0.72f)<hard);
    std::puts("ENEMY_LANDING_CUE_OK bounded impact cue respects distance weather and surface transmission");
}
