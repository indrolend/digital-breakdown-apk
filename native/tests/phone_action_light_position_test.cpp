#include <cassert>
#include <cmath>
#include "../game/RenderContracts.hpp"

int main(){
    using namespace render_contract;
    const Vec3 phone{1.0f,2.0f,3.0f};
    const Vec3 pull{1.0f,2.0f,-7.0f};
    const Vec3 idle=phoneActionLightPosition(phone,pull,0.0f);
    const Vec3 active=phoneActionLightPosition(phone,pull,1.0f);
    const Vec3 clamped=phoneActionLightPosition(phone,pull,4.0f);
    assert(std::abs(idle.z-phone.z)<0.0001f);
    assert(active.z<phone.z&&active.z>pull.z);
    assert(std::abs(active.z-(phone.z+(pull.z-phone.z)*0.34f))<0.0001f);
    assert(std::abs(clamped.z-active.z)<0.0001f);
    return 0;
}
