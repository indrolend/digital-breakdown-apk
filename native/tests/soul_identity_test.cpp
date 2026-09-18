#include "SoulIdentity.hpp"

#include <cassert>
#include <cstdio>
#include <string>

int main(){
    const auto a=soul_identity::profile(41,false,3);
    const auto repeat=soul_identity::profile(41,false,3);
    const auto different=soul_identity::profile(42,false,3);
    assert(std::string(a.givenName)==repeat.givenName);
    assert(std::string(a.familyName)==repeat.familyName);
    assert(std::string(a.occupation)==repeat.occupation);
    assert(a.gender==repeat.gender&&a.age==repeat.age);
    assert(a.age>=18&&a.age<=87);
    assert(soul_identity::name(a).find(' ')!=std::string::npos);
    assert(!soul_identity::details(a).empty());
    assert(soul_identity::name(a)!=soul_identity::name(different)||a.age!=different.age||std::string(a.occupation)!=different.occupation);
    const auto unknown=soul_identity::profile(0,false,0);
    assert(std::string(unknown.givenName)=="UNKNOWN"&&unknown.age==0);
    const auto visual=soul_identity::visualSignature(41,false,3);
    const auto visualRepeat=soul_identity::visualSignature(41,false,3);
    const auto visualDifferent=soul_identity::visualSignature(42,false,3);
    assert(visual.red==visualRepeat.red&&visual.green==visualRepeat.green&&visual.blue==visualRepeat.blue);
    assert(visual.phase==visualRepeat.phase&&visual.spinRate==visualRepeat.spinRate&&visual.aspect==visualRepeat.aspect);
    assert(visual.red>=0.62f&&visual.red<=0.84f&&visual.green>=0.86f&&visual.green<=0.98f&&visual.blue>=0.66f&&visual.blue<=0.90f);
    assert(visual.aspect>=0.92f&&visual.aspect<=1.08f);
    assert(visual.phase!=visualDifferent.phase||visual.red!=visualDifferent.red||visual.aspect!=visualDifferent.aspect);
    std::puts("SOUL_IDENTITY_OK deterministic bounded persistent projection");
    return 0;
}
