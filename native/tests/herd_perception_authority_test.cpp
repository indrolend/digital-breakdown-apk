#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <cstdio>
int main(){
    std::ifstream in("native/game/Game.cpp");
    assert(in.good());
    const std::string source((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    const auto herd=source.find("state_.herd.alarm>0.08f");
    assert(herd!=std::string::npos);
    const auto begin=source.rfind("if(!state_.multiplayer.enabled",herd);
    const auto end=source.find("const Vec3 playerTangent",herd);
    assert(begin!=std::string::npos&&end!=std::string::npos&&end>begin);
    const std::string block=source.substr(begin,end-begin);
    assert(block.find("perception.hasSpatialBelief")!=std::string::npos);
    assert(block.find("attackedPlayerPos")!=std::string::npos);
    assert(block.find("state_.player.pos")==std::string::npos);
    std::puts("HERD_PERCEPTION_AUTHORITY_OK");
}
