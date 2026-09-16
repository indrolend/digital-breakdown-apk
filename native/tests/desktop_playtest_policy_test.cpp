#include <cstdio>

#include "DesktopPlaytestPolicy.hpp"
#include "DesktopMusicPolicy.hpp"

int main(){
    constexpr DesktopPlaytestPolicy normal{};
    constexpr DesktopPlaytestPolicy automated{true};
    const bool ok=
        normal.clearInputOnFocusChange(false)&&normal.clearInputOnFocusChange(true)&&normal.releaseCaptureOnFocusLoss()&&normal.acceptsRelativeMouseLook()&&normal.allowsNetworkMode(true)&&
        automated.clearInputOnFocusChange(false)&&!automated.clearInputOnFocusChange(true)&&!automated.releaseCaptureOnFocusLoss()&&!automated.acceptsRelativeMouseLook()&&
        automated.allowsNetworkMode(false)&&!automated.allowsNetworkMode(true)&&
        desktopMusicMode(false,false,false,false)==DesktopMusicMode::Menu&&
        desktopMusicMode(true,true,true,false)==DesktopMusicMode::Menu&&
        desktopMusicMode(true,false,true,true)==DesktopMusicMode::GameOver&&
        desktopMusicMode(true,false,false,true)==DesktopMusicMode::Flower&&
        desktopMusicMode(true,false,false,false)==DesktopMusicMode::Gameplay;
    if(!ok){std::fprintf(stderr,"DESKTOP_PLAYTEST_POLICY_FAILED\n");return 1;}
    std::printf("DESKTOP_PLAYTEST_POLICY_OK normal_focus_pause=ON automation_focus_pause=OFF input_clearing=ON automation_mouse_look=OFF automation_network=REFUSED\n");
    return 0;
}
