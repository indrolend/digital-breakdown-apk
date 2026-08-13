#include "EarlyBrowserVisuals.hpp"
#include <cassert>
#include <cmath>

int main(){
    using namespace early_browser_visuals;
    const auto a=cityForTile(12345,2),b=cityForTile(12345,2),c=cityForTile(12346,2);
    assert(a[4].pos.x==b[4].pos.x&&a[4].size.y==b[4].size.y);
    assert(a[4].size.y!=c[4].size.y);
    const auto blade=grassBlade(12345,2,7);
    const Vec3 calm=grassTip(blade,0.5f,{0,0,0},0,0);
    const Vec3 displaced=grassTip(blade,0.5f,blade.root,0,1);
    assert(std::isfinite(calm.x)&&std::isfinite(displaced.x));
    assert(soulSymbol(12345,3)==soulSymbol(12345,3));
    assert(soulSymbol(12345,3)>='0');
}
