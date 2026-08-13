#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "Math.hpp"

namespace early_browser_visuals {

constexpr int CityPrimitiveCount = 10;
constexpr int GrassBladeCountLow = 40;
constexpr int GrassBladeCountHigh = 96;

struct CityPrimitive { Vec3 pos; Vec3 size; unsigned char material = 0; };
struct GrassBlade { Vec3 root; float height = 0.3f; float width = 0.035f; float phase = 0.0f; };

inline std::uint32_t mix(std::uint32_t value) {
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

inline float unit(std::uint32_t value) {
    return static_cast<float>(mix(value) & 0x00ffffffu) / 16777215.0f;
}

inline std::array<CityPrimitive, CityPrimitiveCount> cityForTile(int roomSeed, int tileIndex) {
    std::array<CityPrimitive, CityPrimitiveCount> result{};
    const std::uint32_t base = mix(static_cast<std::uint32_t>(roomSeed) ^ (static_cast<std::uint32_t>(tileIndex) * 0x9e3779b9u));
    result[0] = {{-8.35f,0.06f,0.0f},{0.72f,0.12f,24.0f},0};
    result[1] = {{ 8.35f,0.06f,0.0f},{0.72f,0.12f,24.0f},0};
    for (int i=2;i<CityPrimitiveCount;++i) {
        const float side=(i&1)?1.0f:-1.0f;
        const float height=1.4f+unit(base+i*17u)*3.0f;
        const float depth=2.0f+unit(base+i*31u)*2.1f;
        result[i]={{side*(15.8f+unit(base+i*47u)*1.35f),height*0.5f,-8.5f+(i/2)*5.3f+unit(base+i*61u)*0.8f},
                   {1.1f+unit(base+i*73u)*1.3f,height,depth},static_cast<unsigned char>(1+(i%3))};
    }
    return result;
}

inline GrassBlade grassBlade(int roomSeed, int tileIndex, int index) {
    const std::uint32_t base=mix(static_cast<std::uint32_t>(roomSeed) ^ static_cast<std::uint32_t>(tileIndex*4099+index*131));
    const float side=(index&1)?1.0f:-1.0f;
    return {{side*(6.55f+unit(base)*1.20f),0.02f,-11.4f+unit(base+1u)*22.8f},
            0.20f+unit(base+2u)*0.28f,0.025f+unit(base+3u)*0.025f,unit(base+4u)*6.2831853f};
}

inline Vec3 grassTip(const GrassBlade& blade,float time,const Vec3& player,float vacuumStrength,float shotImpulse) {
    const float wind=std::sin(time*1.7f+blade.phase+blade.root.z*0.19f)*0.075f;
    const Vec3 delta=blade.root-player;
    const float distance=std::sqrt(delta.x*delta.x+delta.z*delta.z);
    const float proximity=distance<1.7f?(1.0f-distance/1.7f):0.0f;
    const float invDistance=distance>0.001f?1.0f/distance:0.0f;
    const float away=proximity*0.22f;
    const float vacuum=vacuumStrength*proximity*0.16f;
    const float impulse=shotImpulse*std::max(0.0f,1.0f-distance/4.0f)*0.24f;
    return {blade.root.x+wind+delta.x*invDistance*(away+impulse)-delta.x*invDistance*vacuum,
            blade.root.y+blade.height,
            blade.root.z+delta.z*invDistance*(away+impulse)-delta.z*invDistance*vacuum};
}

inline char soulSymbol(int roomSeed,int targetIndex) {
    constexpr char glyphs[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    return glyphs[mix(static_cast<std::uint32_t>(roomSeed)^static_cast<std::uint32_t>(targetIndex*257))%36u];
}

} // namespace early_browser_visuals
