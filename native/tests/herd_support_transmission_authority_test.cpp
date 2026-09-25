#include <cassert>
#include <cstdio>
#include "../game/gameplay/EnemyPerception.hpp"

int main(){
    using gameplay::herdDisruptionContactActivity;
    using gameplay::physicalHerdCueStrength;
    const float planted=herdDisruptionContactActivity(0.8f,1.0f,1.0f);
    const float partial=herdDisruptionContactActivity(0.8f,0.35f,1.0f);
    const float airborne=herdDisruptionContactActivity(1.0f,0.0f,1.0f);
    assert(planted>partial&&partial>airborne);
    assert(airborne==0.0f);
    const float floorOnlyAirborne=physicalHerdCueStrength(2.0f,0.0f,1.0f,0.0f,1.0f,1.0f);
    const float socialAirborne=physicalHerdCueStrength(2.0f,0.52f,1.0f,0.0f,1.0f,1.0f);
    assert(floorOnlyAirborne==0.0f);
    assert(socialAirborne>0.0f);
    std::printf("HERD_SUPPORT_TRANSMISSION_AUTHORITY_OK planted %.3f partial %.3f social_airborne %.3f\n",planted,partial,socialAirborne);
}
