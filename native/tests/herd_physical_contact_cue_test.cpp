#include <cassert>
#include <cstdio>
#include "../game/gameplay/EnemyPerception.hpp"

int main(){
    using gameplay::physicalHerdCueStrength;
    const float quiet=physicalHerdCueStrength(2.0f,0.0f,0.0f,1.0f,1.0f);
    const float slip=physicalHerdCueStrength(2.0f,0.0f,0.8f,1.0f,1.0f);
    const float distant=physicalHerdCueStrength(6.5f,0.0f,0.8f,1.0f,1.0f);
    const float rain=physicalHerdCueStrength(2.0f,0.0f,0.8f,1.0f,0.78f);
    const float injury=physicalHerdCueStrength(2.0f,0.52f,0.8f,1.0f,1.0f);
    assert(quiet==0.0f);
    assert(slip>0.20f);
    assert(distant<slip);
    assert(rain<slip);
    assert(injury>slip);
    std::printf("HERD_PHYSICAL_CONTACT_CUE_OK quiet %.3f slip %.3f distant %.3f rain %.3f injury %.3f\n",quiet,slip,distant,rain,injury);
}
