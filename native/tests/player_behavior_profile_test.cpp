#include "gameplay/PlayerBehaviorProfile.hpp"
#include <cassert>
int main(){
    gameplay::PlayerBehaviorProfile p{};
    for(int i=0;i<10;++i) gameplay::observePlayerBehavior(p,{true,i<5,i<2,i<7,i<3,0.1f});
    assert(p.observedTime>0.99f);
    assert(gameplay::behaviorRatio(p.movingTime,p)>0.99f);
    const float sprint=gameplay::behaviorRatio(p.sprintingTime,p);
    assert(sprint>0.49f&&sprint<0.51f);
    return 0;
}
