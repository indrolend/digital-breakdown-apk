#include "../game/gameplay/EnemyPerception.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace gameplay;
int main(){
    EnemyPerceptionState state{};
    EnemyPerceptionInput in{};
    in.observerPosition={0,1.6f,0}; in.dt=1.0f/60.0f;
    in.environmentalCuePosition={6,0,0}; in.environmentalCueStrength=0.72f;
    auto first=updateEnemyPerception(in,state);
    assert(first.hasSpatialBelief && first.believedPosition.x>5.9f);

    // A weaker contradictory event should tug attention, not erase the
    // established investigation target in one frame.
    in.environmentalCuePosition={-6,0,0}; in.environmentalCueStrength=0.32f;
    auto weak=updateEnemyPerception(in,state);
    assert(weak.believedPosition.x>3.0f);
    assert(weak.uncertainty>=first.uncertainty);

    // A substantially stronger event is allowed to win attention.
    in.environmentalCuePosition={-6,0,0}; in.environmentalCueStrength=1.0f;
    auto strong=updateEnemyPerception(in,state);
    assert(strong.believedPosition.x<0.0f);
    assert(!strong.confirmed);
    std::printf("ENEMY_PERCEPTION_CUE_MEMORY_OK first=%.2f weak=%.2f strong=%.2f\\n",
        first.believedPosition.x,weak.believedPosition.x,strong.believedPosition.x);
}
