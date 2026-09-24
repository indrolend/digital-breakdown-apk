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

    const auto dryField=floorMaterialResponse(RoomSetting::Field,0.0f,0.0f,true);
    const auto wetField=floorMaterialResponse(RoomSetting::Field,0.0f,1.0f,true);
    const auto shelteredField=floorMaterialResponse(RoomSetting::Field,0.0f,1.0f,false);
    const auto awakenedField=floorMaterialResponse(RoomSetting::Field,1.0f,0.0f,true);
    assert(wetField.wetness>dryField.wetness&&wetField.color.r<dryField.color.r&&wetField.color.g<dryField.color.g);
    assert(wetField.specular>0.0f&&wetField.shininess>0.0f);
    assert(wetField.traction<dryField.traction);
    assert(wetField.movementCueTransmission<dryField.movementCueTransmission);
    assert(near(shelteredField.wetness,0.0f)&&same(shelteredField.color,dryField.color)&&near(shelteredField.traction,1.0f));
    const auto wetCity=floorMaterialResponse(RoomSetting::City,0.0f,1.0f,true);
    const auto wetSterile=floorMaterialResponse(RoomSetting::Sterile,0.0f,1.0f,true);
    const auto wetCoastal=floorMaterialResponse(RoomSetting::Coastal,0.0f,1.0f,true);
    assert(wetCoastal.traction<wetField.traction&&wetField.traction<wetCity.traction&&wetCity.traction<wetSterile.traction);
    assert(dryField.movementCueTransmission<floorMaterialResponse(RoomSetting::City,0.0f,0.0f,true).movementCueTransmission);
    assert(!same(awakenedField.color,dryField.color));

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
