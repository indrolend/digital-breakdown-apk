#include "DesktopRenderer.hpp"
#include "DesktopAudio.hpp"
#include "DesktopMultiplayer.hpp"
#include "Game.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#undef near
#undef far
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <thread>

namespace {
constexpr double PRESENTATION_STEP_SECONDS = 1.0 / 60.0;
constexpr double MAX_FRAME_DELTA_SECONDS = 0.25;
constexpr int MAX_SIMULATION_STEPS_PER_FRAME = 4;
constexpr int KEY_W_ANDROID = 51;
constexpr int KEY_A_ANDROID = 29;
constexpr int KEY_S_ANDROID = 47;
constexpr int KEY_D_ANDROID = 32;
constexpr int KEY_Q_ANDROID = 45;
constexpr int KEY_C_ANDROID = 31;
constexpr int KEY_V_ANDROID = 50;
constexpr int KEY_F_ANDROID = 34;
constexpr int KEY_SHIFT_LEFT_ANDROID = 59;
constexpr int KEY_SHIFT_RIGHT_ANDROID = 60;
constexpr int KEY_SPACE_ANDROID = 62;

struct HostState {
    Game game;
    DesktopRenderer renderer;
    DesktopAudio audio;
    DesktopMultiplayer multiplayer;
    std::string multiplayerService="https://digital-breakdown-multiplayer.indrolend.workers.dev";
    std::string joinCode;
    bool enteringJoinCode=false;
    double lookX = 0.0;
    double lookY = 0.0;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool haveMouse = false;
    bool mouseCaptured = true;
    bool focused = true;
    int gamepadId = -1;
    bool gamepadMapped = false;
    bool restoreCaptureOnFocus = false;
    std::array<unsigned char, GLFW_GAMEPAD_BUTTON_LAST + 1> previousGamepadButtons{};
    bool previousGamepadLeftTrigger = false;
    bool previousGamepadMenuLeft = false;
    bool previousGamepadMenuRight = false;
    bool previousGamepadMenuUp = false;
    bool previousGamepadMenuDown = false;
    std::filesystem::path progressionPath;
    std::uint64_t savedProgressionRevision = 0;
};

struct DesktopGamepadInput {
    float moveX = 0.0f;
    float moveZ = 0.0f;
    float lookX = 0.0f;
    float lookY = 0.0f;
    bool vacuumHeld = false;
    bool sprintHeld = false;
    bool jumpPressed = false;
    bool meleePressed = false;
    bool shootPressed = false;
    bool cameraPressed = false;
};

float gamepadAxis(float value,float deadzone=0.18f){const float magnitude=std::abs(value);if(magnitude<=deadzone)return 0.0f;return std::copysign((magnitude-deadzone)/(1.0f-deadzone),value);}
float gamepadLookAxis(float value){const float axis=gamepadAxis(value,0.16f);return std::copysign(std::pow(std::abs(axis),1.35f),axis);}
bool triggerHeld(float value,float threshold=0.20f){return value>threshold;}
void resetGamepadHistory(HostState& host){host.previousGamepadButtons.fill(GLFW_RELEASE);host.previousGamepadLeftTrigger=false;host.previousGamepadMenuLeft=host.previousGamepadMenuRight=host.previousGamepadMenuUp=host.previousGamepadMenuDown=false;}
bool preferRawXboxLayout(int jid){const char* guid=glfwGetJoystickGUID(jid);return guid&&std::strcmp(guid,"030000005e040000130b000013050000")==0;}

std::filesystem::path progressionSavePath(){const char* local=std::getenv("LOCALAPPDATA");const std::filesystem::path root=local&&*local?std::filesystem::path(local):std::filesystem::temp_directory_path();return root/"DigitalBreakdown"/"progression.v1";}
void loadProgression(Game& game,const std::filesystem::path& path){std::ifstream input(path);std::string magic;int version=0,shot=0,lunge=0,attack=0;long long tokens=0;if(!(input>>magic>>version>>tokens>>shot>>lunge>>attack)||magic!="DBPROG")return;game.setPersistentProgression(tokens,shot,lunge,attack);if(version>=2){auto& settings=game.networkMutableState().localSettings;input>>settings.musicVolume>>settings.sfxVolume>>settings.musicMuted>>settings.sfxMuted>>settings.graphicsPreset>>settings.shadows>>settings.portalWindow>>settings.particles>>settings.fpsCounter>>settings.mouseLookSensitivity>>settings.touchLookSensitivity>>settings.controllerLookSensitivity;settings.musicVolume=clampf(settings.musicVolume,0,1);settings.sfxVolume=clampf(settings.sfxVolume,0,1);settings.mouseLookSensitivity=clampf(settings.mouseLookSensitivity,0.5f,1.75f);settings.touchLookSensitivity=clampf(settings.touchLookSensitivity,0.5f,1.75f);settings.controllerLookSensitivity=clampf(settings.controllerLookSensitivity,0.5f,1.75f);for(int& key:settings.keyboardBindings)if(!(input>>key))break;settings.menuPage=LocalMenuPage::Main;settings.rebindingAction=settings.pendingBinding=settings.conflictingAction=-1;}}
bool saveProgression(const PermanentProgressionState& progression,const LocalSettingsState& settings,const std::filesystem::path& path){
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
    const std::filesystem::path temporary=path.wstring()+L".tmp";
    {std::ofstream output(temporary,std::ios::trunc);if(!output)return false;output<<"DBPROG 2 "<<progression.tokens<<' '<<progression.levels[0]<<' '<<progression.levels[1]<<' '<<progression.levels[2]<<' '<<settings.musicVolume<<' '<<settings.sfxVolume<<' '<<settings.musicMuted<<' '<<settings.sfxMuted<<' '<<settings.graphicsPreset<<' '<<settings.shadows<<' '<<settings.portalWindow<<' '<<settings.particles<<' '<<settings.fpsCounter<<' '<<settings.mouseLookSensitivity<<' '<<settings.touchLookSensitivity<<' '<<settings.controllerLookSensitivity;for(int key:settings.keyboardBindings)output<<' '<<key;output<<'\n';output.flush();if(!output)return false;}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(temporary,error);return false;}
#else
    std::filesystem::rename(temporary,path,error);if(error){std::filesystem::remove(temporary,error);return false;}
#endif
    return true;}

int androidKeyForGlfw(const LocalSettingsState& settings,int key) {
    const int semantic[10]={KEY_W_ANDROID,KEY_S_ANDROID,KEY_A_ANDROID,KEY_D_ANDROID,KEY_SHIFT_LEFT_ANDROID,KEY_SPACE_ANDROID,KEY_C_ANDROID,KEY_Q_ANDROID,KEY_V_ANDROID,KEY_F_ANDROID};
    for(int i=0;i<10;++i)if(settings.keyboardBindings[i]==key)return semantic[i];
    return key==GLFW_KEY_RIGHT_SHIFT?KEY_SHIFT_RIGHT_ANDROID:-1;
}

HostState* stateFor(GLFWwindow* window) {
    return static_cast<HostState*>(glfwGetWindowUserPointer(window));
}

void setMouseCaptured(GLFWwindow* window, HostState& host, bool captured);
bool soloPauseMenu(const GameState& state){return state.started&&state.uiPaused&&!state.multiplayer.enabled&&!state.upgradeMenu.active;}

int menuItemCount(const GameState& state) {
    if(state.upgradeMenu.active)return 6;
    if(soloPauseMenu(state)){
        switch(state.localSettings.menuPage){
            case LocalMenuPage::Main:return 5;
            case LocalMenuPage::Controls:return 14;
            case LocalMenuPage::Audio:return 5;
            case LocalMenuPage::Graphics:return 5;
            default:return 5;
        }
    }
    if(!state.started){
        if(state.dead)return 2;
        switch(state.localSettings.menuPage){
            case LocalMenuPage::Main:return 4;
            case LocalMenuPage::Online:return 3;
            case LocalMenuPage::JoinCode:return 0;
            case LocalMenuPage::Settings:return 4;
            case LocalMenuPage::Controls:return 14;
            case LocalMenuPage::Audio:return 5;
            case LocalMenuPage::Graphics:return 5;
        }
    }
    if(state.uiPaused)return 1;
    return 0;
}

void openMenuPage(HostState& host,LocalMenuPage page){GameState& state=host.game.networkMutableState();state.localSettings.menuPage=page;state.localSettings.rebindingAction=-1;state.localSettings.pendingBinding=-1;state.localSettings.conflictingAction=-1;state.hud.menuSelection=0;state.cinematic.textInteraction=0.36f;}

bool adjustMenuSetting(HostState& host,int direction){GameState& state=host.game.networkMutableState();auto& settings=state.localSettings;const int item=state.hud.menuSelection;if(settings.menuPage==LocalMenuPage::Controls){if(item==10)settings.mouseLookSensitivity=clampf(settings.mouseLookSensitivity+direction*0.10f,0.5f,1.75f);else if(item==11)settings.controllerLookSensitivity=clampf(settings.controllerLookSensitivity+direction*0.10f,0.5f,1.75f);else return false;state.cinematic.textInteraction=0.65f;return true;}if(settings.menuPage==LocalMenuPage::Audio){if(item==0)settings.musicVolume=clampf(settings.musicVolume+direction*0.10f,0,1);else if(item==1)settings.sfxVolume=clampf(settings.sfxVolume+direction*0.10f,0,1);else return false;state.cinematic.textInteraction=0.65f;return true;}if(settings.menuPage==LocalMenuPage::Graphics&&item==0){settings.graphicsPreset=(settings.graphicsPreset+direction+3)%3;if(settings.graphicsPreset==0){settings.shadows=false;settings.portalWindow=false;settings.particles=false;}else if(settings.graphicsPreset==1){settings.shadows=true;settings.portalWindow=true;settings.particles=true;}else{settings.shadows=true;settings.portalWindow=true;settings.particles=true;}state.cinematic.textInteraction=0.65f;return true;}return false;}

void setMenuSelection(HostState& host,int selection) {
    const int count=menuItemCount(host.game.state());
    if(count<=0)return;
    GameState& state=host.game.networkMutableState();const int next=(selection%count+count)%count;
    if(state.hud.menuSelection!=next){state.hud.menuSelection=next;state.cinematic.textInteraction=std::max(state.cinematic.textInteraction,0.22f);host.audio.playMenuCue(false);}
}

int menuItemAt(GLFWwindow* window,const HostState& host,double windowX,double windowY) {
    int ww=1,wh=1,fw=1,fh=1;glfwGetWindowSize(window,&ww,&wh);glfwGetFramebufferSize(window,&fw,&fh);
    // Match DesktopRenderer's Retina logical-canvas scale followed by its
    // bounded menu scale (2.5 * 1.8 at the extreme), so visual and clickable
    // button rectangles stay identical on high-density displays.
    const float uiScale=clampf(std::min(static_cast<float>(fw)/1280.0f,static_cast<float>(fh)/720.0f),0.55f,4.5f);
    const float canvasW=fw/uiScale,canvasH=fh/uiScale;
    const float x=static_cast<float>(windowX)*fw/std::max(1,ww)/uiScale,y=static_cast<float>(windowY)*fh/std::max(1,wh)/uiScale;
    const GameState& state=host.game.state();
    if(state.upgradeMenu.active){
        const float pw=std::min(680.0f,canvasW-24.0f),ph=300.0f,px=(canvasW-pw)*0.5f,py=(canvasH-ph)*0.5f;
        if(y>=py+66&&y<=py+142)for(int column=0;column<3;++column){const float left=px+12+column*(pw-24)/3;if(x>=left&&x<left+(pw-24)/3)return column;}
        if(y>=py+184&&y<=py+250)for(int column=0;column<3;++column){const float left=px+12+column*(pw-24)/3;if(x>=left&&x<left+(pw-24)/3)return 3+column;}
        return -1;
    }
    if(!state.started||soloPauseMenu(state)){
        const bool controls=state.localSettings.menuPage==LocalMenuPage::Controls;
        const float pw=std::min(520.0f,canvasW*0.72f),ph=controls?560.0f:430.0f,px=(canvasW-pw)*0.5f;
        const float py=(canvasH-ph)*0.5f+(controls?0.0f:24.0f),buttonW=std::min(360.0f,pw-48.0f),buttonX=px+(pw-buttonW)*0.5f;
        if(x<buttonX||x>buttonX+buttonW)return -1;
        const int count=menuItemCount(state);
        const float step=controls?32.0f:52.0f,height=controls?27.0f:44.0f;for(int row=0;row<count;++row)if(y>=py+82+row*step&&y<py+82+height+row*step)return row;
        return -1;
    }
    if(state.uiPaused){const float pw=360.0f,px=canvasW-pw-12.0f,py=48.0f;if(x>=px+12&&x<=px+pw-12&&y>=py+34&&y<=py+92)return 0;}
    return -1;
}

void activateMenuSelection(GLFWwindow* window,HostState& host) {
    GameState& state=host.game.networkMutableState();const int selection=state.hud.menuSelection;state.cinematic.textInteraction=0.42f;
    host.audio.playMenuCue(true);
    if(state.upgradeMenu.active){
        if(selection<3)host.game.chooseTemporaryUpgrade(selection);else host.game.purchasePermanentUpgrade(selection-3);
        if(!host.game.state().upgradeMenu.active)setMouseCaptured(window,host,true);
        return;
    }
    if(soloPauseMenu(state)){
        auto& settings=state.localSettings;
        if(settings.menuPage==LocalMenuPage::Main){if(selection==0){setMouseCaptured(window,host,true);}else if(selection==1)openMenuPage(host,LocalMenuPage::Controls);else if(selection==2)openMenuPage(host,LocalMenuPage::Audio);else if(selection==3)openMenuPage(host,LocalMenuPage::Graphics);else{host.multiplayer.disconnect();host.game.prepareStartScreen();setMouseCaptured(window,host,false);}}
        else if(settings.menuPage==LocalMenuPage::Controls){if(selection<10){settings.rebindingAction=selection;settings.pendingBinding=-1;settings.conflictingAction=-1;}else if(selection==10||selection==11){adjustMenuSetting(host,1);}else if(selection==12){settings.keyboardBindings={{87,83,65,68,340,32,67,81,86,70}};settings.mouseLookSensitivity=1.0f;settings.controllerLookSensitivity=1.0f;}else openMenuPage(host,LocalMenuPage::Main);}
        else if(settings.menuPage==LocalMenuPage::Audio){if(selection==0){settings.musicVolume=std::fmod(settings.musicVolume+0.10f,1.01f);}else if(selection==1){settings.sfxVolume=std::fmod(settings.sfxVolume+0.10f,1.01f);}else if(selection==2)settings.musicMuted=!settings.musicMuted;else if(selection==3)settings.sfxMuted=!settings.sfxMuted;else openMenuPage(host,LocalMenuPage::Main);}
        else if(settings.menuPage==LocalMenuPage::Graphics){if(selection==0)adjustMenuSetting(host,1);else if(selection==1)settings.shadows=!settings.shadows;else if(selection==2)settings.particles=!settings.particles;else if(selection==3)settings.fpsCounter=!settings.fpsCounter;else openMenuPage(host,LocalMenuPage::Main);}
        return;
    }
    if(!state.started){
        if(state.dead){if(selection==1){glfwSetWindowShouldClose(window,GLFW_TRUE);return;}host.game.restart();setMouseCaptured(window,host,true);return;}
        auto& settings=state.localSettings;
        if(settings.menuPage==LocalMenuPage::Main){if(selection==0){host.multiplayer.disconnect();host.game.restart();setMouseCaptured(window,host,true);}else if(selection==1)openMenuPage(host,LocalMenuPage::Online);else if(selection==2)openMenuPage(host,LocalMenuPage::Settings);else glfwSetWindowShouldClose(window,GLFW_TRUE);}
        else if(settings.menuPage==LocalMenuPage::Online){if(selection==0){host.multiplayer.host(host.multiplayerService);host.game.setNetworkRoom("","CREATING",false);}else if(selection==1){host.enteringJoinCode=true;host.joinCode.clear();openMenuPage(host,LocalMenuPage::JoinCode);host.game.setNetworkRoom("","ENTER CODE",false);}else openMenuPage(host,LocalMenuPage::Main);}
        else if(settings.menuPage==LocalMenuPage::Settings){if(selection==0)openMenuPage(host,LocalMenuPage::Controls);else if(selection==1)openMenuPage(host,LocalMenuPage::Audio);else if(selection==2)openMenuPage(host,LocalMenuPage::Graphics);else openMenuPage(host,LocalMenuPage::Main);}
        else if(settings.menuPage==LocalMenuPage::Controls){if(selection<10){settings.rebindingAction=selection;settings.pendingBinding=-1;settings.conflictingAction=-1;}else if(selection==10||selection==11){adjustMenuSetting(host,1);}else if(selection==12){settings.keyboardBindings={{87,83,65,68,340,32,67,81,86,70}};settings.mouseLookSensitivity=1.0f;settings.controllerLookSensitivity=1.0f;}else openMenuPage(host,LocalMenuPage::Settings);}
        else if(settings.menuPage==LocalMenuPage::Audio){if(selection==0){settings.musicVolume=std::fmod(settings.musicVolume+0.10f,1.01f);}else if(selection==1){settings.sfxVolume=std::fmod(settings.sfxVolume+0.10f,1.01f);}else if(selection==2)settings.musicMuted=!settings.musicMuted;else if(selection==3)settings.sfxMuted=!settings.sfxMuted;else openMenuPage(host,LocalMenuPage::Settings);}
        else if(settings.menuPage==LocalMenuPage::Graphics){if(selection==0)adjustMenuSetting(host,1);else if(selection==1)settings.shadows=!settings.shadows;else if(selection==2)settings.particles=!settings.particles;else if(selection==3)settings.fpsCounter=!settings.fpsCounter;else openMenuPage(host,LocalMenuPage::Settings);}
        return;
    }
    if(state.uiPaused)setMouseCaptured(window,host,true);
}

void controllerMenuBack(GLFWwindow* window,HostState& host){
    const GameState& state=host.game.state();
    if(soloPauseMenu(state)){
        if(state.localSettings.menuPage!=LocalMenuPage::Main){openMenuPage(host,LocalMenuPage::Main);return;}
        setMouseCaptured(window,host,true);return;
    }
    if(!state.started&&state.localSettings.menuPage!=LocalMenuPage::Main){
        if(host.enteringJoinCode){host.enteringJoinCode=false;host.joinCode.clear();openMenuPage(host,LocalMenuPage::Online);return;}
        const auto page=state.localSettings.menuPage;
        openMenuPage(host,page==LocalMenuPage::Settings||page==LocalMenuPage::Online?LocalMenuPage::Main:LocalMenuPage::Settings);
        return;
    }
    if(state.uiPaused){setMouseCaptured(window,host,true);return;}
}

DesktopGamepadInput pollGamepad(GLFWwindow* window,HostState& host){
    DesktopGamepadInput input;
    int jid=host.gamepadId;
    bool mapped=jid>=GLFW_JOYSTICK_1&&jid<=GLFW_JOYSTICK_LAST&&glfwJoystickPresent(jid)&&glfwJoystickIsGamepad(jid)&&!preferRawXboxLayout(jid);
    bool present=jid>=GLFW_JOYSTICK_1&&jid<=GLFW_JOYSTICK_LAST&&glfwJoystickPresent(jid);
    if(!present){
        jid=-1;
        for(int candidate=GLFW_JOYSTICK_1;candidate<=GLFW_JOYSTICK_LAST;++candidate)if(glfwJoystickIsGamepad(candidate)&&!preferRawXboxLayout(candidate)){jid=candidate;break;}
        if(jid<0)for(int candidate=GLFW_JOYSTICK_1;candidate<=GLFW_JOYSTICK_LAST;++candidate)if(glfwJoystickPresent(candidate)){jid=candidate;break;}
        mapped=jid>=0&&glfwJoystickIsGamepad(jid)&&!preferRawXboxLayout(jid);
        if(jid!=host.gamepadId||mapped!=host.gamepadMapped){resetGamepadHistory(host);host.gamepadId=jid;host.gamepadMapped=mapped;if(jid>=0)std::printf("Controller connected: %s%s\n",mapped?glfwGetGamepadName(jid):glfwGetJoystickName(jid),mapped?"":" (raw joystick fallback)");}
    }
    if(jid<0)return input;
    GLFWgamepadstate pad{};
    std::array<unsigned char, GLFW_GAMEPAD_BUTTON_LAST + 1> currentButtons{};
    std::array<unsigned char, 32> currentRawButtons{};
    std::array<unsigned char, 8> currentRawHats{};
    float leftX=0,leftY=0,rightX=0,rightY=0;
    if(mapped){
        if(!glfwGetGamepadState(jid,&pad)){host.gamepadId=-1;resetGamepadHistory(host);return input;}
        for(int button=0;button<=GLFW_GAMEPAD_BUTTON_LAST;++button)currentButtons[button]=pad.buttons[button];
        leftX=gamepadAxis(pad.axes[GLFW_GAMEPAD_AXIS_LEFT_X],0.12f);leftY=gamepadAxis(pad.axes[GLFW_GAMEPAD_AXIS_LEFT_Y],0.12f);rightX=gamepadLookAxis(pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);rightY=gamepadLookAxis(pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
    }else{
        int axisCount=0,buttonCount=0,hatCount=0;
        const float* axes=glfwGetJoystickAxes(jid,&axisCount);
        const unsigned char* buttons=glfwGetJoystickButtons(jid,&buttonCount);
        const unsigned char* hats=glfwGetJoystickHats(jid,&hatCount);
        if(!axes&&!buttons&&!hats){host.gamepadId=-1;resetGamepadHistory(host);return input;}
        if(axisCount>0)leftX=gamepadAxis(axes[0],0.12f);if(axisCount>1)leftY=gamepadAxis(axes[1],0.12f);if(axisCount>2)rightX=gamepadLookAxis(axes[2]);if(axisCount>3)rightY=gamepadLookAxis(axes[3]);
        for(int i=0;i<std::min(buttonCount,static_cast<int>(currentRawButtons.size()));++i)currentRawButtons[i]=buttons[i];
        for(int i=0;i<std::min(hatCount,static_cast<int>(currentRawHats.size()));++i)currentRawHats[i]=hats[i];
        auto rawButton=[&](int button){return button<buttonCount&&button<static_cast<int>(currentRawButtons.size())&&currentRawButtons[button]==GLFW_PRESS;};
        if(preferRawXboxLayout(jid)){
            currentButtons[GLFW_GAMEPAD_BUTTON_A]=rawButton(0)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_B]=rawButton(1)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_X]=rawButton(3)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_Y]=rawButton(4)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]=rawButton(6)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER]=rawButton(7)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_BACK]=rawButton(10)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_START]=rawButton(11)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB]=rawButton(13)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB]=rawButton(14)?GLFW_PRESS:GLFW_RELEASE;
            pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]=axisCount>5?axes[5]:-1.0f;
            pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]=axisCount>4?axes[4]:-1.0f;
        }else{
            currentButtons[GLFW_GAMEPAD_BUTTON_A]=rawButton(0)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_B]=rawButton(1)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_X]=rawButton(2)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_Y]=rawButton(3)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]=rawButton(4)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER]=rawButton(5)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_START]=(rawButton(7)||rawButton(9))?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB]=rawButton(8)?GLFW_PRESS:GLFW_RELEASE;
            currentButtons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB]=rawButton(9)?GLFW_PRESS:GLFW_RELEASE;
            pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]=axisCount>4?axes[4]:-1.0f;
            pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]=axisCount>5?axes[5]:-1.0f;
        }
        if(hatCount>0){const unsigned char hat=currentRawHats[0];if(hat&GLFW_HAT_UP)currentButtons[GLFW_GAMEPAD_BUTTON_DPAD_UP]=GLFW_PRESS;if(hat&GLFW_HAT_RIGHT)currentButtons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT]=GLFW_PRESS;if(hat&GLFW_HAT_DOWN)currentButtons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]=GLFW_PRESS;if(hat&GLFW_HAT_LEFT)currentButtons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT]=GLFW_PRESS;}
    }
    const auto pressed=[&](int button){return currentButtons[button]==GLFW_PRESS&&host.previousGamepadButtons[button]!=GLFW_PRESS;};
    const bool menuActive=menuItemCount(host.game.state())>0;
    const bool menuLeft=currentButtons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT]==GLFW_PRESS||leftX<-0.55f;
    const bool menuRight=currentButtons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT]==GLFW_PRESS||leftX>0.55f;
    const bool menuUp=currentButtons[GLFW_GAMEPAD_BUTTON_DPAD_UP]==GLFW_PRESS||leftY<-0.55f;
    const bool menuDown=currentButtons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]==GLFW_PRESS||leftY>0.55f;
    const bool leftTriggerDown=triggerHeld(pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER],0.35f);
    const bool rightTriggerDown=triggerHeld(pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);
    const bool leftTriggerPressed=leftTriggerDown&&!host.previousGamepadLeftTrigger;
    if(menuActive&&!host.enteringJoinCode){
        if((menuUp&&!host.previousGamepadMenuUp)||(menuDown&&!host.previousGamepadMenuDown)){
            const int delta=host.game.state().upgradeMenu.active?(menuDown?3:-3):(menuDown?1:-1);setMenuSelection(host,host.game.state().hud.menuSelection+delta);
        }else if((menuLeft&&!host.previousGamepadMenuLeft)||(menuRight&&!host.previousGamepadMenuRight)){
            const int direction=menuRight?1:-1;if(!adjustMenuSetting(host,direction))setMenuSelection(host,host.game.state().hud.menuSelection+direction);
        }
        if(pressed(GLFW_GAMEPAD_BUTTON_A))activateMenuSelection(window,host);
        if(pressed(GLFW_GAMEPAD_BUTTON_B))controllerMenuBack(window,host);
        if(pressed(GLFW_GAMEPAD_BUTTON_START)&&host.game.state().uiPaused)controllerMenuBack(window,host);
    }else if(host.enteringJoinCode){
        if(pressed(GLFW_GAMEPAD_BUTTON_B))controllerMenuBack(window,host);
    }else{
        input.moveX=leftX;input.moveZ=-leftY;input.lookX=rightX*13.5f;input.lookY=rightY*10.5f;
        input.vacuumHeld=rightTriggerDown||currentButtons[GLFW_GAMEPAD_BUTTON_B]==GLFW_PRESS;
        input.sprintHeld=currentButtons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB]==GLFW_PRESS;
        input.jumpPressed=pressed(GLFW_GAMEPAD_BUTTON_A)||pressed(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER);
        input.meleePressed=pressed(GLFW_GAMEPAD_BUTTON_X)||leftTriggerPressed;
        input.shootPressed=pressed(GLFW_GAMEPAD_BUTTON_Y)||pressed(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER);
        input.cameraPressed=pressed(GLFW_GAMEPAD_BUTTON_RIGHT_THUMB);
        if(pressed(GLFW_GAMEPAD_BUTTON_DPAD_UP))host.game.setCommSignal(1);
        else if(pressed(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT))host.game.setCommSignal(2);
        else if(pressed(GLFW_GAMEPAD_BUTTON_DPAD_DOWN))host.game.setCommSignal(3);
        else if(pressed(GLFW_GAMEPAD_BUTTON_DPAD_LEFT))host.game.setCommSignal(4);
        if(pressed(GLFW_GAMEPAD_BUTTON_START))setMouseCaptured(window,host,!host.mouseCaptured);
        if(host.game.state().player.grabbedByTarget>=0&&std::abs(leftX)>0.35f)host.game.setWiggle(leftX*12.0f);
    }
    host.previousGamepadMenuLeft=menuLeft;host.previousGamepadMenuRight=menuRight;host.previousGamepadMenuUp=menuUp;host.previousGamepadMenuDown=menuDown;
    host.previousGamepadLeftTrigger=leftTriggerDown;
    host.previousGamepadButtons=currentButtons;
    return input;
}

void setMouseCaptured(GLFWwindow* window, HostState& host, bool captured) {
    const bool wasPaused=host.game.state().uiPaused;
    host.mouseCaptured = captured;
    host.haveMouse = false;
    host.lookX = 0.0;
    host.lookY = 0.0;
    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    host.game.setUiPaused(!captured);
    if(!captured&&host.game.state().started&&!host.game.state().multiplayer.enabled&&!wasPaused)openMenuPage(host,LocalMenuPage::Main);
    if (captured) {
        glfwGetCursorPos(window, &host.lastMouseX, &host.lastMouseY);
        host.haveMouse = true;
    }
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    HostState* host = stateFor(window);
    if (!host) return;

    if(action==GLFW_PRESS&&host->game.state().localSettings.rebindingAction>=0){auto& settings=host->game.networkMutableState().localSettings;if(key==GLFW_KEY_ESCAPE){settings.rebindingAction=-1;settings.pendingBinding=-1;settings.conflictingAction=-1;return;}const int actionIndex=settings.rebindingAction;int conflict=-1;for(int i=0;i<10;++i)if(i!=actionIndex&&settings.keyboardBindings[i]==key){conflict=i;break;}const int old=settings.keyboardBindings[actionIndex];settings.keyboardBindings[actionIndex]=key;if(conflict>=0)settings.keyboardBindings[conflict]=old;settings.rebindingAction=settings.pendingBinding=settings.conflictingAction=-1;host->audio.playMenuCue(true);return;}

    const bool menuActive=menuItemCount(host->game.state())>0;
    if(action==GLFW_PRESS&&menuActive&&!host->enteringJoinCode){
        // Menus release the cursor, so Escape has the same second-press exit
        // meaning it has after releasing the cursor during ordinary play.
        if(key==GLFW_KEY_ESCAPE){if(soloPauseMenu(host->game.state())){controllerMenuBack(window,*host);return;}if(!host->game.state().started&&host->game.state().localSettings.menuPage!=LocalMenuPage::Main){const auto page=host->game.state().localSettings.menuPage;openMenuPage(*host,page==LocalMenuPage::Settings||page==LocalMenuPage::Online?LocalMenuPage::Main:LocalMenuPage::Settings);return;}glfwSetWindowShouldClose(window,GLFW_TRUE);return;}
        const bool left=key==GLFW_KEY_LEFT||key==GLFW_KEY_A, right=key==GLFW_KEY_RIGHT||key==GLFW_KEY_D;
        const bool up=key==GLFW_KEY_UP||key==GLFW_KEY_W, down=key==GLFW_KEY_DOWN||key==GLFW_KEY_S;
        if(host->game.state().upgradeMenu.active&&(up||down)){setMenuSelection(*host,host->game.state().hud.menuSelection+(down?3:-3));return;}
        if((left||right)&&!host->game.state().started&&adjustMenuSetting(*host,right?1:-1))return;
        if(left||up){setMenuSelection(*host,host->game.state().hud.menuSelection-1);return;}
        if(right||down){setMenuSelection(*host,host->game.state().hud.menuSelection+1);return;}
        if(key==GLFW_KEY_ENTER||key==GLFW_KEY_SPACE||key==GLFW_KEY_F){activateMenuSelection(window,*host);return;}
        if(host->game.state().upgradeMenu.active&&key>=GLFW_KEY_1&&key<=GLFW_KEY_6){setMenuSelection(*host,key-GLFW_KEY_1);activateMenuSelection(window,*host);}
        return;
    }

    if(action==GLFW_PRESS&&!host->game.state().started&&host->enteringJoinCode){
        if(key==GLFW_KEY_ESCAPE){host->enteringJoinCode=false;host->joinCode.clear();openMenuPage(*host,LocalMenuPage::Online);return;}
        if(key==GLFW_KEY_BACKSPACE){if(!host->joinCode.empty())host->joinCode.pop_back();return;}
        if(key==GLFW_KEY_ENTER&&host->joinCode.size()==6){host->multiplayer.join(host->multiplayerService,host->joinCode);host->enteringJoinCode=false;return;}
        if(host->joinCode.size()<6&&((key>=GLFW_KEY_A&&key<=GLFW_KEY_Z)||(key>=GLFW_KEY_2&&key<=GLFW_KEY_9))){host->joinCode.push_back(static_cast<char>(key));host->game.setNetworkRoom(host->joinCode.c_str(),"ENTER CODE",false);if(host->joinCode.size()==6){host->multiplayer.join(host->multiplayerService,host->joinCode);host->enteringJoinCode=false;}return;}
        return;
    }
    if(action==GLFW_PRESS&&!host->game.state().started&&key==GLFW_KEY_H){host->multiplayer.host(host->multiplayerService);return;}
    if(action==GLFW_PRESS&&!host->game.state().started&&key==GLFW_KEY_J){host->enteringJoinCode=true;host->joinCode.clear();host->game.setNetworkRoom("","ENTER ROOM CODE",false);return;}
    if (action == GLFW_PRESS && !host->game.state().started && host->multiplayer.role()==DesktopMultiplayer::Role::Offline &&
        (key == GLFW_KEY_ENTER || key == GLFW_KEY_R || key == GLFW_KEY_SPACE)) {
        host->game.restart();
        setMouseCaptured(window, *host, true);
        return;
    }

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (host->mouseCaptured) {
            setMouseCaptured(window, *host, false);
        } else if (soloPauseMenu(host->game.state())) {
            controllerMenuBack(window, *host);
        } else {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        return;
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        setMouseCaptured(window, *host, !host->mouseCaptured);
        return;
    }

    if(action==GLFW_PRESS&&host->game.state().started&&!host->game.state().uiPaused&&key>=GLFW_KEY_1&&key<=GLFW_KEY_4){
        host->game.setCommSignal(key-GLFW_KEY_1+1);
        return;
    }

    const int androidKey = androidKeyForGlfw(host->game.state().localSettings,key);
    if (androidKey >= 0) {
        host->game.setKey(androidKey, action != GLFW_RELEASE);
    }
}

void cursorCallback(GLFWwindow* window, double x, double y) {
    HostState* host = stateFor(window);
    if (!host) return;
    if(!host->mouseCaptured){const int hovered=menuItemAt(window,*host,x,y);if(hovered>=0)setMenuSelection(*host,hovered);return;}

    if (!host->haveMouse) {
        host->lastMouseX = x;
        host->lastMouseY = y;
        host->haveMouse = true;
        return;
    }

    if(host->game.state().player.grabbedByTarget>=0){const double delta=x-host->lastMouseX;host->lastMouseX=x;host->lastMouseY=y;if(std::abs(delta)>=1.0)host->game.setWiggle(static_cast<float>(delta));return;}

    host->lookX += x - host->lastMouseX;
    host->lookY += y - host->lastMouseY;
    host->lastMouseX = x;
    host->lastMouseY = y;
}

void framebufferCallback(GLFWwindow* window, int width, int height) {
    HostState* host = stateFor(window);
    if (host) host->renderer.resize(width, height);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    HostState* host = stateFor(window);
    if (!host || action != GLFW_PRESS) return;
    if(menuItemCount(host->game.state())>0){
        double x=0,y=0;glfwGetCursorPos(window,&x,&y);const int hovered=menuItemAt(window,*host,x,y);
        if(hovered>=0)setMenuSelection(*host,hovered);
        if(button==GLFW_MOUSE_BUTTON_LEFT||button==GLFW_MOUSE_BUTTON_RIGHT)activateMenuSelection(window,*host);
        return;
    }
    if(button==GLFW_MOUSE_BUTTON_RIGHT && !host->game.state().dead) {
        host->game.setTouchControls(0,0,0,0,false,false,false,true,false,false);
        return;
    }
    if(button!=GLFW_MOUSE_BUTTON_LEFT) return;
    if (!host->game.state().started) {
        if(host->multiplayer.role()!=DesktopMultiplayer::Role::Offline)return;
        host->game.restart();
        setMouseCaptured(window, *host, true);
        return;
    }
    if (!host->mouseCaptured) {
        setMouseCaptured(window, *host, true);
    }
}

void windowFocusCallback(GLFWwindow* window, int focused) {
    HostState* host = stateFor(window);
    if (!host) return;
    const bool wasFocused = host->focused;
    host->focused = focused == GLFW_TRUE;
    host->game.clearInputState();
    resetGamepadHistory(*host);
    if (!host->focused) {
        const GameState& state = host->game.state();
        host->restoreCaptureOnFocus = host->mouseCaptured && state.started && state.multiplayer.enabled && !state.dead && !state.upgradeMenu.active;
        setMouseCaptured(window, *host, false);
    } else if (!wasFocused) {
        host->lookX = 0.0;
        host->lookY = 0.0;
        host->haveMouse = false;
        const GameState& state = host->game.state();
        if (host->restoreCaptureOnFocus && state.started && !state.dead && !state.upgradeMenu.active) {
            setMouseCaptured(window, *host, true);
        }
        host->restoreCaptureOnFocus = false;
    }
}

void errorCallback(int code, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, description ? description : "unknown");
}

bool hasArg(int argc, char** argv, const char* expected) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], expected) == 0) return true;
    }
    return false;
}

bool near(float a, float b, float eps = 0.025f) {
    return std::abs(a - b) <= eps;
}

int runParityProximityTest() {
    Game game;
    game.reset();
    GameState& s = const_cast<GameState&>(game.state());
    bool ok = true;

    const float playerRadius = 0.34f;
    const float cameraRadius = 0.42f;
    const float minWallX = -30.0f * 0.5f + 1.1f;
    const float maxWallX =  30.0f * 0.5f - 1.1f;

    s.player.pos = {minWallX - 0.25f, 0.08f, 0.0f};
    s.player.vel = {-4.0f, 0.0f, 0.0f};
    game.update(1.0f / 60.0f);
    ok &= near(s.player.pos.x, minWallX);
    std::printf("PARITY left_wall_stop x=%.3f expected=%.3f\n", s.player.pos.x, minWallX);

    s.player.pos = {maxWallX + 0.25f, 0.08f, 0.0f};
    s.player.vel = {4.0f, 0.0f, 0.0f};
    game.update(1.0f / 60.0f);
    ok &= near(s.player.pos.x, maxWallX);
    std::printf("PARITY right_wall_stop x=%.3f expected=%.3f\n", s.player.pos.x, maxWallX);

    const RoomCollider& c = s.roomColliders[0];
    s.player.grounded = true;
    s.player.pos = {c.minX - playerRadius + 0.10f, 0.08f, (c.minZ + c.maxZ) * 0.5f};
    s.player.vel = {2.0f, 0.0f, 0.0f};
    game.update(1.0f / 60.0f);
    ok &= near(s.player.pos.x, c.minX - playerRadius);
    std::printf("PARITY obstacle_face_stop x=%.3f expected=%.3f radius=%.2f\n", s.player.pos.x, c.minX - playerRadius, playerRadius);

    s.player.pos = {c.minX - playerRadius + 0.10f, 0.08f, c.minZ - playerRadius + 0.12f};
    s.player.vel = {2.0f, 0.0f, 2.0f};
    game.update(1.0f / 60.0f);
    const bool cornerResolved = s.player.pos.x <= c.minX - playerRadius + 0.025f || s.player.pos.z <= c.minZ - playerRadius + 0.025f;
    ok &= cornerResolved;
    std::printf("PARITY obstacle_corner x=%.3f z=%.3f limits=(%.3f,%.3f)\n", s.player.pos.x, s.player.pos.z, c.minX - playerRadius, c.minZ - playerRadius);

    s.player.pos = {(c.minX + c.maxX) * 0.5f, c.topY + 0.08f, (c.minZ + c.maxZ) * 0.5f};
    s.player.vel = {0.0f, 0.0f, 0.0f};
    s.player.grounded = true;
    game.update(1.0f / 60.0f);
    ok &= near(s.player.pos.y, c.topY + 0.08f);
    std::printf("PARITY support_on_obstacle y=%.3f expected=%.3f support_radius=%.2f\n", s.player.pos.y, c.topY + 0.08f, playerRadius);

    s.player.pos = {0.0f, 0.08f, 0.0f};
    s.camera.firstPerson = false;
    s.camera.yaw = 3.14159265f * 0.5f;
    s.camera.pitch = 0.0f;
    game.update(1.0f / 60.0f);
    ok &= s.camera.pos.x >= minWallX - 3.0f && s.camera.pos.x <= 30.0f * 0.5f - cameraRadius + 0.001f;
    std::printf("PARITY camera_open pos=(%.3f,%.3f,%.3f) radius=%.2f\n", s.camera.pos.x, s.camera.pos.y, s.camera.pos.z, cameraRadius);

    s.player.pos = {maxWallX - 0.05f, 0.08f, 0.0f};
    s.camera.yaw = 3.14159265f * 0.5f;
    game.update(1.0f / 60.0f);
    ok &= s.camera.pos.x <= 30.0f * 0.5f - cameraRadius + 0.001f;
    std::printf("PARITY camera_wall_compress x=%.3f max=%.3f\n", s.camera.pos.x, 30.0f * 0.5f - cameraRadius);

    std::printf("PARITY_PROXIMITY_%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}

int runSmokeTest() {
    Game game;
    game.reset();

    for (int i = 0; i < 8; ++i) {
        game.update(1.0f / 60.0f);
    }

    const GameState& state = game.state();
    if (state.frame != 8 || state.roomIndex < 1 || !state.player.alive) {
        std::fprintf(stderr, "SMOKE_TEST_FAILED frame=%d room=%d alive=%d\n",
                     state.frame,
                     state.roomIndex,
                     state.player.alive ? 1 : 0);
        return 1;
    }

    std::printf(
        "SMOKE_TEST_OK frame=%d room=%d battery=%.2f targets=%d\n",
        state.frame,
        state.roomIndex,
        state.player.battery,
        TARGET_COUNT
    );
    return 0;
}
const char* argValue(int argc,char** argv,const char* expected){for(int i=1;i+1<argc;++i)if(std::strcmp(argv[i],expected)==0)return argv[i+1];return nullptr;}
bool captureFramebuffer(const std::filesystem::path& path,int width,int height){std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3u);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());std::ofstream out(path,std::ios::binary);if(!out)return false;out<<"P6\n"<<width<<" "<<height<<"\n255\n";for(int y=height-1;y>=0;--y)out.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(y)*width*3u),static_cast<std::streamsize>(width*3));return static_cast<bool>(out);}

int runModelTest(const std::filesystem::path& root) {
    HumanModelData human;StaticModelData phone,flower;
    if(!human.load((root/"models"/"human.dbhuman").string())||!phone.load((root/"models"/"phone.dbmesh").string())||!flower.load((root/"models"/"flower.dbmesh").string())){std::fprintf(stderr,"MODEL_TEST_FAILED load\n");return 1;}
    std::vector<float> idle,walk,attack;human.skin(0.0f,0.0f,0,idle);human.skin(0.25f,0.0f,0,walk);human.skin(0.25f,0.24f,1,attack);
    auto finite=[](const std::vector<float>& v){return !v.empty()&&std::all_of(v.begin(),v.end(),[](float x){return std::isfinite(x)&&std::abs(x)<100.0f;});};
    auto differs=[](const std::vector<float>& a,const std::vector<float>& b){if(a.size()!=b.size())return true;for(std::size_t i=0;i<a.size();++i)if(std::abs(a[i]-b[i])>0.00001f)return true;return false;};
    float minY=100,maxY=-100;for(std::size_t i=1;i<idle.size();i+=3){minY=std::min(minY,idle[i]);maxY=std::max(maxY,idle[i]);}
    const bool ok=human.vertices.size()==4164&&human.bones.size()==33&&human.frameCount==60&&phone.vertices.size()/3==253740&&phone.batches.size()==30&&flower.vertices.size()/3==11628&&finite(idle)&&finite(walk)&&finite(attack)&&differs(idle,walk)&&differs(walk,attack)&&minY>-0.03f&&maxY>1.0f&&maxY<1.25f;
    std::printf("MODEL_TEST_%s human=%zu bones=%zu frames=%u phone=%zu/%zu flower=%zu y=[%.4f,%.4f] walk=%d attack=%d\n",ok?"OK":"FAILED",human.vertices.size(),human.bones.size(),human.frameCount,phone.vertices.size()/3,phone.batches.size(),flower.vertices.size()/3,minY,maxY,differs(idle,walk)?1:0,differs(walk,attack)?1:0);return ok?0:1;
}

int runControllerTest(){
    int found=0;
    for(int jid=GLFW_JOYSTICK_1;jid<=GLFW_JOYSTICK_LAST;++jid){
        if(!glfwJoystickPresent(jid))continue;
        ++found;
        int axisCount=0,buttonCount=0,hatCount=0;
        const float* axes=glfwGetJoystickAxes(jid,&axisCount);
        const unsigned char* buttons=glfwGetJoystickButtons(jid,&buttonCount);
        const unsigned char* hats=glfwGetJoystickHats(jid,&hatCount);
        const int mapped=glfwJoystickIsGamepad(jid);
        std::printf("CONTROLLER jid=%d name=\"%s\" guid=%s mapped=%d gamepad=\"%s\" axes=%d buttons=%d hats=%d\n",
            jid,glfwGetJoystickName(jid)?glfwGetJoystickName(jid):"",glfwGetJoystickGUID(jid)?glfwGetJoystickGUID(jid):"",mapped,mapped&&glfwGetGamepadName(jid)?glfwGetGamepadName(jid):"",axisCount,buttonCount,hatCount);
        std::printf("  axes:");
        for(int i=0;i<axisCount&&i<8;++i)std::printf(" %d=%.3f",i,axes?axes[i]:0.0f);
        std::printf("\n  buttons:");
        for(int i=0;i<buttonCount&&i<16;++i)std::printf(" %d=%d",i,buttons&&buttons[i]==GLFW_PRESS?1:0);
        std::printf("\n  hats:");
        for(int i=0;i<hatCount&&i<4;++i)std::printf(" %d=%u",i,hats?static_cast<unsigned int>(hats[i]):0u);
        std::printf("\n");
    }
    std::printf("CONTROLLER_TEST_%s count=%d\n",found>0?"OK":"NO_CONTROLLERS",found);
    return found>0?0:2;
}

int runControllerLiveTest(){
    int jid=-1;
    for(int candidate=GLFW_JOYSTICK_1;candidate<=GLFW_JOYSTICK_LAST;++candidate)if(glfwJoystickPresent(candidate)){jid=candidate;break;}
    if(jid<0){std::printf("CONTROLLER_LIVE_NO_CONTROLLERS\n");return 2;}
    std::printf("CONTROLLER_LIVE_START jid=%d name=\"%s\" guid=%s mode=%s\n",jid,glfwGetJoystickName(jid)?glfwGetJoystickName(jid):"",glfwGetJoystickGUID(jid)?glfwGetJoystickGUID(jid):"",preferRawXboxLayout(jid)?"xbox-raw-macos":"glfw/generic");
    std::printf("Press sticks/buttons/triggers now; this samples for 12 seconds.\n");
    std::array<unsigned char,32> previousButtons{};
    std::array<unsigned char,8> previousHats{};
    std::array<int,8> previousAxisBuckets{};
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(12);
    while(std::chrono::steady_clock::now()<end){
        glfwPollEvents();
        int axisCount=0,buttonCount=0,hatCount=0;
        const float* axes=glfwGetJoystickAxes(jid,&axisCount);
        const unsigned char* buttons=glfwGetJoystickButtons(jid,&buttonCount);
        const unsigned char* hats=glfwGetJoystickHats(jid,&hatCount);
        bool changed=false;
        for(int i=0;i<std::min(axisCount,8);++i){const int bucket=static_cast<int>(std::round((axes?axes[i]:0.0f)*10.0f));if(bucket!=previousAxisBuckets[i]){previousAxisBuckets[i]=bucket;changed=true;}}
        for(int i=0;i<std::min(buttonCount,32);++i){const unsigned char value=buttons?buttons[i]:0;if(value!=previousButtons[i]){previousButtons[i]=value;changed=true;}}
        for(int i=0;i<std::min(hatCount,8);++i){const unsigned char value=hats?hats[i]:0;if(value!=previousHats[i]){previousHats[i]=value;changed=true;}}
        if(changed){
            auto raw=[&](int button){return button<buttonCount&&button<32&&previousButtons[button]==GLFW_PRESS;};
            const bool xbox=preferRawXboxLayout(jid);
            const bool a=raw(0),b=raw(1),x=xbox?raw(3):raw(2),y=xbox?raw(4):raw(3);
            const bool lb=xbox?raw(6):raw(4),rb=xbox?raw(7):raw(5),start=xbox?raw(11):(raw(7)||raw(9)),ls=xbox?raw(13):raw(8),rs=xbox?raw(14):raw(9);
            const float lx=axisCount>0?gamepadAxis(axes[0],0.12f):0.0f,ly=axisCount>1?gamepadAxis(axes[1],0.12f):0.0f,rx=axisCount>2?gamepadLookAxis(axes[2]):0.0f,ry=axisCount>3?gamepadLookAxis(axes[3]):0.0f;
            const float lt=xbox?(axisCount>5?axes[5]:-1.0f):(axisCount>4?axes[4]:-1.0f),rt=xbox?(axisCount>4?axes[4]:-1.0f):(axisCount>5?axes[5]:-1.0f);
            std::printf("SEM A=%d B=%d X=%d Y=%d LB=%d RB=%d LT=%.2f RT=%.2f LS=%d RS=%d START=%d LX=%.2f LY=%.2f RX=%.2f RY=%.2f raw_buttons:",a,b,x,y,lb,rb,lt,rt,ls,rs,start,lx,ly,rx,ry);
            for(int i=0;i<std::min(buttonCount,16);++i)if(raw(i))std::printf(" b%d",i);
            std::printf(" hats:");
            for(int i=0;i<std::min(hatCount,4);++i)if(previousHats[i])std::printf(" h%d=%u",i,static_cast<unsigned int>(previousHats[i]));
            std::printf("\n");
            std::fflush(stdout);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::printf("CONTROLLER_LIVE_DONE\n");
    return 0;
}
}

int main(int argc, char** argv) {
    const bool captureHuman=argValue(argc,argv,"--capture-human-frame")!=nullptr;
    const bool captureSoul=argValue(argc,argv,"--capture-soul-frame")!=nullptr;
    const bool captureStart=argValue(argc,argv,"--capture-start-frame")!=nullptr;
    const bool capturePaused=argValue(argc,argv,"--capture-paused-frame")!=nullptr;
    const bool captureMosh=argValue(argc,argv,"--capture-mosh-frame")!=nullptr;
    const bool capturePhone=argValue(argc,argv,"--capture-phone-frame")!=nullptr;
    const char* capturePath=captureHuman?argValue(argc,argv,"--capture-human-frame"):(captureSoul?argValue(argc,argv,"--capture-soul-frame"):(captureStart?argValue(argc,argv,"--capture-start-frame"):(capturePaused?argValue(argc,argv,"--capture-paused-frame"):(captureMosh?argValue(argc,argv,"--capture-mosh-frame"):(capturePhone?argValue(argc,argv,"--capture-phone-frame"):argValue(argc,argv,"--capture-frame"))))));
    if (hasArg(argc, argv, "--smoke-test")) {
        return runSmokeTest();
    }
    if (hasArg(argc, argv, "--parity-proximity-test")) {
        return runParityProximityTest();
    }
    if (hasArg(argc, argv, "--model-test")) {
        return runModelTest(std::filesystem::absolute(argv[0]).parent_path());
    }

    glfwSetErrorCallback(errorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Digital Breakdown: GLFW initialization failed.\n");
        return 1;
    }
    if (hasArg(argc, argv, "--controller-test")) {
        const int result=runControllerTest();
        glfwTerminate();
        return result;
    }
    if (hasArg(argc, argv, "--controller-live-test")) {
        const int result=runControllerLiveTest();
        glfwTerminate();
        return result;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    // Browser reference creates WebGL with antialias:true. Four samples are a
    // modest desktop cost and remove the most visible geometry/crosshair jaggies.
    glfwWindowHint(GLFW_SAMPLES, 4);
    if(capturePath)glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
        "Digital Breakdown - Native Desktop",
        nullptr,
        nullptr
    );
    if (!window) {
        glfwTerminate();
        std::fprintf(stderr, "Digital Breakdown: window creation failed.\n");
        return 1;
    }

    HostState host;
    host.progressionPath=progressionSavePath();
    loadProgression(host.game,host.progressionPath);
    if(const char* service=std::getenv("DIGITAL_BREAKDOWN_MULTIPLAYER_URL"))host.multiplayerService=service;
    host.game.reset();
    if(!capturePath||captureStart)host.game.prepareStartScreen();
    host.savedProgressionRevision=host.game.state().progression.permanent.revision;
    if(captureHuman){GameState& fixture=const_cast<GameState&>(host.game.state());for(auto& target:fixture.targets)target.alive=false;auto& target=fixture.targets[0];target.alive=true;target.slurpable=false;target.pos={0,0.08f,fixture.player.pos.z-4.0f};target.walkTarget=target.pos;target.visualYaw=0;target.scale=1;target.visibility=1;target.attackCooldown=999;fixture.camera.yaw=0;fixture.camera.pitch=0;}
    if(captureSoul){GameState& fixture=const_cast<GameState&>(host.game.state());for(int i=1;i<TARGET_COUNT;++i)fixture.targets[i].alive=false;auto& target=fixture.targets[0];target.alive=true;target.slurpable=true;target.soulMorph=1;target.soulCubeAmount=1;target.pos=fixture.player.pos+Vec3{0,0.5f,-1.5f};target.walkTarget=target.pos;target.health=1;target.armor=0;target.soulState=SoulState::Free;fixture.camera.yaw=0;fixture.camera.pitch=0;}
    if(capturePaused)host.game.setUiPaused(true);
    if(capturePhone){GameState& fixture=const_cast<GameState&>(host.game.state());for(auto& target:fixture.targets)target.alive=false;}
    host.audio.setAssetRoot(std::filesystem::absolute(argv[0]).parent_path()/"audio");

    glfwSetWindowUserPointer(window, &host);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetWindowFocusCallback(window, windowFocusCallback);
    glfwSetFramebufferSizeCallback(window, framebufferCallback);

    glfwMakeContextCurrent(window);
    host.renderer.setAssetRoot(std::filesystem::absolute(argv[0]).parent_path()/"models");
    // A software deadline below owns the 60 Hz presentation rate. Disabling
    // monitor-rate vsync avoids running gameplay at 120/144 Hz or being
    // double-throttled on displays whose refresh is not an even multiple of 60.
    glfwSwapInterval(0);
    setMouseCaptured(window, host, host.game.state().started);
    if(capturePaused)host.game.setUiPaused(true);

    int framebufferWidth = 1;
    int framebufferHeight = 1;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    host.renderer.resize(framebufferWidth, framebufferHeight);
    if(const char* service=argValue(argc,argv,"--service-url"))host.multiplayerService=service;
    if(hasArg(argc,argv,"--host-room"))host.multiplayer.host(host.multiplayerService);
    if(const char* room=argValue(argc,argv,"--join-room"))host.multiplayer.join(host.multiplayerService,room);

    std::printf("Digital Breakdown native desktop host running.\n");
    std::printf("WASD move | Shift sprint | Space jump | Mouse look | Left mouse vacuum | F melee | Q shoot | C camera | Tab release mouse | Esc quit\n");

    auto previous = std::chrono::steady_clock::now();
    auto nextPresentation = previous;
    double simulationAccumulator = 0.0;
    int captureFrames=0;
    while (!glfwWindowShouldClose(window)) {
        if (!capturePath) {
            const auto beforePacing = std::chrono::steady_clock::now();
            if (beforePacing < nextPresentation) std::this_thread::sleep_until(nextPresentation);
        }
        glfwPollEvents();

        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        if (!capturePath) {
            if (now > nextPresentation + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(PRESENTATION_STEP_SECONDS * 4.0))) nextPresentation = now;
            nextPresentation += std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(PRESENTATION_STEP_SECONDS));
            simulationAccumulator += std::min(elapsed, MAX_FRAME_DELTA_SECONDS);
        }

        const DesktopGamepadInput gamepad=pollGamepad(window,host);
        const bool vacuumHeld = captureSoul || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS || gamepad.vacuumHeld;
        const bool sprintHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS || gamepad.sprintHeld;

        host.game.setTouchControls(
            gamepad.moveX,
            gamepad.moveZ,
            static_cast<float>(host.lookX)*host.game.state().localSettings.mouseLookSensitivity+gamepad.lookX*host.game.state().localSettings.controllerLookSensitivity,
            static_cast<float>(host.lookY)*host.game.state().localSettings.mouseLookSensitivity+gamepad.lookY*host.game.state().localSettings.controllerLookSensitivity,
            vacuumHeld,
            sprintHeld,
            gamepad.jumpPressed,
            gamepad.meleePressed,
            gamepad.shootPressed,
            gamepad.cameraPressed
        );
        host.lookX = 0.0;
        host.lookY = 0.0;

        host.multiplayer.update(host.game);
        if((!host.game.state().started||host.game.state().upgradeMenu.active)&&host.mouseCaptured)setMouseCaptured(window,host,false);
        if (host.game.state().started && host.game.state().multiplayer.connected && !host.game.state().upgradeMenu.active && !host.game.state().uiPaused && !host.mouseCaptured) {
            setMouseCaptured(window, host, true);
        }

        if(captureMosh&&captureFrames==10){GameState& fixture=const_cast<GameState&>(host.game.state());fixture.doorTransition.active=true;fixture.doorTransition.progress=1.0f;fixture.doorTransition.distanceTravelled=0;fixture.doorTransition.lastPlayerPos=fixture.player.pos;}
        if (capturePath) {
            host.game.update(static_cast<float>(PRESENTATION_STEP_SECONDS));
        } else {
            int simulationSteps = 0;
            while (simulationAccumulator >= PRESENTATION_STEP_SECONDS && simulationSteps < MAX_SIMULATION_STEPS_PER_FRAME) {
                host.game.update(static_cast<float>(PRESENTATION_STEP_SECONDS));
                simulationAccumulator -= PRESENTATION_STEP_SECONDS;
                ++simulationSteps;
            }
            if (simulationSteps == MAX_SIMULATION_STEPS_PER_FRAME && simulationAccumulator >= PRESENTATION_STEP_SECONDS) simulationAccumulator = 0.0;
        }
        if(capturePhone){GameState& fixture=const_cast<GameState&>(host.game.state());fixture.camera.pos=fixture.phoneTransform.position+Vec3{0,0.035f,0.38f};fixture.camera.lookTarget=fixture.phoneTransform.position;fixture.camera.forward=normalized(fixture.camera.lookTarget-fixture.camera.pos);}
        host.audio.update(host.game.state());
        const auto& permanent=host.game.state().progression.permanent;if((host.game.state().frame%60)==0||permanent.revision!=host.savedProgressionRevision){if(saveProgression(permanent,host.game.state().localSettings,host.progressionPath))host.savedProgressionRevision=permanent.revision;}
        host.renderer.draw(host.game.state());
        if(capturePath&&++captureFrames>=30){const bool captured=captureFramebuffer(capturePath,framebufferWidth,framebufferHeight);std::printf("CAPTURE_FRAME_%s %s\n",captured?"OK":"FAILED",capturePath);glfwSetWindowShouldClose(window,GLFW_TRUE);}
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    host.audio.stopAll();
    host.multiplayer.disconnect();
    glfwTerminate();
    return 0;
}
