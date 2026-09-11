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
    std::puts("SOUL_IDENTITY_OK deterministic bounded persistent projection");
    return 0;
}
