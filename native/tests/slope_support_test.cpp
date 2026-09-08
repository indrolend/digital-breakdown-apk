#include "SlopeSupport.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

int main(){
    const SlopeSupport moderate{-2.0f,2.0f,2.0f,12.0f,0.0f,1.6f,SlopeAxis::NegativeZ};
    const auto low=sampleSlopeSupport(moderate,0.0f,12.0f);
    const auto middle=sampleSlopeSupport(moderate,0.0f,7.0f);
    const auto high=sampleSlopeSupport(moderate,0.0f,2.0f);
    assert(low.inside&&middle.inside&&high.inside);
    assert(std::fabs(low.height-0.0f)<0.0001f);
    assert(std::fabs(middle.height-0.8f)<0.0001f);
    assert(std::fabs(high.height-1.6f)<0.0001f);
    assert(low.classification==SupportClassification::TraversableSlope);
    assert(low.normal.y>0.98f&&low.normal.z>0.0f);
    assert(!sampleSlopeSupport(moderate,2.01f,7.0f).inside);
    assert(!sampleSlopeSupport(moderate,0.0f,12.01f).inside);
    const SlopeSupport flat{-1,1,-1,1,0.5f,0.5f,SlopeAxis::PositiveX};
    assert(classifySupport(flat)==SupportClassification::Ordinary);
    const SlopeSupport steep{-1,1,-0.5f,0.5f,0.0f,2.0f,SlopeAxis::PositiveZ};
    assert(classifySupport(steep)==SupportClassification::Steep);
    std::puts("Slope support tests passed.");
    return 0;
}
