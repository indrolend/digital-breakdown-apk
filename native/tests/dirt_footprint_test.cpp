#include <cassert>
#include <cmath>
#include <cstdio>
#include "../game/EarlyBrowserVisuals.hpp"
int main(){
    using namespace early_browser_visuals;
    for(int patch=0;patch<4;++patch){
        for(int lobe=0;lobe<3;++lobe){
            const auto shape=dirtLobe(patch,lobe);
            assert(shape.radiusX>0.0f&&shape.radiusZ>0.0f);
            // Every lobe overlaps the patch origin: one contiguous worn area.
            assert(dirtLobeContains(shape,0.0f,0.0f));
            const auto again=dirtLobe(patch,lobe);
            assert(shape.offsetX==again.offsetX&&shape.offsetZ==again.offsetZ);
        }
    }
    const auto dryImpact=dirtContactResponse(0.8f,0.0f);
    const auto wetImpact=dirtContactResponse(0.8f,1.0f);
    const auto halfWetImpact=dirtContactResponse(0.8f,0.5f);
    assert(std::abs(dryImpact.dust-0.8f)<0.0001f&&dryImpact.darkKick==0.0f);
    assert(wetImpact.dust==0.0f&&std::abs(wetImpact.darkKick-0.8f)<0.0001f);
    assert(std::abs(halfWetImpact.dust-halfWetImpact.darkKick)<0.0001f);
    std::puts("DIRT_FOOTPRINT_OK contiguous deterministic three-lobe patches contact=WEATHER_MATERIAL_RESPONSE");
}
