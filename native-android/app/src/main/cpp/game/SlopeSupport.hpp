#pragma once

#include "Math.hpp"

#include <cmath>

enum class SlopeAxis : unsigned char { PositiveX, NegativeX, PositiveZ, NegativeZ };
enum class SupportClassification : unsigned char { Ordinary, TraversableSlope, Steep };

struct SlopeSupport {
    float minX=0.0f,maxX=0.0f,minZ=0.0f,maxZ=0.0f;
    float lowHeight=0.0f,highHeight=0.0f;
    SlopeAxis axis=SlopeAxis::NegativeZ;
};

struct SlopeSupportSample {
    bool inside=false;
    float height=0.0f;
    Vec3 normal{0.0f,1.0f,0.0f};
    SupportClassification classification=SupportClassification::Ordinary;
};

inline float slopeRun(const SlopeSupport& slope){
    return slope.axis==SlopeAxis::PositiveX||slope.axis==SlopeAxis::NegativeX
        ?slope.maxX-slope.minX:slope.maxZ-slope.minZ;
}

inline Vec3 slopeSupportNormal(const SlopeSupport& slope){
    const float run=std::max(0.0001f,slopeRun(slope));
    const float grade=(slope.highHeight-slope.lowHeight)/run;
    switch(slope.axis){
        case SlopeAxis::PositiveX:return normalized({-grade,1.0f,0.0f});
        case SlopeAxis::NegativeX:return normalized({grade,1.0f,0.0f});
        case SlopeAxis::PositiveZ:return normalized({0.0f,1.0f,-grade});
        case SlopeAxis::NegativeZ:return normalized({0.0f,1.0f,grade});
    }
    return {0.0f,1.0f,0.0f};
}

inline SupportClassification classifySupport(const SlopeSupport& slope){
    if(std::fabs(slope.highHeight-slope.lowHeight)<0.0001f)return SupportClassification::Ordinary;
    constexpr float MinimumWalkableNormalY=0.819152f; // 35 degrees from world up.
    return slopeSupportNormal(slope).y>=MinimumWalkableNormalY
        ?SupportClassification::TraversableSlope:SupportClassification::Steep;
}

inline SlopeSupportSample sampleSlopeSupport(const SlopeSupport& slope,float x,float z){
    SlopeSupportSample sample{};
    if(x<slope.minX||x>slope.maxX||z<slope.minZ||z>slope.maxZ)return sample;
    const float run=std::max(0.0001f,slopeRun(slope));
    float t=0.0f;
    switch(slope.axis){
        case SlopeAxis::PositiveX:t=(x-slope.minX)/run;break;
        case SlopeAxis::NegativeX:t=(slope.maxX-x)/run;break;
        case SlopeAxis::PositiveZ:t=(z-slope.minZ)/run;break;
        case SlopeAxis::NegativeZ:t=(slope.maxZ-z)/run;break;
    }
    sample.inside=true;sample.height=slope.lowHeight+(slope.highHeight-slope.lowHeight)*clampf(t,0.0f,1.0f);
    sample.normal=slopeSupportNormal(slope);sample.classification=classifySupport(slope);
    return sample;
}
