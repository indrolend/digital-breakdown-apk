#include <cstdio>

#include "DesktopPlaytestPolicy.hpp"

int main(){
    constexpr DesktopPlaytestPolicy normal{};
    constexpr DesktopPlaytestPolicy automated{true};
    const bool ok=
        normal.clearInputOnFocusChange()&&normal.releaseCaptureOnFocusLoss()&&normal.allowsNetworkMode(true)&&
        automated.clearInputOnFocusChange()&&!automated.releaseCaptureOnFocusLoss()&&
        automated.allowsNetworkMode(false)&&!automated.allowsNetworkMode(true);
    if(!ok){std::fprintf(stderr,"DESKTOP_PLAYTEST_POLICY_FAILED\n");return 1;}
    std::printf("DESKTOP_PLAYTEST_POLICY_OK normal_focus_pause=ON automation_focus_pause=OFF input_clearing=ON automation_network=REFUSED\n");
    return 0;
}
