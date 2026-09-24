#include "../game/RenderContracts.hpp"
#include <cassert>
#include <cstdio>
using namespace render_contract;
int main(){
    SceneResponseInputs calm{}; calm.roomHeat=0.0f;
    SceneResponseInputs hot{}; hot.roomHeat=0.82f;
    SceneResponseInputs cleared=hot; cleared.roomClear=true;
    const auto a=sceneResponse(calm), b=sceneResponse(hot), c=sceneResponse(cleared);
    assert(a.roomPressure==0.0f);
    assert(b.roomPressure>0.81f && b.roomPressure<0.83f);
    assert(c.roomPressure<b.roomPressure*0.23f);
    assert(c.roomPressure>0.0f);
    RoomLightingProfile base{};
    base.ambient={0.4f,0.5f,0.6f}; base.fill={0.3f,0.4f,0.5f}; base.primary={0.8f,0.7f,0.6f}; base.fogDensity=0.02f;
    const auto calmLight=roomPressureLightingProfile(base,a.roomPressure);
    const auto hotLight=roomPressureLightingProfile(base,b.roomPressure);
    const auto clearedLight=roomPressureLightingProfile(base,c.roomPressure);
    assert(calmLight.ambient.r==base.ambient.r && calmLight.primary.r==base.primary.r && calmLight.fogDensity==base.fogDensity);
    assert(hotLight.ambient.r<base.ambient.r && hotLight.fill.r<base.fill.r);
    assert(hotLight.primary.r>base.primary.r && hotLight.fogDensity>base.fogDensity);
    assert(clearedLight.ambient.r>hotLight.ambient.r && clearedLight.fogDensity<hotLight.fogDensity);
    std::printf("SCENE_RESPONSE_ROOM_PRESSURE_OK calm=%.3f hot=%.3f cleared=%.3f fog=%.5f\n",a.roomPressure,b.roomPressure,c.roomPressure,hotLight.fogDensity);
}
