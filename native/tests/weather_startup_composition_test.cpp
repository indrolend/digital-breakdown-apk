#include "../game/Game.hpp"
#include "../game/RenderContracts.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>

int main(){
    RoomWeatherState weather{};
    constexpr float dayNightSeconds=180.0f;
    constexpr float pi=3.14159265358979323846f;
    const float phase=std::fmod(weather.cycleTime,dayNightSeconds)/dayNightSeconds;
    const float daylight=clampf(0.5f+0.5f*std::cos((phase-0.25f)*pi*2.0f),0.0f,1.0f);
    assert(daylight>0.999f);
    assert(weather.rain==0.0f);
    assert(weather.wetness==0.0f);

    using namespace render_contract;
    using early_browser_visuals::RoomForm;
    using early_browser_visuals::RoomSetting;
    const auto authored=roomLightingProfile(RoomSetting::Field,RoomForm::Open,12345,1,0.0f,0.0f);
    const auto startup=weatherLightingProfile(authored,true,daylight,0.0f);
    // Peak daylight must not reintroduce the launch-time dimming that this
    // convergence fixed. Fog still follows the authored horizon relationship.
    assert(std::abs(startup.skyTop.r-authored.skyTop.r)<0.00001f);
    assert(std::abs(startup.ambient.r-authored.ambient.r)<0.00001f);
    assert(std::abs(startup.primary.r-authored.primary.r)<0.00001f);
    const auto night=weatherLightingProfile(authored,true,0.0f,0.0f);
    const auto rain=weatherLightingProfile(authored,true,1.0f,1.0f);
    assert(night.primary.r<startup.primary.r);
    assert(night.fogDensity>startup.fogDensity);
    assert(rain.primary.r<startup.primary.r);
    assert(rain.fogDensity>startup.fogDensity);
    std::printf("WEATHER_STARTUP_COMPOSITION_OK cycle=%.1f daylight=%.3f\n",weather.cycleTime,daylight);
}
