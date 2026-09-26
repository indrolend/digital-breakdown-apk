#include <cassert>
#include <cmath>

#include "../game/gameplay/RelentlessZombieMotor.hpp"

int main() {
    const auto pursuit=gameplay::relentlessZombieMotor({{3.0f,4.0f,-4.0f},5.0f,true});
    assert(pursuit.pursuing);
    assert(std::abs(pursuit.steering.x-0.6f)<0.0001f);
    assert(std::abs(pursuit.steering.y)<0.0001f);
    assert(std::abs(pursuit.steering.z+0.8f)<0.0001f);
    assert(pursuit.speedScale==1.0f);
    assert(pursuit.attackCommitment==1.0f);
    assert(pursuit.steering.x*3.0f+pursuit.steering.z*-4.0f>4.99f);

    // The motor has no disruption, personality, memory, or recovery input.
    // Re-evaluation after any physical event therefore immediately restores
    // the same DATA pursuit objective.
    const auto afterDisruption=gameplay::relentlessZombieMotor({{3.0f,0.0f,-4.0f},5.0f,true});
    assert(afterDisruption.pursuing);
    assert(std::abs(afterDisruption.steering.x-pursuit.steering.x)<0.0001f);
    assert(std::abs(afterDisruption.steering.z-pursuit.steering.z)<0.0001f);

    const auto invalid=gameplay::relentlessZombieMotor({{1.0f,0.0f,0.0f},1.0f,false});
    assert(!invalid.pursuing);
    assert(invalid.speedScale==0.0f);
    return 0;
}
