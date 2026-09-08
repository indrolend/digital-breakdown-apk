#include "MaterialResponse.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
bool near(float a,float b){return std::fabs(a-b)<0.0001f;}
bool same(const VisualColor& a,const VisualColor& b){return near(a.r,b.r)&&near(a.g,b.g)&&near(a.b,b.b);}
}

int main(){
    using early_browser_visuals::RoomSetting;
    const auto field=roomSubstrateColor(RoomSetting::Field);
    const auto city=roomSubstrateColor(RoomSetting::City);
    const auto sterile=roomSubstrateColor(RoomSetting::Sterile);
    const auto coastal=roomSubstrateColor(RoomSetting::Coastal);
    assert(!same(field,city)&&!same(field,sterile)&&!same(field,coastal));
    assert(!same(city,sterile)&&!same(city,coastal)&&!same(sterile,coastal));

    assert(near(humanSubstrateAmount(2.0f,2.0f,false),0.0f));
    assert(near(humanSubstrateAmount(1.44f,2.0f,false),0.0f));
    const float mid=humanSubstrateAmount(1.0f,2.0f,false);
    const float critical=humanSubstrateAmount(0.1f,2.0f,false);
    assert(mid>0.0f&&critical>mid&&critical<=0.68f);
    assert(near(humanSubstrateAmount(1.0f,2.0f,true),1.0f));

    const auto freshFragment=particleMaterialColor(ParticleMaterial::Environment,RoomSetting::Coastal,1.0f);
    const auto reclaimedFragment=particleMaterialColor(ParticleMaterial::Environment,RoomSetting::Coastal,0.0f);
    assert(same(freshFragment,Pass7Visual::SoulFlesh));
    assert(same(reclaimedFragment,coastal));
    assert(same(particleMaterialColor(ParticleMaterial::Soul,RoomSetting::Field,0.5f),Pass7Visual::SoulBase));
    assert(same(particleMaterialColor(ParticleMaterial::Data,RoomSetting::Field,0.5f),Pass7Visual::ElectricCyan));

    std::puts("Material response tests passed.");
    return 0;
}
