#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
int main(){
    std::ifstream f("native/game/Game.cpp"); assert(f);
    std::ostringstream ss; ss<<f.rdbuf(); const std::string s=ss.str();
    assert(s.find("physicalSupportCuePosition(runtimePool.bodies[allyIndex],ally.pos)")!=std::string::npos);
    assert(s.find("socialCue=gameplay::physicalHerdCueStrength")!=std::string::npos);
    assert(s.find("runtimePool.bodies[allyIndex].supportContact,rainHearing")!=std::string::npos);
    const auto begin=s.find("const float allyMovementContact=");
    const auto end=s.find("gameplay::EnemyPerceptionInput perceptionInput",begin);
    assert(begin!=std::string::npos&&end!=std::string::npos);
    const auto block=s.substr(begin,end-begin);
    assert(block.find("state_.player.pos")==std::string::npos);
    std::puts("HERD_SOCIAL_CUE_AUTHORITY_OK");
}
