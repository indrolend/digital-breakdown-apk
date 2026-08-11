#include "DesktopRenderer.hpp"
#include "DesktopAudio.hpp"
#include "DesktopMultiplayer.hpp"
#include "DesktopUpdateService.hpp"
#include "BuildIdentity.hpp"
#include "MenuNavigation.hpp"
#include "ControllerRumble.hpp"
#include "Game.hpp"
#include "gameplay/TargetRoles.hpp"
#include "PhoneDisplayLayout.hpp"

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
#include <iomanip>
#include <memory>
#include <numeric>
#include <vector>
#include <string>
#include <cstdlib>
#include <thread>

namespace {
constexpr double SIMULATION_STEP_SECONDS = 1.0 / 60.0;
constexpr double MAX_FRAME_DELTA_SECONDS = 0.10;
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

bool samePersistentSettings(const LocalSettingsState& a,const LocalSettingsState& b){
    return a.musicVolume==b.musicVolume&&a.sfxVolume==b.sfxVolume&&
        a.musicMuted==b.musicMuted&&a.sfxMuted==b.sfxMuted&&
        a.graphicsPreset==b.graphicsPreset&&a.shadows==b.shadows&&
        a.portalWindow==b.portalWindow&&a.particles==b.particles&&
        a.fpsCounter==b.fpsCounter&&a.mouseLookSensitivity==b.mouseLookSensitivity&&
        a.touchLookSensitivity==b.touchLookSensitivity&&
        a.controllerLookSensitivity==b.controllerLookSensitivity&&
        a.controllerTriggerSensitivity==b.controllerTriggerSensitivity&&
        a.controllerVibration==b.controllerVibration&&
        a.keyboardBindings==b.keyboardBindings;
}

struct HostState {
    Game game;
    DesktopRenderer renderer;
    DesktopAudio audio;
    DesktopMultiplayer multiplayer;
    DesktopUpdateService updater;
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
    bool suppressLeftMouseUntilRelease = false;
    int gamepadId = -1;
    bool gamepadMapped = false;
    bool restoreCaptureOnFocus = false;
    std::array<unsigned char, GLFW_GAMEPAD_BUTTON_LAST + 1> previousGamepadButtons{};
    bool previousGamepadLeftTrigger = false;
    bool previousGamepadRightTrigger = false;
    bool previousGamepadMenuLeft = false;
    bool previousGamepadMenuRight = false;
    bool previousGamepadMenuUp = false;
    bool previousGamepadMenuDown = false;
    unsigned int lastHapticAudioSerial = 0;
    unsigned int previousMeleeHitMask = 0;
    std::array<int,3> previousPermanentLevels{};
    bool previousPlayerAlive = true;
    std::filesystem::path progressionPath;
    std::uint64_t savedProgressionRevision = 0;
    LocalSettingsState savedSettings;
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
void rumblePulse(const LocalSettingsState& settings,float low,float high,int milliseconds){
    if(settings.controllerVibration<=0)return;
    const float scale=settings.controllerVibration==1?1.0f:1.30f;
    controllerRumblePulse(clampf(low*scale,0.0f,1.0f),clampf(high*scale,0.0f,1.0f),milliseconds);
}
void resetGamepadHistory(HostState& host){host.previousGamepadButtons.fill(GLFW_RELEASE);host.previousGamepadLeftTrigger=false;host.previousGamepadRightTrigger=false;host.previousGamepadMenuLeft=host.previousGamepadMenuRight=host.previousGamepadMenuUp=host.previousGamepadMenuDown=false;controllerRumbleStop();}
bool preferRawXboxLayout(int jid){const char* guid=glfwGetJoystickGUID(jid);return guid&&std::strcmp(guid,"030000005e040000130b000013050000")==0;}

void updateOutcomeRumble(HostState& host){
    const GameState& state=host.game.state();
    int priority=0,duration=0;float low=0.0f,high=0.0f;
    const auto offer=[&](int candidatePriority,float candidateLow,float candidateHigh,int candidateDuration){
        if(candidatePriority<=priority)return;priority=candidatePriority;low=candidateLow;high=candidateHigh;duration=candidateDuration;
    };
    const unsigned int newest=state.audio.nextSerial>0?state.audio.nextSerial-1:0;
    const unsigned int first=std::max(host.lastHapticAudioSerial+1,newest>=AUDIO_EVENT_COUNT?newest-AUDIO_EVENT_COUNT+1:1u);
    for(unsigned int serial=first;serial<=newest;++serial){
        const AudioEventState& event=state.audio.events[(serial-1u)%AUDIO_EVENT_COUNT];
        if(event.serial!=serial)continue;
        switch(event.cue){
            case AudioCue::PaymentSuccess: offer(6,0.38f,0.45f,85);break;
            case AudioCue::Capture1:case AudioCue::Capture2:case AudioCue::Capture3:case AudioCue::Capture4:case AudioCue::Capture5:
                offer(4,0.20f,0.32f,55);break;
            case AudioCue::HeadshotCritical:offer(5,0.28f,0.52f,65);break;
            case AudioCue::Headshot:offer(3,0.12f,0.36f,40);break;
            case AudioCue::NegativeAck:offer(5,0.38f,0.08f,75);break;
            default:break;
        }
    }
    host.lastHapticAudioSerial=newest;
    if(state.meleeVisual.hitMask!=host.previousMeleeHitMask&&(state.meleeVisual.hitMask&~host.previousMeleeHitMask)!=0)
        offer(3,0.32f,0.12f,45);
    host.previousMeleeHitMask=state.meleeVisual.hitMask;
    for(int track=0;track<3;++track)if(state.progression.permanent.levels[track]>host.previousPermanentLevels[track])
        offer(5,0.22f,0.36f,65);
    host.previousPermanentLevels=state.progression.permanent.levels;
    if(host.previousPlayerAlive&&!state.player.alive)offer(7,0.55f,0.10f,140);
    host.previousPlayerAlive=state.player.alive;
    if(priority>0)rumblePulse(state.localSettings,low,high,duration);
}

std::filesystem::path progressionSavePath(){
    const char* overridePath=std::getenv("DB_SAVE_PATH");
    if(overridePath&&*overridePath)return std::filesystem::path(overridePath);
#ifdef _WIN32
    const char* local=std::getenv("LOCALAPPDATA");
    const std::filesystem::path root=local&&*local?std::filesystem::path(local):std::filesystem::temp_directory_path();
    return root/"DigitalBreakdown"/"progression.v1";
#elif defined(__APPLE__)
    const char* home=std::getenv("HOME");
    const std::filesystem::path root=home&&*home?std::filesystem::path(home):std::filesystem::temp_directory_path();
    return root/"Library"/"Application Support"/"DigitalBreakdown"/"progression.v1";
#else
    const char* data=std::getenv("XDG_DATA_HOME");
    if(data&&*data)return std::filesystem::path(data)/"DigitalBreakdown"/"progression.v1";
    const char* home=std::getenv("HOME");
    const std::filesystem::path root=home&&*home?std::filesystem::path(home)/".local"/"share":std::filesystem::temp_directory_path();
    return root/"DigitalBreakdown"/"progression.v1";
#endif
}
std::filesystem::path legacyTemporaryProgressionSavePath(){return std::filesystem::temp_directory_path()/"DigitalBreakdown"/"progression.v1";}
std::filesystem::path progressionBackupPath(const std::filesystem::path& path){return path.wstring()+L".bak";}
bool loadProgression(Game& game,const std::filesystem::path& path){
    std::ifstream input(path);std::string magic;int version=0,shot=0,lunge=0,attack=0;long long tokens=0;
    if(!(input>>magic>>version>>tokens>>shot>>lunge>>attack)||magic!="DBPROG"||version<1||version>4)return false;
    LocalSettingsState settings=game.state().localSettings;
    if(version>=2){
        if(!(input>>settings.musicVolume>>settings.sfxVolume>>settings.musicMuted>>settings.sfxMuted>>settings.graphicsPreset>>settings.shadows>>settings.portalWindow>>settings.particles>>settings.fpsCounter>>settings.mouseLookSensitivity>>settings.touchLookSensitivity>>settings.controllerLookSensitivity))return false;
        if(version>=3&&!(input>>settings.controllerTriggerSensitivity))return false;
        if(version>=4&&!(input>>settings.controllerVibration))return false;
        for(int& key:settings.keyboardBindings)if(!(input>>key))return false;
        if(!std::isfinite(settings.musicVolume)||!std::isfinite(settings.sfxVolume)||!std::isfinite(settings.mouseLookSensitivity)||!std::isfinite(settings.touchLookSensitivity)||!std::isfinite(settings.controllerLookSensitivity))return false;
        settings.musicVolume=clampf(settings.musicVolume,0,1);settings.sfxVolume=clampf(settings.sfxVolume,0,1);
        settings.graphicsPreset=std::max(0,std::min(2,settings.graphicsPreset));
        settings.mouseLookSensitivity=clampf(settings.mouseLookSensitivity,0.5f,1.75f);settings.touchLookSensitivity=clampf(settings.touchLookSensitivity,0.5f,1.75f);settings.controllerLookSensitivity=clampf(settings.controllerLookSensitivity,0.5f,1.75f);
        settings.controllerTriggerSensitivity=std::max(0,std::min(2,settings.controllerTriggerSensitivity));settings.controllerVibration=std::max(0,std::min(2,settings.controllerVibration));
        settings.menuPage=LocalMenuPage::Main;settings.rebindingAction=settings.pendingBinding=settings.conflictingAction=-1;
    }
    game.setPersistentProgression(tokens,shot,lunge,attack);
    if(version>=2)game.networkMutableState().localSettings=settings;
    return true;
}
bool replaceProgressionFile(const std::filesystem::path& source,const std::filesystem::path& destination){
    std::error_code error;
#ifdef _WIN32
    return MoveFileExW(source.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::filesystem::rename(source,destination,error);return !error;
#endif
}
bool saveProgression(const PermanentProgressionState& progression,const LocalSettingsState& settings,const std::filesystem::path& path){
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
    const std::filesystem::path temporary=path.wstring()+L".tmp";
    {std::ofstream output(temporary,std::ios::trunc);if(!output)return false;output<<"DBPROG 4 "<<progression.tokens<<' '<<progression.levels[0]<<' '<<progression.levels[1]<<' '<<progression.levels[2]<<' '<<settings.musicVolume<<' '<<settings.sfxVolume<<' '<<settings.musicMuted<<' '<<settings.sfxMuted<<' '<<settings.graphicsPreset<<' '<<settings.shadows<<' '<<settings.portalWindow<<' '<<settings.particles<<' '<<settings.fpsCounter<<' '<<settings.mouseLookSensitivity<<' '<<settings.touchLookSensitivity<<' '<<settings.controllerLookSensitivity<<' '<<settings.controllerTriggerSensitivity<<' '<<settings.controllerVibration;for(int key:settings.keyboardBindings)output<<' '<<key;output<<'\n';output.flush();if(!output)return false;}
    if(!replaceProgressionFile(temporary,path)){std::filesystem::remove(temporary,error);return false;}
    const std::filesystem::path backup=progressionBackupPath(path),backupTemporary=backup.wstring()+L".tmp";
    error.clear();
    std::filesystem::copy_file(path,backupTemporary,std::filesystem::copy_options::overwrite_existing,error);
    if(error||!replaceProgressionFile(backupTemporary,backup)){error.clear();std::filesystem::remove(backupTemporary,error);}
    return true;}
bool loadProgressionWithBackup(Game& game,const std::filesystem::path& path,bool* recovered=nullptr){
    if(recovered)*recovered=false;
    if(loadProgression(game,path))return true;
    if(!loadProgression(game,progressionBackupPath(path)))return false;
    if(recovered)*recovered=true;
    saveProgression(game.state().progression.permanent,game.state().localSettings,path);
    return true;
}

int runSaveRoundtripTest(){
    const auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root=std::filesystem::temp_directory_path()/("DigitalBreakdownSaveTest-"+std::to_string(nonce));
    const std::filesystem::path path=root/"progression.v1";
    Game source;
    source.setPersistentProgression(37,2,4,5);
    auto& settings=source.networkMutableState().localSettings;
    settings.musicVolume=0.31f;settings.sfxVolume=0.82f;settings.musicMuted=true;settings.sfxMuted=false;
    settings.graphicsPreset=2;settings.shadows=true;settings.portalWindow=false;settings.particles=true;settings.fpsCounter=true;
    settings.mouseLookSensitivity=1.30f;settings.touchLookSensitivity=0.80f;settings.controllerLookSensitivity=1.45f;
    settings.controllerTriggerSensitivity=2;settings.controllerVibration=2;
    settings.keyboardBindings[0]=73;settings.keyboardBindings[9]=88;
    LocalSettingsState transientSettings=settings;transientSettings.menuPage=LocalMenuPage::Controls;transientSettings.menuScroll=280.0f;transientSettings.menuHistoryDepth=2;transientSettings.rebindingAction=4;
    LocalSettingsState changedSettings=settings;changedSettings.sfxVolume=0.21f;
    LocalSettingsState reboundSettings=settings;reboundSettings.keyboardBindings[3]=74;
    const bool dirtyDetectionOk=samePersistentSettings(settings,transientSettings)&&!samePersistentSettings(settings,changedSettings)&&!samePersistentSettings(settings,reboundSettings);
    if(!saveProgression(source.state().progression.permanent,settings,path)){std::printf("SAVE_ROUNDTRIP_FAILED write\n");return 1;}
    Game restored;
    const bool loaded=loadProgression(restored,path);
    const auto& progression=restored.state().progression.permanent;
    const auto& loadedSettings=restored.state().localSettings;
    const bool matches=loaded&&progression.tokens==37&&progression.levels[0]==2&&progression.levels[1]==4&&progression.levels[2]==5&&
        std::abs(loadedSettings.musicVolume-0.31f)<0.001f&&std::abs(loadedSettings.sfxVolume-0.82f)<0.001f&&
        loadedSettings.musicMuted&&!loadedSettings.sfxMuted&&loadedSettings.graphicsPreset==2&&loadedSettings.shadows&&
        !loadedSettings.portalWindow&&loadedSettings.particles&&loadedSettings.fpsCounter&&
        std::abs(loadedSettings.mouseLookSensitivity-1.30f)<0.001f&&std::abs(loadedSettings.touchLookSensitivity-0.80f)<0.001f&&
        std::abs(loadedSettings.controllerLookSensitivity-1.45f)<0.001f&&loadedSettings.controllerTriggerSensitivity==2&&
        loadedSettings.controllerVibration==2&&loadedSettings.keyboardBindings[0]==73&&loadedSettings.keyboardBindings[9]==88;
    const auto legacyLoads=[&](int version){
        {std::ofstream legacy(path,std::ios::trunc);legacy<<"DBPROG "<<version<<" 23 1 2 3";if(version>=2){legacy<<" 0.4 0.6 1 0 1 0 1 1 0 1.2 0.9 1.4";if(version>=3)legacy<<" 2";for(int key:settings.keyboardBindings)legacy<<' '<<key;}legacy<<'\n';}
        Game legacyGame;if(!loadProgression(legacyGame,path))return false;const auto& legacyState=legacyGame.state();
        return legacyState.progression.permanent.tokens==23&&legacyState.progression.permanent.levels==std::array<int,3>{1,2,3}&&
            (version<2||(std::abs(legacyState.localSettings.musicVolume-0.4f)<0.001f&&legacyState.localSettings.keyboardBindings[0]==73))&&
            (version<3||legacyState.localSettings.controllerTriggerSensitivity==2);
    };
    const bool legacyVersionsOk=legacyLoads(1)&&legacyLoads(2)&&legacyLoads(3);
    {std::ofstream corrupt(path,std::ios::trunc);corrupt<<"DBPROG 4 999 5";}
    Game rejected;rejected.setPersistentProgression(11,1,1,1);
    const bool corruptRejected=!loadProgression(rejected,path)&&rejected.state().progression.permanent.tokens==11;
    bool recoveredFromBackup=false;Game recovered;
    const bool recoveredOk=loadProgressionWithBackup(recovered,path,&recoveredFromBackup)&&recoveredFromBackup&&recovered.state().progression.permanent.tokens==37&&recovered.state().progression.permanent.levels==std::array<int,3>{2,4,5};
    Game repaired;const bool primaryRepaired=loadProgression(repaired,path)&&repaired.state().progression.permanent.tokens==37;
    {std::ofstream future(path,std::ios::trunc);future<<"DBPROG 99 500 5 5 5\n";}
    Game futureRejected;const bool futureRejectedOk=!loadProgression(futureRejected,path);
    const bool allValid=matches&&dirtyDetectionOk&&legacyVersionsOk&&corruptRejected&&recoveredOk&&primaryRepaired&&futureRejectedOk;
    std::error_code cleanupError;std::filesystem::remove(path,cleanupError);std::filesystem::remove(progressionBackupPath(path),cleanupError);std::filesystem::remove(root,cleanupError);
    std::printf("SAVE_ROUNDTRIP_%s format=4 dirty_detection=%d legacy_versions=%d corruption_rejected=%d backup_recovered=%d primary_repaired=%d future_rejected=%d\n",allValid?"OK":"FAILED",dirtyDetectionOk?1:0,legacyVersionsOk?1:0,corruptRejected?1:0,recoveredOk?1:0,primaryRepaired?1:0,futureRejectedOk?1:0);
    return allValid?0:1;
}

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
bool multiplayerPauseMenu(const GameState& state){return state.started&&state.uiPaused&&state.multiplayer.enabled&&!state.upgradeMenu.active;}
int menuItemCount(const GameState& state);

Vec3 cross3(const Vec3& a,const Vec3& b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}

PhoneTransformState interpolatePhoneTransform(
    const PhoneTransformState& previous,
    const PhoneTransformState& current,
    float alpha
) {
    PhoneTransformState result = current;

    result.position =
        previous.position +
        (current.position - previous.position) * alpha;

    result.orientation =
        quatSlerp(previous.orientation, current.orientation, alpha);

    result.screenRight =
        normalized(rotate(result.orientation, {1.0f, 0.0f, 0.0f}));
    result.screenUp =
        normalized(rotate(result.orientation, {0.0f, 1.0f, 0.0f}));
    result.screenNormal =
        normalized(rotate(result.orientation, {0.0f, 0.0f, 1.0f}));

    const float previousScreenOffset =
        dot3(previous.screenCenter - previous.position, previous.screenNormal);
    const float currentScreenOffset =
        dot3(current.screenCenter - current.position, current.screenNormal);
    const float screenOffset =
        previousScreenOffset +
        (currentScreenOffset - previousScreenOffset) * alpha;

    result.screenCenter =
        result.position + result.screenNormal * screenOffset;

    result.vacuumPullPoint =
        previous.vacuumPullPoint +
        (current.vacuumPullPoint - previous.vacuumPullPoint) * alpha;

    return result;
}

int phoneMenuItemAt(const GameState& state,float cursorX,float cursorY,int fw,int fh){
    if(state.upgradeMenu.active||state.camera.firstPerson)return -1;
    if(state.started&&!soloPauseMenu(state))return -1;
    const PhoneDisplayMenuLayout layout=makePhoneDisplayMenuLayout(state);
    if(layout.selectableCount<=0)return -1;
    const PhoneTransformState& phone=state.phoneTransform;
    const Vec3 forward=normalized(state.camera.lookTarget-state.camera.pos);
    Vec3 right=normalized(cross3(forward,{0.0f,1.0f,0.0f}));
    if(lengthSq(right)<0.00001f)right={1.0f,0.0f,0.0f};
    const Vec3 up=cross3(right,forward);
    const float aspect=static_cast<float>(fw)/std::max(1,fh);
    const float tanHalf=std::tan(state.camera.verticalFovDegrees*DB_PI/360.0f);
    const float nx=cursorX/static_cast<float>(std::max(1,fw))*2.0f-1.0f;
    const float ny=1.0f-cursorY/static_cast<float>(std::max(1,fh))*2.0f;
    const Vec3 ray=normalized(forward+right*(nx*tanHalf*aspect)+up*(ny*tanHalf));
    const float denom=dot3(ray,phone.screenNormal);
    if(std::abs(denom)<0.00001f)return -1;
    const float t=dot3(phone.screenCenter-state.camera.pos,phone.screenNormal)/denom;
    if(t<=0.0f)return -1;
    const Vec3 hit=state.camera.pos+ray*t;
    const Vec3 local=hit-phone.screenCenter;
    const float screenW=PHONE_SCREEN_WIDTH*std::max(0.65f,state.phoneVisual.screenScale.x);
    const float screenH=PHONE_SCREEN_HEIGHT*std::max(0.65f,state.phoneVisual.screenScale.y);
    const float u=dot3(local,phone.screenRight)/screenW+0.5f;
    const float v=0.5f-dot3(local,phone.screenUp)/screenH;
    if(u<0.0f||u>1.0f||v<0.0f||v>1.0f)return -1;
    return phoneDisplayItemAt(layout,u*static_cast<float>(layout.logicalW),v*static_cast<float>(layout.logicalH));
}

int menuItemCount(const GameState& state) {
    if(state.upgradeMenu.active)return 6;
    if(state.dead)return 2;
    if(soloPauseMenu(state))return makePhoneMenuPageModel(state).selectableCount;
    if(!state.started){
        return makePhoneMenuPageModel(state).selectableCount;
    }
    if(state.uiPaused)return 1;
    return 0;
}

int deathMenuItemAt(const GameState& state,float x,float y,float canvasW,float canvasH){
    if(!state.dead)return -1;
    const float cx=canvasW*0.5f,cy=canvasH*0.56f;
    const auto rowAt=[&](const char* label,int index,float rowY,float scale)->int{
        const float tw=static_cast<float>(std::strlen(label))*6.0f*scale;
        const float padX=34.0f,padY=18.0f;
        if(x>=cx-tw*0.5f-padX&&x<=cx+tw*0.5f+padX&&y>=rowY-padY&&y<=rowY+7.0f*scale+padY)return index;
        return -1;
    };
    int hit=rowAt("Again?",0,cy,2.6f);
    if(hit>=0)return hit;
    return rowAt("Quit",1,cy+58.0f,1.55f);
}

void clearMenuHistory(LocalSettingsState& settings){settings.menuHistoryDepth=0;settings.menuScroll=0.0f;}
void setMenuPageDirect(HostState& host,LocalMenuPage page,int selection=0,float scroll=0.0f){GameState& state=host.game.networkMutableState();state.localSettings.menuPage=page;state.localSettings.menuScroll=std::max(0.0f,scroll);state.localSettings.rebindingAction=-1;state.localSettings.pendingBinding=-1;state.localSettings.conflictingAction=-1;state.hud.menuSelection=std::max(0,selection);state.cinematic.textInteraction=0.36f;}
void openMenuRoot(HostState& host,LocalMenuPage page=LocalMenuPage::Main){GameState& state=host.game.networkMutableState();clearMenuHistory(state.localSettings);setMenuPageDirect(host,page);}
void pushMenuPage(HostState& host,LocalMenuPage page){GameState& state=host.game.networkMutableState();auto& settings=state.localSettings;if(settings.menuHistoryDepth<LocalSettingsState::MenuHistoryCapacity){settings.menuHistory[settings.menuHistoryDepth++]={settings.menuPage,state.hud.menuSelection,settings.menuScroll};}else{for(int i=1;i<LocalSettingsState::MenuHistoryCapacity;++i)settings.menuHistory[i-1]=settings.menuHistory[i];settings.menuHistory[LocalSettingsState::MenuHistoryCapacity-1]={settings.menuPage,state.hud.menuSelection,settings.menuScroll};}setMenuPageDirect(host,page);}
bool popMenuPage(HostState& host){GameState& state=host.game.networkMutableState();auto& settings=state.localSettings;if(settings.menuHistoryDepth<=0)return false;const LocalMenuHistoryEntry entry=settings.menuHistory[--settings.menuHistoryDepth];setMenuPageDirect(host,entry.page,entry.selection,entry.scroll);return true;}

PhoneMenuElement selectedPhoneElement(const GameState& state){const PhoneMenuPageViewModel page=makePhoneMenuPageModel(state);const PhoneMenuElement* element=phoneMenuElementForSelection(page,state.hud.menuSelection);return element?*element:PhoneMenuElement{};}
PhoneMenuAction selectedPhoneAction(const GameState& state){return selectedPhoneElement(state).action;}

bool adjustMenuSetting(HostState& host,int direction){GameState& state=host.game.networkMutableState();auto& settings=state.localSettings;const PhoneMenuAction action=selectedPhoneElement(state).action;if(action==PhoneMenuAction::AdjustMouse){settings.mouseLookSensitivity=clampf(settings.mouseLookSensitivity+direction*0.10f,0.5f,1.75f);state.cinematic.textInteraction=0.65f;return true;}if(action==PhoneMenuAction::AdjustController){settings.controllerLookSensitivity=clampf(settings.controllerLookSensitivity+direction*0.10f,0.5f,1.75f);state.cinematic.textInteraction=0.65f;return true;}if(action==PhoneMenuAction::AdjustTriggers){settings.controllerTriggerSensitivity=std::max(0,std::min(2,settings.controllerTriggerSensitivity+direction));state.cinematic.textInteraction=0.65f;return true;}if(action==PhoneMenuAction::AdjustVibration){settings.controllerVibration=std::max(0,std::min(2,settings.controllerVibration+direction));state.cinematic.textInteraction=0.65f;return true;}if(action==PhoneMenuAction::MusicVolume){settings.musicVolume=clampf(settings.musicVolume+direction*0.10f,0,1);state.cinematic.textInteraction=0.65f;return true;}if(action==PhoneMenuAction::SfxVolume){settings.sfxVolume=clampf(settings.sfxVolume+direction*0.10f,0,1);state.cinematic.textInteraction=0.65f;return true;}if(action==PhoneMenuAction::GraphicsPreset){applyPhoneGraphicsPreset(settings,(settings.graphicsPreset+direction+3)%3);state.cinematic.textInteraction=0.65f;return true;}return false;}
bool toggleMenuSetting(HostState& host){GameState& state=host.game.networkMutableState();auto& settings=state.localSettings;switch(selectedPhoneAction(state)){case PhoneMenuAction::MusicMute:settings.musicMuted=!settings.musicMuted;break;case PhoneMenuAction::SfxMute:settings.sfxMuted=!settings.sfxMuted;break;case PhoneMenuAction::ToggleShadows:settings.shadows=!settings.shadows;break;case PhoneMenuAction::ToggleParticles:settings.particles=!settings.particles;break;case PhoneMenuAction::ToggleFps:settings.fpsCounter=!settings.fpsCounter;break;default:return false;}state.cinematic.textInteraction=0.65f;return true;}

void setMenuSelection(HostState& host,int selection) {
    const int count=menuItemCount(host.game.state());
    if(count<=0)return;

    GameState& state=host.game.networkMutableState();

    // The two-choice death menu must not wrap. A tiny repeated stick,
    // key, or mouse movement should stop at Again?/Quit rather than
    // cycling through both choices.
    const int next=state.dead
        ? std::max(0,std::min(count-1,selection))
        : (selection%count+count)%count;

    // Ignore repeated hover and held-input reports for the same row.
    if(state.hud.menuSelection==next){
        if(state.dead)state.cinematic.deathChoice=next;
        return;
    }

    state.hud.menuSelection=next;

    if(state.dead){
        state.cinematic.deathChoice=next;
        state.cinematic.textInteraction=
            std::max(state.cinematic.textInteraction,0.22f);
        host.audio.playMenuCue(false);
        return;
    }

    const PhoneDisplayMenuLayout layout=
        makePhoneDisplayMenuLayout(state);
    state.localSettings.menuScroll=
        phoneDisplayScrollForSelection(layout,next);
    state.cinematic.textInteraction=
        std::max(state.cinematic.textInteraction,0.22f);
    host.audio.playMenuCue(false);
}

int menuItemAt(GLFWwindow* window,const HostState& host,double windowX,double windowY) {
    int ww=1,wh=1,fw=1,fh=1;glfwGetWindowSize(window,&ww,&wh);glfwGetFramebufferSize(window,&fw,&fh);
    // Match DesktopRenderer's Retina logical-canvas scale followed by its
    // bounded menu scale (2.5 * 1.8 at the extreme), so visual and clickable
    // button rectangles stay identical on high-density displays.
    const float uiScale=clampf(std::min(static_cast<float>(fw)/1280.0f,static_cast<float>(fh)/720.0f),0.55f,4.5f);
    const float canvasW=fw/uiScale,canvasH=fh/uiScale;
    const float framebufferX=static_cast<float>(windowX)*fw/std::max(1,ww),framebufferY=static_cast<float>(windowY)*fh/std::max(1,wh);
    const float x=framebufferX/uiScale,y=framebufferY/uiScale;
    const GameState& state=host.game.state();
    if(state.upgradeMenu.active){
        const float pw=std::min(680.0f,canvasW-24.0f),ph=300.0f,px=(canvasW-pw)*0.5f,py=(canvasH-ph)*0.5f;
        if(y>=py+66&&y<=py+142)for(int column=0;column<3;++column){const float left=px+12+column*(pw-24)/3;if(x>=left&&x<left+(pw-24)/3)return column;}
        if(y>=py+184&&y<=py+250)for(int column=0;column<3;++column){const float left=px+12+column*(pw-24)/3;if(x>=left&&x<left+(pw-24)/3)return 3+column;}
        return -1;
    }
    const int deathItem=deathMenuItemAt(state,x,y,canvasW,canvasH);
    if(deathItem>=0)return deathItem;
    const int phoneItem=phoneMenuItemAt(state,framebufferX,framebufferY,fw,fh);
    if(phoneItem>=0)return phoneItem;
    if(!state.started||soloPauseMenu(state))return -1;
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
    if(state.dead){
        if(selection==0){host.game.restart();setMouseCaptured(window,host,true);}
        else {host.game.prepareStartScreen();setMouseCaptured(window,host,false);}
        return;
    }
    if(soloPauseMenu(state)){
        auto& settings=state.localSettings;
        const PhoneMenuElement row=selectedPhoneElement(state);
        if(row.action==PhoneMenuAction::Resume){setMouseCaptured(window,host,true);}
        else if(row.action==PhoneMenuAction::Controls)pushMenuPage(host,LocalMenuPage::Controls);
        else if(row.action==PhoneMenuAction::Audio)pushMenuPage(host,LocalMenuPage::Audio);
        else if(row.action==PhoneMenuAction::Graphics)pushMenuPage(host,LocalMenuPage::Graphics);
        else if(row.action==PhoneMenuAction::ExitRun){host.multiplayer.disconnect();host.game.prepareStartScreen();setMouseCaptured(window,host,false);openMenuRoot(host);}
        else if(row.action==PhoneMenuAction::Back){if(!popMenuPage(host))setMouseCaptured(window,host,true);}
        else if(row.action==PhoneMenuAction::Rebind&&row.bindingAction>=0){settings.rebindingAction=row.bindingAction;settings.pendingBinding=-1;settings.conflictingAction=-1;}
        else if(row.action==PhoneMenuAction::Defaults){settings.keyboardBindings={{87,83,65,68,340,32,67,81,86,70}};settings.mouseLookSensitivity=1.0f;settings.controllerLookSensitivity=1.15f;settings.controllerTriggerSensitivity=1;settings.controllerVibration=1;}
        else if(row.action==PhoneMenuAction::CheckUpdates)host.updater.checkForUpdates(desktopBuildIdentity());
        else if(!adjustMenuSetting(host,1))toggleMenuSetting(host);
        return;
    }
    if(!state.started){
        auto& settings=state.localSettings;
        const PhoneMenuElement row=selectedPhoneElement(state);
        const PhoneMenuAction action=selectedPhoneAction(state);
        if(action==PhoneMenuAction::Solo){host.multiplayer.disconnect();host.game.restart();setMouseCaptured(window,host,true);}
        else if(action==PhoneMenuAction::Online)pushMenuPage(host,LocalMenuPage::Online);
        else if(action==PhoneMenuAction::Settings)pushMenuPage(host,LocalMenuPage::Settings);
        else if(action==PhoneMenuAction::Exit)glfwSetWindowShouldClose(window,GLFW_TRUE);
        else if(action==PhoneMenuAction::Start&&host.multiplayer.role()==DesktopMultiplayer::Role::Host)host.multiplayer.startMatch();
        else if(action==PhoneMenuAction::Host){host.multiplayer.host(host.multiplayerService);host.game.setNetworkRoom("","CREATING",false);}
        else if(action==PhoneMenuAction::Join){host.enteringJoinCode=true;host.joinCode.clear();pushMenuPage(host,LocalMenuPage::JoinCode);host.game.setNetworkRoom("","ENTER CODE",false);}
        else if(action==PhoneMenuAction::Controls)pushMenuPage(host,LocalMenuPage::Controls);
        else if(action==PhoneMenuAction::Audio)pushMenuPage(host,LocalMenuPage::Audio);
        else if(action==PhoneMenuAction::Graphics)pushMenuPage(host,LocalMenuPage::Graphics);
        else if(action==PhoneMenuAction::CheckUpdates)host.updater.checkForUpdates(desktopBuildIdentity());
        else if(action==PhoneMenuAction::Back){if(host.multiplayer.role()!=DesktopMultiplayer::Role::Offline)host.multiplayer.disconnect();popMenuPage(host);}
        else if(row.action==PhoneMenuAction::Rebind&&row.bindingAction>=0){settings.rebindingAction=row.bindingAction;settings.pendingBinding=-1;settings.conflictingAction=-1;}
        else if(row.action==PhoneMenuAction::Defaults){settings.keyboardBindings={{87,83,65,68,340,32,67,81,86,70}};settings.mouseLookSensitivity=1.0f;settings.controllerLookSensitivity=1.15f;settings.controllerTriggerSensitivity=1;settings.controllerVibration=1;}
        else if(!adjustMenuSetting(host,1))toggleMenuSetting(host);
        return;
    }
    if(state.uiPaused)setMouseCaptured(window,host,true);
}

void controllerMenuBack(GLFWwindow* window,HostState& host){
    const GameState& state=host.game.state();
    if(state.dead){GameState& mutableState=host.game.networkMutableState();mutableState.hud.menuSelection=1;mutableState.cinematic.deathChoice=1;return;}
    if(soloPauseMenu(state)){
        if(state.localSettings.menuPage!=LocalMenuPage::Main){if(popMenuPage(host))return;}
        setMouseCaptured(window,host,true);return;
    }
    if(!state.started&&state.localSettings.menuPage!=LocalMenuPage::Main){
        if(host.multiplayer.pending())host.multiplayer.disconnect();
        if(host.enteringJoinCode){host.enteringJoinCode=false;host.joinCode.clear();}
        popMenuPage(host);
        return;
    }
    if(state.uiPaused){setMouseCaptured(window,host,true);return;}
}

DesktopGamepadInput pollGamepad(GLFWwindow* window,HostState& host){
    DesktopGamepadInput input;
    controllerRumbleUpdate();
    int jid=host.gamepadId;
    bool mapped=jid>=GLFW_JOYSTICK_1&&jid<=GLFW_JOYSTICK_LAST&&glfwJoystickPresent(jid)&&glfwJoystickIsGamepad(jid)&&!preferRawXboxLayout(jid);
    bool present=jid>=GLFW_JOYSTICK_1&&jid<=GLFW_JOYSTICK_LAST&&glfwJoystickPresent(jid);
    if(!present){
        jid=-1;
        for(int candidate=GLFW_JOYSTICK_1;candidate<=GLFW_JOYSTICK_LAST;++candidate)if(glfwJoystickIsGamepad(candidate)&&!preferRawXboxLayout(candidate)){jid=candidate;break;}
        if(jid<0)for(int candidate=GLFW_JOYSTICK_1;candidate<=GLFW_JOYSTICK_LAST;++candidate)if(glfwJoystickPresent(candidate)){jid=candidate;break;}
        mapped=jid>=0&&glfwJoystickIsGamepad(jid)&&!preferRawXboxLayout(jid);
        if(jid!=host.gamepadId||mapped!=host.gamepadMapped){
            resetGamepadHistory(host);
            host.gamepadId=jid;
            host.gamepadMapped=mapped;

            if(jid>=0){
                const char* controllerName=mapped
                    ? glfwGetGamepadName(jid)
                    : glfwGetJoystickName(jid);

                std::printf(
                    "Controller connected: %s%s\n",
                    controllerName&&*controllerName
                        ? controllerName
                        : "Unknown controller",
                    mapped ? "" : " (raw joystick fallback)"
                );
            }

            // A Bluetooth device can be reported as present before Windows and
            // GLFW have finished exposing its complete state. Do not query axes
            // or buttons until the following frame.
            return input;
        }
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
    const auto triggerThresholds=dbmenu::controllerTriggerThresholds(host.game.state().localSettings.controllerTriggerSensitivity);
    const bool leftTriggerDown=triggerHeld(pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER],triggerThresholds.left);
    const bool rightTriggerDown=triggerHeld(pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER],triggerThresholds.right);
    const bool leftTriggerPressed=leftTriggerDown&&!host.previousGamepadLeftTrigger;
    const bool rightTriggerPressed=rightTriggerDown&&!host.previousGamepadRightTrigger;
    if(menuActive&&!host.enteringJoinCode){
        if((menuUp&&!host.previousGamepadMenuUp)||(menuDown&&!host.previousGamepadMenuDown)){
            const int current=host.game.state().hud.menuSelection;
            const int next=host.game.state().upgradeMenu.active
                ?dbmenu::moveUpgradeGridSelection(current,0,menuDown?1:-1)
                :current+(menuDown?1:-1);
            setMenuSelection(host,next);
            rumblePulse(host.game.state().localSettings,0.03f,0.14f,18);
        }else if((menuLeft&&!host.previousGamepadMenuLeft)||(menuRight&&!host.previousGamepadMenuRight)){
            const int direction=menuRight?1:-1;
            if(host.game.state().upgradeMenu.active)
                setMenuSelection(host,dbmenu::moveUpgradeGridSelection(host.game.state().hud.menuSelection,direction,0));
            else if(!adjustMenuSetting(host,direction)&&menuRight)
                toggleMenuSetting(host);
            rumblePulse(host.game.state().localSettings,0.05f,0.20f,22);
        }
        if(pressed(GLFW_GAMEPAD_BUTTON_A)){rumblePulse(host.game.state().localSettings,0.10f,0.24f,32);activateMenuSelection(window,host);}
        if(pressed(GLFW_GAMEPAD_BUTTON_B)){rumblePulse(host.game.state().localSettings,0.12f,0.05f,28);controllerMenuBack(window,host);}
        if(pressed(GLFW_GAMEPAD_BUTTON_START)&&host.game.state().uiPaused){rumblePulse(host.game.state().localSettings,0.12f,0.05f,28);controllerMenuBack(window,host);}
    }else if(host.enteringJoinCode){
        if(pressed(GLFW_GAMEPAD_BUTTON_B)){rumblePulse(host.game.state().localSettings,0.12f,0.05f,28);controllerMenuBack(window,host);}
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
        if(input.shootPressed)rumblePulse(host.game.state().localSettings,0.08f,0.32f,36);
        else if(rightTriggerPressed)rumblePulse(host.game.state().localSettings,0.04f,0.16f,20);
        else if(input.jumpPressed)rumblePulse(host.game.state().localSettings,0.16f,0.04f,28);
    }
    host.previousGamepadMenuLeft=menuLeft;host.previousGamepadMenuRight=menuRight;host.previousGamepadMenuUp=menuUp;host.previousGamepadMenuDown=menuDown;
    host.previousGamepadLeftTrigger=leftTriggerDown;
    host.previousGamepadRightTrigger=rightTriggerDown;
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
    // Attract mode deliberately leaves the cursor available for the one-click
    // menu handoff while its autonomous gameplay continues behind the title.
    // Ordinary solo play still couples cursor release to pause ownership.
    if(!host.game.state().attractMode)host.game.setUiPaused(!captured);
    if(host.game.state().multiplayer.enabled&&wasPaused==captured){
        std::printf("MULTIPLAYER_MENU_%s player=%d\n",captured?"CLOSED":"OPENED",host.game.state().multiplayer.localPlayerId);
        std::fflush(stdout);
    }
    if(!captured&&host.game.state().started&&!host.game.state().multiplayer.enabled&&!wasPaused)openMenuRoot(host,LocalMenuPage::Main);
    if (captured) {
        glfwGetCursorPos(window, &host.lastMouseX, &host.lastMouseY);
        host.haveMouse = true;
        if(host.game.state().multiplayer.enabled){
            std::printf("MULTIPLAYER_MOUSE_CAPTURE_RESTORED player=%d\n",host.game.state().multiplayer.localPlayerId);
            std::fflush(stdout);
        }
    } else if(host.game.state().multiplayer.enabled) {
        host.game.clearInputState();
    }
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    HostState* host = stateFor(window);
    if (!host) return;
    if(action==GLFW_PRESS&&host->game.state().attractMode){host->game.dismissAttractMode();setMouseCaptured(window,*host,false);return;}

    if(action==GLFW_PRESS&&host->game.state().localSettings.rebindingAction>=0){auto& settings=host->game.networkMutableState().localSettings;if(key==GLFW_KEY_ESCAPE){settings.rebindingAction=-1;settings.pendingBinding=-1;settings.conflictingAction=-1;return;}const int actionIndex=settings.rebindingAction;int conflict=-1;for(int i=0;i<10;++i)if(i!=actionIndex&&settings.keyboardBindings[i]==key){conflict=i;break;}const int old=settings.keyboardBindings[actionIndex];settings.keyboardBindings[actionIndex]=key;if(conflict>=0)settings.keyboardBindings[conflict]=old;settings.rebindingAction=settings.pendingBinding=settings.conflictingAction=-1;host->audio.playMenuCue(true);return;}

    const bool menuActive=menuItemCount(host->game.state())>0;
    if(action==GLFW_PRESS&&menuActive&&!host->enteringJoinCode){
        // Menus release the cursor, so Escape has the same second-press exit
        // meaning it has after releasing the cursor during ordinary play.
        if(key==GLFW_KEY_ESCAPE){if(soloPauseMenu(host->game.state())||multiplayerPauseMenu(host->game.state())){controllerMenuBack(window,*host);return;}if(!host->game.state().started&&host->game.state().localSettings.menuPage!=LocalMenuPage::Main){controllerMenuBack(window,*host);return;}glfwSetWindowShouldClose(window,GLFW_TRUE);return;}
        const bool left=key==GLFW_KEY_LEFT||key==GLFW_KEY_A, right=key==GLFW_KEY_RIGHT||key==GLFW_KEY_D;
        const bool up=key==GLFW_KEY_UP||key==GLFW_KEY_W, down=key==GLFW_KEY_DOWN||key==GLFW_KEY_S;
        if(host->game.state().upgradeMenu.active&&(up||down)){setMenuSelection(*host,host->game.state().hud.menuSelection+(down?3:-3));return;}
        if((left||right)){if(!adjustMenuSetting(*host,right?1:-1)&&right)toggleMenuSetting(*host);return;}
        if(up){setMenuSelection(*host,host->game.state().hud.menuSelection-1);return;}
        if(down){setMenuSelection(*host,host->game.state().hud.menuSelection+1);return;}
        if(key==GLFW_KEY_ENTER||key==GLFW_KEY_SPACE||key==GLFW_KEY_F){activateMenuSelection(window,*host);return;}
        if(host->game.state().upgradeMenu.active&&key>=GLFW_KEY_1&&key<=GLFW_KEY_6){setMenuSelection(*host,key-GLFW_KEY_1);activateMenuSelection(window,*host);}
        return;
    }

    if(action==GLFW_PRESS&&!host->game.state().started&&host->enteringJoinCode){
        if(key==GLFW_KEY_ESCAPE){if(host->multiplayer.pending())host->multiplayer.disconnect();host->enteringJoinCode=false;host->joinCode.clear();popMenuPage(*host);return;}
        if(key==GLFW_KEY_BACKSPACE){if(!host->joinCode.empty())host->joinCode.pop_back();return;}
        if(key==GLFW_KEY_ENTER&&host->joinCode.size()==6){host->multiplayer.join(host->multiplayerService,host->joinCode);host->enteringJoinCode=false;popMenuPage(*host);return;}
        if(host->joinCode.size()<6&&((key>=GLFW_KEY_A&&key<=GLFW_KEY_Z)||(key>=GLFW_KEY_2&&key<=GLFW_KEY_9))){const char value=static_cast<char>(key);if(dbmultiplayer::isRoomCharacter(value))host->joinCode.push_back(value);host->game.setNetworkRoom(host->joinCode.c_str(),host->joinCode.size()==6?"PRESS ENTER":"ENTER CODE",false);return;}
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
        } else if (soloPauseMenu(host->game.state())||multiplayerPauseMenu(host->game.state())) {
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
    if(!host->mouseCaptured){const int hovered=menuItemAt(window,*host,x,y);if(hovered>=0){const int before=host->game.state().hud.menuSelection;setMenuSelection(*host,hovered);if(before!=host->game.state().hud.menuSelection)touchpadHapticPulse(TouchpadHapticNavigate,host->game.state().localSettings.controllerVibration);}return;}

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

void scrollCallback(GLFWwindow* window,double,double yOffset){
    HostState* host=stateFor(window);
    if(!host||host->mouseCaptured||host->enteringJoinCode||yOffset==0.0)return;
    const int count=menuItemCount(host->game.state());
    if(count<=0)return;
    const int direction=yOffset<0.0?1:-1;
    setMenuSelection(*host,dbmenu::wheelSelection(host->game.state().hud.menuSelection,count,direction));
}

void framebufferCallback(GLFWwindow* window, int width, int height) {
    HostState* host = stateFor(window);
    if (host) host->renderer.resize(width, height);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    HostState* host = stateFor(window);
    if (!host || action != GLFW_PRESS) return;
    if(host->game.state().attractMode){host->game.dismissAttractMode();setMouseCaptured(window,*host,false);return;}
    if(menuItemCount(host->game.state())>0){
        const dbmenu::PointerAction pointerAction=dbmenu::pointerAction(button,GLFW_MOUSE_BUTTON_LEFT,GLFW_MOUSE_BUTTON_RIGHT);
        if(pointerAction==dbmenu::PointerAction::Back){
            touchpadHapticPulse(TouchpadHapticNavigate,host->game.state().localSettings.controllerVibration);
            controllerMenuBack(window,*host);
            return;
        }
        double x=0,y=0;glfwGetCursorPos(window,&x,&y);const int hovered=menuItemAt(window,*host,x,y);
        if(hovered>=0)setMenuSelection(*host,hovered);
        if(pointerAction==dbmenu::PointerAction::Activate&&hovered>=0){
            touchpadHapticPulse(TouchpadHapticConfirm,host->game.state().localSettings.controllerVibration);
            activateMenuSelection(window,*host);
            if(button==GLFW_MOUSE_BUTTON_LEFT&&host->mouseCaptured)host->suppressLeftMouseUntilRelease=true;
        }
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

struct RuntimePerfTrace {
    struct Stats { double average=0.0,p95=0.0,maximum=0.0; };
    std::ofstream output;
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point windowStarted=started;
    std::vector<double> totalMs,updateMs,audioMs,renderMs,swapMs;
    int maximumSimulationSteps=0;
    int droppedAccumulatorFrames=0;

    explicit RuntimePerfTrace(const char* path) {
        if(!path||!*path)return;
        output.open(path,std::ios::trunc);
        if(output)output<<"seconds,game_frame,room,samples,total_avg,total_p95,total_max,update_avg,update_p95,update_max,audio_avg,audio_p95,audio_max,render_avg,render_p95,render_max,swap_avg,swap_p95,swap_max,sim_steps_max,dropped_accumulator_frames,active_humans,exposed_souls,active_particles,active_fragments,pending_respawns,stored_souls\n";
    }
    bool active() const{return output.is_open();}
    static Stats stats(std::vector<double> values){
        Stats result;if(values.empty())return result;
        result.average=std::accumulate(values.begin(),values.end(),0.0)/static_cast<double>(values.size());
        std::sort(values.begin(),values.end());
        result.p95=values[static_cast<std::size_t>(std::round(0.95*static_cast<double>(values.size()-1)))];
        result.maximum=values.back();return result;
    }
    void sample(const GameState& state,double total,double update,double audio,double render,double swap,int simulationSteps,bool dropped){
        if(!active())return;
        totalMs.push_back(total);updateMs.push_back(update);audioMs.push_back(audio);renderMs.push_back(render);swapMs.push_back(swap);
        maximumSimulationSteps=std::max(maximumSimulationSteps,simulationSteps);
        if(dropped)++droppedAccumulatorFrames;
        const auto now=std::chrono::steady_clock::now();
        if(std::chrono::duration<double>(now-windowStarted).count()<1.0)return;
        int humans=0,souls=0,particles=0,fragments=0,respawns=0;
        for(const auto& target:state.targets){if(gameplay::isActiveHuman(target))++humans;if(target.alive&&target.slurpable&&target.soulCubeAmount>0.001f)++souls;}
        for(const auto& particle:state.particles)if(particle.life>0.0f){++particles;if(particle.kind==1)++fragments;}
        for(const auto& request:state.respawnQueue)if(request.active)++respawns;
        const Stats totalStats=stats(totalMs),updateStats=stats(updateMs),audioStats=stats(audioMs),renderStats=stats(renderMs),swapStats=stats(swapMs);
        output<<std::fixed<<std::setprecision(3)<<std::chrono::duration<double>(now-started).count()<<','<<state.frame<<','<<state.roomIndex<<','<<totalMs.size()<<','
            <<totalStats.average<<','<<totalStats.p95<<','<<totalStats.maximum<<','<<updateStats.average<<','<<updateStats.p95<<','<<updateStats.maximum<<','
            <<audioStats.average<<','<<audioStats.p95<<','<<audioStats.maximum<<','
            <<renderStats.average<<','<<renderStats.p95<<','<<renderStats.maximum<<','<<swapStats.average<<','<<swapStats.p95<<','<<swapStats.maximum<<','
            <<maximumSimulationSteps<<','<<droppedAccumulatorFrames<<','<<humans<<','<<souls<<','<<particles<<','<<fragments<<','<<respawns<<','<<state.player.souls<<'\n';
        output.flush();totalMs.clear();updateMs.clear();audioMs.clear();renderMs.clear();swapMs.clear();maximumSimulationSteps=0;droppedAccumulatorFrames=0;windowStarted=now;
    }
};

int activeHumanCount(const GameState& state){int count=0;for(const auto& target:state.targets)if(gameplay::isActiveHuman(target))++count;return count;}

int runCombatRenderStress(GLFWwindow* window,HostState& host){
    constexpr int cycles=10;
    constexpr int captureFrameLimit=360;
    constexpr int respawnFrameLimit=300;
    constexpr int settledRespawnFrames=60;
    host.game.reset();
    GameState& initial=host.game.networkMutableState();
    for(auto& target:initial.targets)target=TargetState{};
    for(auto& request:initial.respawnQueue)request=HumanRespawnRequest{};
    initial.cinematic=CinematicState{};
    initial.localSettings.particles=true;
    initial.localSettings.shadows=true;
    initial.camera.yaw=0.0f;initial.camera.pitch=0.0f;initial.camera.forward={0,0,-1};
    std::array<double,cycles> captureAverage{},respawnAverage{};
    std::unique_ptr<GameState> earlySettledState;
    int peakParticles=0,peakFragments=0,peakSouls=0;

    const auto step=[&](bool vacuum,bool melee,std::vector<double>& samples){
        host.game.setTouchControls(0,0,0,0,vacuum,false,false,melee,false,false);
        host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));
        const auto begin=std::chrono::steady_clock::now();
        host.renderer.draw(host.game.state());glFinish();
        const auto end=std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double,std::milli>(end-begin).count());
        glfwSwapBuffers(window);glfwPollEvents();
        int particles=0,fragments=0,souls=0;
        for(const auto& particle:host.game.state().particles)if(particle.life>0){++particles;if(particle.kind==1)++fragments;}
        for(const auto& target:host.game.state().targets)if(target.alive&&target.slurpable&&target.soulCubeAmount>0.001f)++souls;
        peakParticles=std::max(peakParticles,particles);peakFragments=std::max(peakFragments,fragments);peakSouls=std::max(peakSouls,souls);
    };

    for(int cycle=0;cycle<cycles;++cycle){
        GameState& state=host.game.networkMutableState();
        TargetState& target=state.targets[0];
        target=TargetState{};target.alive=true;target.armor=0.10f;target.health=1.0f;target.attackCooldown=999.0f;
        target.pos=state.player.pos+Vec3{0,0,-0.75f};target.walkTarget=target.pos;
        state.player.battery=100.0f;state.player.vel={};state.player.jumpVel=0;state.player.grounded=true;state.player.grabbedByTarget=-1;
        const int soulsBefore=state.player.souls;
        std::vector<double> captureSamples,respawnSamples;
        step(false,true,captureSamples);step(false,false,captureSamples);
        if(!host.game.state().targets[0].slurpable){std::fprintf(stderr,"COMBAT_RENDER_STRESS_FAIL cycle=%d phase=melee\n",cycle+1);return 1;}
        int captureFrames=0;
        while(host.game.state().player.souls==soulsBefore&&captureFrames++<captureFrameLimit)step(true,false,captureSamples);
        if(host.game.state().player.souls!=soulsBefore+1){std::fprintf(stderr,"COMBAT_RENDER_STRESS_FAIL cycle=%d phase=capture frames=%d\n",cycle+1,captureFrames);return 1;}
        std::vector<double> respawnWaitSamples;
        int respawnWaitFrames=0;
        while(activeHumanCount(host.game.state())==0&&respawnWaitFrames++<respawnFrameLimit)step(false,false,respawnWaitSamples);
        if(activeHumanCount(host.game.state())!=1){std::fprintf(stderr,"COMBAT_RENDER_STRESS_FAIL cycle=%d phase=respawn frames=%d\n",cycle+1,respawnWaitFrames);return 1;}
        for(auto& respawned:host.game.networkMutableState().targets)if(gameplay::isActiveHuman(respawned)){
            respawned=TargetState{};respawned.alive=true;respawned.attackCooldown=999.0f;
            respawned.pos=host.game.state().player.pos+Vec3{0,0,-1.5f};respawned.walkTarget=respawned.pos;
        }
        for(int frame=0;frame<settledRespawnFrames;++frame)step(false,false,respawnSamples);
        if(cycle==0)earlySettledState=std::make_unique<GameState>(host.game.state());
        captureAverage[cycle]=RuntimePerfTrace::stats(captureSamples).average;
        respawnAverage[cycle]=RuntimePerfTrace::stats(respawnSamples).average;
        std::printf("COMBAT_RENDER_CYCLE cycle=%d capture_frames=%zu capture_avg=%.3f respawn_wait=%d settled_frames=%zu settled_avg=%.3f stored_souls=%d\n",cycle+1,captureSamples.size(),captureAverage[cycle],respawnWaitFrames,respawnSamples.size(),respawnAverage[cycle],host.game.state().player.souls);
    }
    const double captureEarly=std::accumulate(captureAverage.begin(),captureAverage.begin()+3,0.0)/3.0;
    const double captureLate=std::accumulate(captureAverage.end()-3,captureAverage.end(),0.0)/3.0;
    const double respawnEarly=std::accumulate(respawnAverage.begin(),respawnAverage.begin()+3,0.0)/3.0;
    const double respawnLate=std::accumulate(respawnAverage.end()-3,respawnAverage.end(),0.0)/3.0;
    std::printf("COMBAT_RENDER_STRESS_OK cycles=%d capture_early=%.3f capture_late=%.3f capture_ratio=%.3f settled_early=%.3f settled_late=%.3f settled_ratio=%.3f peak_particles=%d peak_fragments=%d peak_souls=%d\n",cycles,captureEarly,captureLate,captureEarly>0?captureLate/captureEarly:0,respawnEarly,respawnLate,respawnEarly>0?respawnLate/respawnEarly:0,peakParticles,peakFragments,peakSouls);
    constexpr std::array<int,3> displayedSoulCounts{{0,5,10}};
    std::array<std::vector<double>,displayedSoulCounts.size()> inventorySamples;
    const GameState stableState=host.game.state();
    for(int frame=0;frame<150;++frame)for(std::size_t order=0;order<displayedSoulCounts.size();++order){
        const std::size_t index=(static_cast<std::size_t>(frame)+order)%displayedSoulCounts.size();
        GameState renderState=stableState;renderState.player.souls=displayedSoulCounts[index];renderState.hud.storedSouls=displayedSoulCounts[index];
        const auto begin=std::chrono::steady_clock::now();host.renderer.draw(renderState);glFinish();const auto end=std::chrono::steady_clock::now();
        if(frame>=30)inventorySamples[index].push_back(std::chrono::duration<double,std::milli>(end-begin).count());
        glfwSwapBuffers(window);glfwPollEvents();
    }
    for(std::size_t index=0;index<displayedSoulCounts.size();++index){const auto stats=RuntimePerfTrace::stats(inventorySamples[index]);std::printf("COMBAT_RENDER_INVENTORY souls=%d render_avg=%.3f render_p95=%.3f render_max=%.3f\n",displayedSoulCounts[index],stats.average,stats.p95,stats.maximum);}
    GameState lateSettledState=host.game.state();
    earlySettledState->player.souls=lateSettledState.player.souls=0;
    earlySettledState->hud.storedSouls=lateSettledState.hud.storedSouls=0;
    std::array<std::vector<double>,2> settledSnapshotSamples;
    for(int frame=0;frame<180;++frame)for(int order=0;order<2;++order){
        const int index=(frame+order)%2;const GameState& renderState=index==0?*earlySettledState:lateSettledState;
        const auto begin=std::chrono::steady_clock::now();host.renderer.draw(renderState);glFinish();const auto end=std::chrono::steady_clock::now();
        if(frame>=30)settledSnapshotSamples[index].push_back(std::chrono::duration<double,std::milli>(end-begin).count());
        glfwSwapBuffers(window);glfwPollEvents();
    }
    const auto earlySnapshotStats=RuntimePerfTrace::stats(settledSnapshotSamples[0]);
    const auto lateSnapshotStats=RuntimePerfTrace::stats(settledSnapshotSamples[1]);
    std::printf("COMBAT_RENDER_SNAPSHOT_COMPARE early_avg=%.3f late_avg=%.3f late_ratio=%.3f\n",earlySnapshotStats.average,lateSnapshotStats.average,earlySnapshotStats.average>0?lateSnapshotStats.average/earlySnapshotStats.average:0);
    return 0;
}

int runCombatCrowdStress(GLFWwindow* window,HostState& host){
    constexpr int waves=5,crowd=6;
    host.game.reset();GameState& initial=host.game.networkMutableState();
    for(auto& target:initial.targets)target=TargetState{};
    for(auto& request:initial.respawnQueue)request=HumanRespawnRequest{};
    initial.cinematic=CinematicState{};initial.localSettings.particles=true;initial.localSettings.shadows=true;initial.localSettings.musicMuted=true;initial.localSettings.sfxMuted=true;initial.runRules.crowdedRoomStacks=1;
    initial.player.pos={0,0.08f,0};initial.camera.yaw=0;initial.camera.pitch=0;initial.camera.forward={0,0,-1};
    std::array<double,waves> waveAverage{};std::unique_ptr<GameState> earlyState,peakLoadState;
    int peakParticles=0,peakFragments=0,peakSouls=0;std::vector<double> audioSamples;
    host.audio.update(host.game.state());
    const auto step=[&](bool vacuum,bool melee,float look,std::vector<double>& samples){
        host.game.setTouchControls(0,0,look,0,vacuum,false,false,melee,false,false);
        host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));
        const auto audioBegin=std::chrono::steady_clock::now();host.audio.update(host.game.state());const auto audioEnd=std::chrono::steady_clock::now();audioSamples.push_back(std::chrono::duration<double,std::milli>(audioEnd-audioBegin).count());
        const auto begin=std::chrono::steady_clock::now();host.renderer.draw(host.game.state());glFinish();const auto end=std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double,std::milli>(end-begin).count());glfwSwapBuffers(window);glfwPollEvents();
        int particles=0,fragments=0,souls=0;for(const auto& particle:host.game.state().particles)if(particle.life>0){++particles;if(particle.kind==1)++fragments;}for(const auto& target:host.game.state().targets)if(target.alive&&target.slurpable&&target.soulCubeAmount>0.001f)++souls;
        if(particles>peakParticles)peakLoadState=std::make_unique<GameState>(host.game.state());peakParticles=std::max(peakParticles,particles);peakFragments=std::max(peakFragments,fragments);peakSouls=std::max(peakSouls,souls);
    };
    for(int wave=0;wave<waves;++wave){
        GameState& state=host.game.networkMutableState();state.player.battery=100;state.player.pos={0,0.08f,0};state.player.vel={};state.player.jumpVel=0;state.player.grounded=true;state.player.grabbedByTarget=-1;state.camera.yaw=0;state.camera.pitch=0;
        int prepared=0;for(auto& target:state.targets)if(gameplay::isActiveHuman(target)&&prepared<crowd){target.health=1;target.armor=0.10f;target.attackCooldown=999;target.pos={-0.65f+0.26f*prepared,0.08f,-0.78f-0.06f*(prepared%2)};target.walkTarget=target.pos;target.vel={};++prepared;}
        for(int i=prepared;i<crowd;++i){TargetState& target=state.targets[i];target=TargetState{};target.alive=true;target.health=1;target.armor=0.10f;target.attackCooldown=999;target.pos={-0.65f+0.26f*i,0.08f,-0.78f-0.06f*(i%2)};target.walkTarget=target.pos;}prepared=crowd;
        const int soulsBefore=state.player.souls;std::vector<double> samples;
        int attackFrames=0;while(activeHumanCount(host.game.state())>0&&attackFrames<240){const bool attack=attackFrames%32==0;step(false,attack,0,samples);++attackFrames;GameState& mutableState=host.game.networkMutableState();int index=0;for(auto& target:mutableState.targets)if(gameplay::isActiveHuman(target)){target.pos={-0.65f+0.26f*(index%crowd),0.08f,-0.78f-0.06f*(index%2)};target.walkTarget=target.pos;target.attackCooldown=999;++index;}}
        if(activeHumanCount(host.game.state())>0){std::fprintf(stderr,"COMBAT_CROWD_STRESS_FAIL wave=%d phase=exposure humans=%d\n",wave+1,activeHumanCount(host.game.state()));return 1;}
        int captureFrames=0;while(host.game.state().player.souls<soulsBefore+crowd&&captureFrames++<900)step(true,false,0,samples);
        if(host.game.state().player.souls!=soulsBefore+crowd){std::fprintf(stderr,"COMBAT_CROWD_STRESS_FAIL wave=%d phase=capture gained=%d frames=%d\n",wave+1,host.game.state().player.souls-soulsBefore,captureFrames);return 1;}
        int respawnFrames=0;while(activeHumanCount(host.game.state())<crowd&&respawnFrames++<900)step(false,false,0.55f,samples);
        if(activeHumanCount(host.game.state())<crowd){std::fprintf(stderr,"COMBAT_CROWD_STRESS_FAIL wave=%d phase=respawn humans=%d frames=%d\n",wave+1,activeHumanCount(host.game.state()),respawnFrames);return 1;}
        for(int frame=0;frame<90;++frame)step(false,false,0.55f,samples);
        if(wave==0)earlyState=std::make_unique<GameState>(host.game.state());waveAverage[wave]=RuntimePerfTrace::stats(samples).average;
        std::printf("COMBAT_CROWD_WAVE wave=%d enemies=%d attack_frames=%d capture_frames=%d respawn_frames=%d render_avg=%.3f stored_souls=%d\n",wave+1,crowd,attackFrames,captureFrames,respawnFrames,waveAverage[wave],host.game.state().player.souls);
    }
    GameState lateState=host.game.state();earlyState->player.souls=lateState.player.souls=0;earlyState->hud.storedSouls=lateState.hud.storedSouls=0;std::array<std::vector<double>,2> snapshots;
    for(int frame=0;frame<180;++frame)for(int order=0;order<2;++order){const int index=(frame+order)%2;const GameState& renderState=index?lateState:*earlyState;const auto begin=std::chrono::steady_clock::now();host.renderer.draw(renderState);glFinish();const auto end=std::chrono::steady_clock::now();if(frame>=30)snapshots[index].push_back(std::chrono::duration<double,std::milli>(end-begin).count());glfwSwapBuffers(window);glfwPollEvents();}
    const auto early=RuntimePerfTrace::stats(snapshots[0]),late=RuntimePerfTrace::stats(snapshots[1]);const double waveEarly=(waveAverage[0]+waveAverage[1])/2,waveLate=(waveAverage[waves-2]+waveAverage[waves-1])/2;
    enum PeakRenderVariant { Legacy, Normal, Pretty, PrettyNoParticles, PrettyNoActors, PrettyNoCombatFx, PrettyNoCaptures, PeakRenderVariantCount };
    constexpr std::array<const char*,PeakRenderVariantCount> variantNames{{"legacy","normal","pretty","pretty_no_particles","pretty_no_actors","pretty_no_combat_fx","pretty_no_captures"}};
    std::array<std::vector<double>,PeakRenderVariantCount> presetSamples;
    for(int variant=0;variant<PeakRenderVariantCount;++variant){
        GameState renderState=*peakLoadState;
        applyPhoneGraphicsPreset(renderState.localSettings,variant<=Pretty?variant:Pretty);
        if(variant==PrettyNoParticles)renderState.localSettings.particles=false;
        if(variant==PrettyNoActors)for(auto& target:renderState.targets)target.alive=false;
        if(variant==PrettyNoCombatFx){renderState.localSettings.particles=false;renderState.localSettings.shadows=false;renderState.localSettings.portalWindow=false;for(auto& target:renderState.targets)target.alive=false;for(auto& bullet:renderState.bullets)bullet.alive=false;for(auto& flower:renderState.flowers)flower.active=false;renderState.requiredSouls=0;}
        if(variant==PrettyNoCaptures)renderState.requiredSouls=0;
        for(int frame=0;frame<240;++frame){const auto begin=std::chrono::steady_clock::now();host.renderer.draw(renderState);glFinish();const auto end=std::chrono::steady_clock::now();if(frame>=120)presetSamples[variant].push_back(std::chrono::duration<double,std::milli>(end-begin).count());glfwSwapBuffers(window);glfwPollEvents();}
    }
    for(int variant=0;variant<PeakRenderVariantCount;++variant){const auto stats=RuntimePerfTrace::stats(presetSamples[variant]);std::printf("COMBAT_CROWD_PRESET preset=%s render_avg=%.3f render_p95=%.3f render_max=%.3f\n",variantNames[variant],stats.average,stats.p95,stats.maximum);}
    int originalWidth=1,originalHeight=1;glfwGetFramebufferSize(window,&originalWidth,&originalHeight);
    constexpr std::array<float,3> resolutionScales{{0.50f,0.75f,1.0f}};
    std::array<std::vector<double>,resolutionScales.size()> resolutionSamples;
    for(std::size_t scaleIndex=0;scaleIndex<resolutionScales.size();++scaleIndex){
        const float scale=resolutionScales[scaleIndex];host.renderer.resize(std::max(1,static_cast<int>(originalWidth*scale)),std::max(1,static_cast<int>(originalHeight*scale)));
        GameState renderState=*peakLoadState;applyPhoneGraphicsPreset(renderState.localSettings,Pretty);
        for(int frame=0;frame<240;++frame){const auto begin=std::chrono::steady_clock::now();host.renderer.draw(renderState);glFinish();const auto end=std::chrono::steady_clock::now();if(frame>=120)resolutionSamples[scaleIndex].push_back(std::chrono::duration<double,std::milli>(end-begin).count());glfwSwapBuffers(window);glfwPollEvents();}
    }
    host.renderer.resize(originalWidth,originalHeight);
    for(std::size_t scaleIndex=0;scaleIndex<resolutionScales.size();++scaleIndex){const auto stats=RuntimePerfTrace::stats(resolutionSamples[scaleIndex]);std::printf("COMBAT_CROWD_RESOLUTION scale=%.2f width=%d height=%d render_avg=%.3f render_p95=%.3f render_max=%.3f\n",resolutionScales[scaleIndex],std::max(1,static_cast<int>(originalWidth*resolutionScales[scaleIndex])),std::max(1,static_cast<int>(originalHeight*resolutionScales[scaleIndex])),stats.average,stats.p95,stats.maximum);}
    const auto audioStats=RuntimePerfTrace::stats(audioSamples);std::printf("COMBAT_CROWD_AUDIO samples=%zu update_avg=%.3f update_p95=%.3f update_max=%.3f\n",audioSamples.size(),audioStats.average,audioStats.p95,audioStats.maximum);
    std::printf("COMBAT_CROWD_STRESS_OK waves=%d enemies=%d wave_early=%.3f wave_late=%.3f wave_ratio=%.3f snapshot_early=%.3f snapshot_late=%.3f snapshot_ratio=%.3f peak_particles=%d peak_fragments=%d peak_souls=%d\n",waves,crowd,waveEarly,waveLate,waveEarly>0?waveLate/waveEarly:0,early.average,late.average,early.average>0?late.average/early.average:0,peakParticles,peakFragments,peakSouls);return 0;
}

void printUsage() {
    std::printf("Data native desktop host\n");
    std::printf("  build: %s\n", desktopBuildIdentityLine().c_str());
    std::printf("  --tv-room-test       Local lab exploit: start level 10 beside the awakened TV-room entrance.\n");
    std::printf("  --tv-room-enter      Local lab exploit: start directly inside the TV room.\n");
    std::printf("  --smoke-test         Run the desktop smoke test and exit.\n");
    std::printf("  --combat-render-stress  Measure ten repeated kill/capture/respawn cycles.\n");
    std::printf("  --combat-crowd-stress   Measure repeated six-enemy overlapping combat waves.\n");
    std::printf("  --capture-soul-lifecycle DIR  Capture two annotated soul lifecycle cycles.\n");
    std::printf("  --save-roundtrip-test  Verify persistent save write and reload.\n");
    std::printf("  --check-updates      Check the latest native manifest and exit.\n");
    std::printf("  --parity-proximity-test  Run the camera/player wall parity test and exit.\n");
    std::printf("  --controller-test    Print connected controller state once and exit.\n");
    std::printf("  --controller-live-test   Stream controller state for a short live test.\n");
    std::printf("  --build-identity-json    Print machine-readable build identity and exit.\n");
    std::printf("  --capture-frame PATH Capture a hidden frame and exit.\n");
    std::printf("  --capture-spectator-frame PATH  Capture the multiplayer spectator presentation.\n");
    std::printf("  --capture-menu-frame PATH --menu-page NAME  Capture a phone menu page and exit.\n");
    std::printf("  --capture-cpu-demo DIR  Record a HUD-free deterministic gameplay vignette as PPM frames.\n");
    std::printf("  --capture-cinematic-demo DIR  Record the real lunge/capture sequence from a cinematic spectator camera.\n");
    std::printf("  --capture-width N --capture-height N  Set capture framebuffer dimensions.\n");
    std::printf("  --capture-hide-hud  Hide framebuffer HUD elements in visual captures.\n");
    std::printf("  --perf-trace FILE   Record one-second runtime performance summaries as CSV.\n");
    std::printf("  --net-latency-ms N --net-jitter-ms N  Enable explicit deterministic network impairment.\n");
    std::printf("  --net-drop-snapshot-every N --net-drop-input-every N --net-seed N\n");
}

void printBuildIdentityJson() {
    const BuildIdentity& identity = desktopBuildIdentity();
    std::printf(
        "{\"commit\":\"%s\",\"commit_short\":\"%s\",\"protocol\":%u,"
        "\"gameplay\":%u,\"save_format\":%d,\"platform\":\"%s\","
        "\"architecture\":\"%s\",\"configuration\":\"%s\"}\n",
        identity.commit.c_str(),
        identity.commitShort.c_str(),
        static_cast<unsigned int>(identity.protocolVersion),
        static_cast<unsigned int>(identity.gameplayVersion),
        identity.saveFormatVersion,
        identity.platform.c_str(),
        identity.architecture.c_str(),
        identity.buildConfiguration.c_str()
    );
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
int argInt(int argc,char** argv,const char* expected,int fallback=0){const char* value=argValue(argc,argv,expected);if(!value)return fallback;try{return std::stoi(value);}catch(...){return fallback;}}
bool captureFramebuffer(const std::filesystem::path& path,int width,int height){std::error_code directoryError;if(path.has_parent_path())std::filesystem::create_directories(path.parent_path(),directoryError);if(directoryError)return false;std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3u);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());std::ofstream out(path,std::ios::binary);if(!out)return false;out<<"P6\n"<<width<<" "<<height<<"\n255\n";for(int y=height-1;y>=0;--y)out.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(y)*width*3u),static_cast<std::streamsize>(width*3));return static_cast<bool>(out);}

int runSoulLifecycleCapture(GLFWwindow* window,HostState& host,const std::filesystem::path& outputDirectory,int width,int height){
    std::error_code error;std::filesystem::create_directories(outputDirectory,error);
    if(error){std::fprintf(stderr,"SOUL_LIFECYCLE_CAPTURE_FAIL create_directory=%s\n",error.message().c_str());return 1;}
    std::ofstream manifest(outputDirectory/"manifest.csv",std::ios::trunc);
    if(!manifest){std::fprintf(stderr,"SOUL_LIFECYCLE_CAPTURE_FAIL manifest\n");return 1;}
    manifest<<"cycle,label,game_frame,souls,target_alive,soul_state,ingest,morph,cube,visibility,target_scale,visual_x,visual_y,visual_z,rotation_y,target_x,target_y,target_z,camera_x,camera_y,camera_z,particles,file\n";
    host.game.reset();GameState& initial=host.game.networkMutableState();
    for(auto& target:initial.targets)target=TargetState{};
    for(auto& request:initial.respawnQueue)request=HumanRespawnRequest{};
    initial.cinematic=CinematicState{};initial.localSettings.shadows=true;initial.localSettings.particles=true;
    const auto prepare=[&](int targetIndex){GameState& state=host.game.networkMutableState();state.player.pos={0,0.08f,0};state.player.battery=100;state.player.vel={};state.player.jumpVel=0;state.player.grounded=true;state.camera.yaw=0;state.camera.pitch=0;state.camera.forward={0,0,-1};TargetState& target=state.targets[targetIndex];target=TargetState{};target.alive=true;target.armor=0.10f;target.health=1;target.attackCooldown=999;target.pos=state.player.pos+Vec3{0,0,-0.75f};target.walkTarget=target.pos;};
    prepare(0);
    const auto step=[&](bool vacuum,bool melee){host.game.setTouchControls(0,0,0,0,vacuum,false,false,melee,false,false);host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));};
    const auto capture=[&](int cycle,const char* label,int targetIndex){const GameState& state=host.game.state();const TargetState& target=state.targets[targetIndex];const Vec3 visualScale=target.soulVisual.scale;const bool visualFinite=std::isfinite(visualScale.x)&&std::isfinite(visualScale.y)&&std::isfinite(visualScale.z)&&std::isfinite(target.soulVisual.rotationY);const bool visualUniform=std::abs(visualScale.x-visualScale.y)<0.0001f&&std::abs(visualScale.x-visualScale.z)<0.0001f;if(!visualFinite||!visualUniform){std::fprintf(stderr,"SOUL_LIFECYCLE_CAPTURE_FAIL cycle=%d phase=%s visual=(%.6f,%.6f,%.6f) rotation=%.6f\n",cycle,label,visualScale.x,visualScale.y,visualScale.z,target.soulVisual.rotationY);return false;}int particles=0;for(const auto& particle:state.particles)if(particle.life>0)++particles;char filename[96]{};std::snprintf(filename,sizeof(filename),"cycle%d_%s.ppm",cycle,label);host.renderer.draw(state);glFinish();const bool saved=captureFramebuffer(outputDirectory/filename,width,height);glfwSwapBuffers(window);glfwPollEvents();manifest<<cycle<<','<<label<<','<<state.frame<<','<<state.player.souls<<','<<(target.alive?1:0)<<','<<static_cast<int>(target.soulState)<<','<<target.ingestProgress<<','<<target.soulMorph<<','<<target.soulCubeAmount<<','<<target.visibility<<','<<target.scale<<','<<visualScale.x<<','<<visualScale.y<<','<<visualScale.z<<','<<target.soulVisual.rotationY<<','<<target.pos.x<<','<<target.pos.y<<','<<target.pos.z<<','<<state.camera.pos.x<<','<<state.camera.pos.y<<','<<state.camera.pos.z<<','<<particles<<','<<filename<<'\n';manifest.flush();std::printf("SOUL_LIFECYCLE_FRAME cycle=%d label=%s frame=%d alive=%d state=%d ingest=%.3f morph=%.3f cube=%.3f rotation=%.3f file=%s\n",cycle,label,state.frame,target.alive?1:0,static_cast<int>(target.soulState),target.ingestProgress,target.soulMorph,target.soulCubeAmount,target.soulVisual.rotationY,filename);return saved;};
    for(int cycle=1;cycle<=2;++cycle){
        int targetIndex=-1;for(int i=0;i<TARGET_COUNT;++i)if(gameplay::isActiveHuman(host.game.state().targets[i])){targetIndex=i;break;}
        if(targetIndex<0){std::fprintf(stderr,"SOUL_LIFECYCLE_CAPTURE_FAIL cycle=%d phase=population\n",cycle);return 1;}
        prepare(targetIndex);if(!capture(cycle,"00_living",targetIndex))return 1;
        step(false,true);step(false,false);if(!host.game.state().targets[targetIndex].slurpable){std::fprintf(stderr,"SOUL_LIFECYCLE_CAPTURE_FAIL cycle=%d phase=exposure\n",cycle);return 1;}if(!capture(cycle,"01_exposed",targetIndex))return 1;
        int frames=0;while(host.game.state().targets[targetIndex].soulMorph<0.50f&&frames++<180)step(false,false);if(!capture(cycle,"02_morph_mid",targetIndex))return 1;
        while(host.game.state().targets[targetIndex].soulMorph<0.995f&&frames++<240)step(false,false);if(host.game.state().targets[targetIndex].soulMorph<0.995f){std::fprintf(stderr,"SOUL_LIFECYCLE_CAPTURE_FAIL cycle=%d phase=morph\n",cycle);return 1;}if(!capture(cycle,"03_free",targetIndex))return 1;
        {GameState& state=host.game.networkMutableState();TargetState& target=state.targets[targetIndex];target.pos=state.player.pos+Vec3{0,0.50f,-3.0f};target.walkTarget=target.pos;target.vel={};state.camera.yaw=0;state.camera.pitch=0;state.camera.forward={0,0,-1};}
        frames=0;while(host.game.state().targets[targetIndex].soulState!=SoulState::Attracted&&frames++<180)step(true,false);if(host.game.state().targets[targetIndex].soulState==SoulState::Attracted&&!capture(cycle,"04_attracted",targetIndex))return 1;
        while(host.game.state().targets[targetIndex].soulState!=SoulState::Latched&&host.game.state().targets[targetIndex].soulState!=SoulState::Ingesting&&frames++<300)step(true,false);if(frames>=300){std::fprintf(stderr,"SOUL_LIFECYCLE_CAPTURE_FAIL cycle=%d phase=latch\n",cycle);return 1;}if(!capture(cycle,"05_latched",targetIndex))return 1;
        while(host.game.state().targets[targetIndex].alive&&host.game.state().targets[targetIndex].ingestProgress<0.25f&&frames++<420)step(true,false);if(!capture(cycle,"06_ingest_25",targetIndex))return 1;
        while(host.game.state().targets[targetIndex].alive&&host.game.state().targets[targetIndex].ingestProgress<0.75f&&frames++<540)step(true,false);if(!capture(cycle,"07_ingest_75",targetIndex))return 1;
        const int soulsBefore=host.game.state().player.souls;while(host.game.state().targets[targetIndex].alive&&frames++<720)step(true,false);if(host.game.state().player.souls!=soulsBefore+1){std::fprintf(stderr,"SOUL_LIFECYCLE_CAPTURE_FAIL cycle=%d phase=capture\n",cycle);return 1;}if(!capture(cycle,"08_captured",targetIndex))return 1;
        step(false,false);frames=0;while(activeHumanCount(host.game.state())==0&&frames++<300)step(false,false);int respawned=-1;for(int i=0;i<TARGET_COUNT;++i)if(gameplay::isActiveHuman(host.game.state().targets[i])){respawned=i;break;}if(respawned<0){std::fprintf(stderr,"SOUL_LIFECYCLE_CAPTURE_FAIL cycle=%d phase=respawn\n",cycle);return 1;}if(!capture(cycle,"09_respawned",respawned))return 1;
    }
    std::printf("SOUL_LIFECYCLE_CAPTURE_OK directory=%s\n",outputDirectory.string().c_str());return 0;
}

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
    if (hasArg(argc, argv, "--build-identity-json")) {
        printBuildIdentityJson();
        return 0;
    }
    if (hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h")) {
        printUsage();
        return 0;
    }
    const bool captureHuman=argValue(argc,argv,"--capture-human-frame")!=nullptr;
    const bool captureSoul=argValue(argc,argv,"--capture-soul-frame")!=nullptr;
    const bool captureStart=argValue(argc,argv,"--capture-start-frame")!=nullptr;
    const bool capturePaused=argValue(argc,argv,"--capture-paused-frame")!=nullptr;
    const bool captureMosh=argValue(argc,argv,"--capture-mosh-frame")!=nullptr;
    const bool capturePhone=argValue(argc,argv,"--capture-phone-frame")!=nullptr;
    const bool captureMenu=argValue(argc,argv,"--capture-menu-frame")!=nullptr;
    const bool captureSpectator=argValue(argc,argv,"--capture-spectator-frame")!=nullptr;
    const char* captureDemoDir=argValue(argc,argv,"--capture-cpu-demo");
    const char* captureCinematicDir=argValue(argc,argv,"--capture-cinematic-demo");
    const bool captureCinematic=captureCinematicDir!=nullptr;
    if(captureCinematic)captureDemoDir=captureCinematicDir;
    const bool captureDemo=captureDemoDir!=nullptr;
    const bool captureHideHud=hasArg(argc,argv,"--capture-hide-hud");
    const char* perfTracePath=argValue(argc,argv,"--perf-trace");
    const char* captureMenuPage=argValue(argc,argv,"--menu-page");
    const bool captureMenuPause=captureMenu&&captureMenuPage&&std::strcmp(captureMenuPage,"pause")==0;
    const bool tvRoomTest=hasArg(argc,argv,"--tv-room-test");
    const bool tvRoomEnter=hasArg(argc,argv,"--tv-room-enter");
    const bool multiplayerParityTest=hasArg(argc,argv,"--multiplayer-parity-test");
    const bool multiplayerTest=hasArg(argc,argv,"--multiplayer-test")||multiplayerParityTest;
    const bool combatRenderStress=hasArg(argc,argv,"--combat-render-stress");
    const bool combatCrowdStress=hasArg(argc,argv,"--combat-crowd-stress");
    const char* soulLifecycleDirectory=argValue(argc,argv,"--capture-soul-lifecycle");
    const char* capturePath=captureHuman?argValue(argc,argv,"--capture-human-frame"):(captureSoul?argValue(argc,argv,"--capture-soul-frame"):(captureStart?argValue(argc,argv,"--capture-start-frame"):(capturePaused?argValue(argc,argv,"--capture-paused-frame"):(captureMosh?argValue(argc,argv,"--capture-mosh-frame"):(capturePhone?argValue(argc,argv,"--capture-phone-frame"):(captureMenu?argValue(argc,argv,"--capture-menu-frame"):(captureSpectator?argValue(argc,argv,"--capture-spectator-frame"):argValue(argc,argv,"--capture-frame"))))))));
    const int windowWidth=std::max(320,std::min(7680,argInt(argc,argv,"--capture-width",1280)));
    const int windowHeight=std::max(180,std::min(4320,argInt(argc,argv,"--capture-height",720)));
    if (hasArg(argc, argv, "--smoke-test")) {
        return runSmokeTest();
    }
    if (hasArg(argc, argv, "--save-roundtrip-test")) {
        return runSaveRoundtripTest();
    }
    if (hasArg(argc, argv, "--parity-proximity-test")) {
        return runParityProximityTest();
    }
    if (hasArg(argc, argv, "--model-test")) {
        return runModelTest(std::filesystem::absolute(argv[0]).parent_path());
    }
    if (hasArg(argc, argv, "--check-updates")) {
        DesktopUpdateService updater;
        updater.checkForUpdates(desktopBuildIdentity());
        updater.disconnect();
        const DesktopUpdateService::State result = updater.state();
        return result == DesktopUpdateService::State::Failed ? 1 : 0;
    }

    glfwSetErrorCallback(errorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Data: GLFW initialization failed.\n");
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
    if(capturePath||multiplayerTest||combatRenderStress||combatCrowdStress||soulLifecycleDirectory)glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(
        windowWidth,
        windowHeight,
        "Data",
        nullptr,
        nullptr
    );
    if (!window) {
        glfwTerminate();
        std::fprintf(stderr, "Data: window creation failed.\n");
        return 1;
    }

    HostState host;
    host.progressionPath=progressionSavePath();
    bool recoveredPersistentSave=false;
    bool loadedPersistentSave=loadProgressionWithBackup(host.game,host.progressionPath,&recoveredPersistentSave);
#ifdef __APPLE__
    if(!loadedPersistentSave){
        const std::filesystem::path legacyPath=legacyTemporaryProgressionSavePath();
        if(legacyPath!=host.progressionPath&&loadProgression(host.game,legacyPath)){
            loadedPersistentSave=saveProgression(host.game.state().progression.permanent,host.game.state().localSettings,host.progressionPath);
            std::printf("Migrated legacy macOS save to %s\n",host.progressionPath.string().c_str());
        }
    }
#endif
    std::printf("Persistent save: %s%s\n",host.progressionPath.string().c_str(),recoveredPersistentSave?" (recovered backup)":(loadedPersistentSave?" (loaded)":""));
    if(const char* service=std::getenv("DIGITAL_BREAKDOWN_MULTIPLAYER_URL"))host.multiplayerService=service;
    host.game.reset();
    if((!capturePath&&!captureDemo)||captureStart)host.game.prepareAttractScreen();
    if(captureMenu){
        const char* page=captureMenuPage;
        GameState& fixture=host.game.networkMutableState();
        host.game.prepareStartScreen();
        fixture.localSettings.menuPage=LocalMenuPage::Main;
        fixture.localSettings.menuScroll=0.0f;
        if(page&&std::strcmp(page,"pause")==0){host.game.restart();fixture.cinematic.introActive=false;host.game.setUiPaused(true);}
        else if(page&&std::strcmp(page,"online")==0)fixture.localSettings.menuPage=LocalMenuPage::Online;
        else if(page&&std::strcmp(page,"settings")==0)fixture.localSettings.menuPage=LocalMenuPage::Settings;
        else if(page&&std::strcmp(page,"controls-bottom")==0){fixture.localSettings.menuPage=LocalMenuPage::Controls;fixture.hud.menuSelection=15;fixture.localSettings.menuScroll=999.0f;}
        else if(page&&std::strcmp(page,"controls-middle")==0){fixture.localSettings.menuPage=LocalMenuPage::Controls;fixture.hud.menuSelection=7;fixture.localSettings.menuScroll=360.0f;}
        else if(page&&std::strncmp(page,"controls",8)==0){fixture.localSettings.menuPage=LocalMenuPage::Controls;fixture.localSettings.menuScroll=0.0f;}
        else if(page&&std::strcmp(page,"audio")==0)fixture.localSettings.menuPage=LocalMenuPage::Audio;
        else if(page&&std::strcmp(page,"graphics")==0)fixture.localSettings.menuPage=LocalMenuPage::Graphics;
        else if(page&&std::strcmp(page,"death")==0){fixture.dead=true;fixture.started=false;fixture.player.alive=false;fixture.player.battery=0.0f;}
        else fixture.localSettings.menuPage=LocalMenuPage::Main;
        if(!(page&&std::strcmp(page,"controls-bottom")==0)&&!(page&&std::strcmp(page,"controls-middle")==0))fixture.hud.menuSelection=0;
    }
    if(tvRoomTest||tvRoomEnter){
        host.game.debugStartSecretTvTest(tvRoomEnter);
        const auto& tv=host.game.state().secretTv;
        std::printf("TV_ROOM_TEST_%s entrance=(%.2f, %.2f, %.2f) normal=(%.1f, %.1f, %.1f)\n",
            tvRoomEnter?"ENTER":"AWAKE",
            tv.entrancePos.x,tv.entrancePos.y,tv.entrancePos.z,
            tv.entranceNormal.x,tv.entranceNormal.y,tv.entranceNormal.z);
    }
    host.savedProgressionRevision=host.game.state().progression.permanent.revision;
    host.savedSettings=host.game.state().localSettings;
    host.previousPermanentLevels=host.game.state().progression.permanent.levels;
    host.previousPlayerAlive=host.game.state().player.alive;
    host.lastHapticAudioSerial=host.game.state().audio.nextSerial>0?host.game.state().audio.nextSerial-1:0;
    if(captureHuman){GameState& fixture=const_cast<GameState&>(host.game.state());for(auto& target:fixture.targets)target.alive=false;auto& target=fixture.targets[0];target.alive=true;target.slurpable=false;target.pos={0,0.08f,fixture.player.pos.z-4.0f};target.walkTarget=target.pos;target.visualYaw=0;target.scale=1;target.visibility=1;target.attackCooldown=999;fixture.camera.yaw=0;fixture.camera.pitch=0;}
    if(captureSoul){GameState& fixture=const_cast<GameState&>(host.game.state());for(int i=1;i<TARGET_COUNT;++i)fixture.targets[i].alive=false;auto& target=fixture.targets[0];target.alive=true;target.slurpable=true;target.soulMorph=1;target.soulCubeAmount=1;target.pos=fixture.player.pos+Vec3{0,0.5f,-1.5f};target.walkTarget=target.pos;target.health=1;target.armor=0;target.soulState=SoulState::Free;fixture.camera.yaw=0;fixture.camera.pitch=0;}
    if(capturePaused)host.game.setUiPaused(true);
    if(capturePhone){GameState& fixture=const_cast<GameState&>(host.game.state());for(auto& target:fixture.targets)target.alive=false;}
    if(captureSpectator){
        host.game.configureNetworkHost();
        host.game.setNetworkPeerActive(1,true);
        GameState& fixture=host.game.networkMutableState();
        for(auto& target:fixture.targets)target.alive=false;
        fixture.player.alive=false;
        fixture.player.downed=false;
        auto& peer=fixture.multiplayer.peers[1];
        peer.player.alive=true;
        peer.player.downed=false;
        peer.player.pos={0.4f,0.08f,-2.2f};
        peer.player.yaw=-0.35f;
        peer.input.touchMoveZ=0.72f;
        peer.input.touchSprint=true;
    }
    if(captureDemo){
        std::filesystem::create_directories(captureDemoDir);
        host.game.restart();
        GameState& fixture=host.game.networkMutableState();
        fixture.camera.firstPerson=false;
        if(captureCinematic)fixture.cinematic.introActive=false;
        for(auto& target:fixture.targets)target.alive=false;
        auto& target=fixture.targets[0];
        target=TargetState{};
        target.alive=true;
        target.pos=fixture.player.pos+Vec3{0.15f,0.0f,captureCinematic?-2.65f:-1.60f};
        target.walkTarget=target.pos;
        target.visualYaw=0.0f;
        target.scale=captureCinematic?1.18f:1.08f;
        target.attackCooldown=999.0f;
        target.armor=0.0f;
        if(captureCinematic)target.health=0.20f;
        fixture.localSettings.particles=true;
        fixture.localSettings.fpsCounter=false;
    }
    host.audio.setAssetRoot(std::filesystem::absolute(argv[0]).parent_path()/"audio");

    glfwSetWindowUserPointer(window, &host);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetWindowFocusCallback(window, windowFocusCallback);
    glfwSetFramebufferSizeCallback(window, framebufferCallback);

    glfwMakeContextCurrent(window);
    host.renderer.setAssetRoot(std::filesystem::absolute(argv[0]).parent_path()/"models");
    // Let the platform compositor pace presentation while gameplay remains fixed
    // at 60 Hz. The renderer interpolates camera state between simulation ticks.
    glfwSwapInterval((multiplayerTest||combatRenderStress||combatCrowdStress||soulLifecycleDirectory)?0:1);
    setMouseCaptured(window, host, host.game.state().started&&!host.game.state().attractMode);
    if(capturePaused||captureMenuPause)host.game.setUiPaused(true);

    int framebufferWidth = 1;
    int framebufferHeight = 1;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    host.renderer.resize(framebufferWidth, framebufferHeight);
    if(captureDemo||captureHideHud)host.renderer.setHudVisible(false);
    if(combatRenderStress){const int result=runCombatRenderStress(window,host);glfwDestroyWindow(window);host.audio.stopAll();glfwTerminate();return result;}
    if(combatCrowdStress){const int result=runCombatCrowdStress(window,host);glfwDestroyWindow(window);host.audio.stopAll();glfwTerminate();return result;}
    if(soulLifecycleDirectory){const int result=runSoulLifecycleCapture(window,host,soulLifecycleDirectory,framebufferWidth,framebufferHeight);glfwDestroyWindow(window);host.audio.stopAll();glfwTerminate();return result;}
    if(!tvRoomTest&&!tvRoomEnter){
        host.multiplayer.configureImpairment(
            argInt(argc,argv,"--net-latency-ms"),
            argInt(argc,argv,"--net-jitter-ms"),
            argInt(argc,argv,"--net-drop-snapshot-every"),
            argInt(argc,argv,"--net-drop-input-every"),
            static_cast<std::uint32_t>(std::max(1,argInt(argc,argv,"--net-seed",1))));
        if(const char* service=argValue(argc,argv,"--service-url"))host.multiplayerService=service;
        if(hasArg(argc,argv,"--host-room"))host.multiplayer.host(host.multiplayerService);
        if(const char* room=argValue(argc,argv,"--join-room"))host.multiplayer.join(host.multiplayerService,room);
    }

    std::printf("Data native desktop host running.\n");
    std::printf("Build identity: %s\n", desktopBuildIdentityLine().c_str());
    std::printf("WASD move | Shift sprint | Space jump | Mouse look | Left mouse vacuum | F melee | Q shoot | C camera | Tab release mouse | Esc quit\n");

    auto previous = std::chrono::steady_clock::now();
    double simulationAccumulator = 0.0;
    auto previousCamera = host.game.state().camera;
    auto previousPhoneTransform = host.game.state().phoneTransform;
    int captureFrames=0;
    bool captureDemoSucceeded=false;
    bool multiplayerAutoStartIssued=false;
    int multiplayerParityFrame=0;
    bool multiplayerMetricsPrinted=false;
    bool multiplayerVacuumPredicted=false;
    bool multiplayerDischargeIssued=false;
    bool multiplayerMeleeDurable=false;
    bool multiplayerSoulDurable=false;
    bool multiplayerProjectileDurable=false;
    bool multiplayerProjectileTerminal=false;
    int multiplayerSoulStoredFrame=-1;
    RuntimePerfTrace perfTrace(perfTracePath);
    if(perfTracePath&&!perfTrace.active()){std::fprintf(stderr,"PERF_TRACE_FAILED path=%s\n",perfTracePath);glfwDestroyWindow(window);host.audio.stopAll();glfwTerminate();return 1;}
    if(perfTrace.active())std::printf("PERF_TRACE_ACTIVE path=%s\n",perfTracePath);
    while (!glfwWindowShouldClose(window)) {
        const auto frameBegin=std::chrono::steady_clock::now();
        if(multiplayerTest){
            if(multiplayerParityTest&&host.multiplayer.phase()==dbmultiplayer::Phase::Playing&&
               host.multiplayer.role()==DesktopMultiplayer::Role::Guest){
                const bool melee=multiplayerParityFrame>=129&&multiplayerParityFrame<499&&
                    ((multiplayerParityFrame-129)%60)<30;
                const bool vacuum=multiplayerParityFrame>=619&&
                    host.game.state().player.souls==0;
                if(host.game.state().player.souls>0&&multiplayerSoulStoredFrame<0)
                    multiplayerSoulStoredFrame=multiplayerParityFrame;
                const int dischargeAge=multiplayerSoulStoredFrame<0?0:
                    multiplayerParityFrame-multiplayerSoulStoredFrame;
                const bool shoot=multiplayerSoulStoredFrame>=0&&
                    host.game.state().player.souls>0&&dischargeAge>=10&&
                    dischargeAge%20==10;
                host.game.setTouchControls(0,0,0,0,vacuum,false,false,melee,
                                           shoot,false);
                if(shoot&&!multiplayerDischargeIssued){
                    multiplayerDischargeIssued=true;
                    std::printf("MULTIPLAYER_DISCHARGE_PREDICTED frame=%d\n",
                                multiplayerParityFrame);
                    std::fflush(stdout);
                }
            }
            host.multiplayer.update(host.game);
            if(!multiplayerAutoStartIssued&&hasArg(argc,argv,"--auto-start-multiplayer")&&
               host.multiplayer.role()==DesktopMultiplayer::Role::Host&&
               host.multiplayer.phase()==dbmultiplayer::Phase::Lobby&&host.multiplayer.playerCount()==2)
                multiplayerAutoStartIssued=host.multiplayer.startMatch();
            if(multiplayerParityTest&&host.multiplayer.phase()==dbmultiplayer::Phase::Playing){
                ++multiplayerParityFrame;
                const bool guest=host.multiplayer.role()==DesktopMultiplayer::Role::Guest;
                const bool move=guest&&multiplayerParityFrame<80;
                const bool jump=guest&&(multiplayerParityFrame==30||multiplayerParityFrame==75);
                const bool melee=guest&&multiplayerParityFrame>=130&&multiplayerParityFrame<500&&
                    ((multiplayerParityFrame-130)%60)<30;
                const bool vacuum=guest&&multiplayerParityFrame>=620&&
                    host.game.state().player.souls==0;
                if(!guest&&multiplayerParityFrame==100){
                    GameState& fixture=host.game.networkMutableState();
                    fixture.player.pos={8,0.08f,8};
                    fixture.multiplayer.peers[1].player.pos={0,0.08f,0};
                    fixture.multiplayer.peers[1].player.vel={};
                    fixture.multiplayer.peers[1].player.grounded=true;
                    fixture.multiplayer.peers[1].player.battery=100;
                    for(auto& target:fixture.targets)target.alive=false;
                    auto& enemy=fixture.targets[0];enemy=TargetState{};enemy.alive=true;
                    enemy.pos={0,0.08f,-0.7f};enemy.walkTarget=enemy.pos;enemy.armor=0.45f;
                    std::printf("MULTIPLAYER_COMBAT_FIXTURE enemy=0 armor=%.2f\n",enemy.armor);
                    std::fflush(stdout);
                }
                if(!guest&&multiplayerParityFrame>=100&&multiplayerParityFrame<2500){
                    GameState& fixture=host.game.networkMutableState();
                    fixture.multiplayer.peers[1].player.pos={0,0.08f,0};
                    fixture.multiplayer.peers[1].player.vel={};
                    auto& enemy=fixture.targets[0];
                    if(enemy.alive&&!enemy.slurpable){enemy.pos={0,0.08f,-0.7f};enemy.walkTarget=enemy.pos;enemy.attackCooldown=10.0f;}
                }
                if(!guest&&multiplayerParityFrame==600){
                    GameState& fixture=host.game.networkMutableState();
                    auto& soul=fixture.targets[1];
                    for(std::size_t i=0;i<fixture.targets.size();++i)
                        if(i!=1)fixture.targets[i].alive=false;
                    soul=TargetState{};soul.alive=true;soul.slurpable=true;
                    soul.soulMorph=1.0f;soul.health=1.0f;
                    soul.pos={0,0.57f,-2.0f};soul.walkTarget=soul.pos;
                    std::printf("MULTIPLAYER_VACUUM_FIXTURE target=1\n");
                    std::fflush(stdout);
                }
                if(!guest&&multiplayerParityFrame>=600&&multiplayerParityFrame<2500){
                    GameState& fixture=host.game.networkMutableState();
                    auto& soul=fixture.targets[1];
                    if(soul.alive&&soul.soulState==SoulState::Free)
                        soul.pos={0,0.57f,-2.0f};
                    if(soul.soulState==SoulState::Latched||
                       soul.soulState==SoulState::Ingesting)
                        soul.ingestProgress=std::max(soul.ingestProgress,0.82f);
                    for(auto& projectile:fixture.bullets)
                        if(projectile.alive)
                            projectile.life=std::min(projectile.life,0.12f);
                }
                host.game.setTouchControls(0.0f,move?1.0f:0.0f,0.0f,0.0f,
                                           vacuum,move,jump,melee,false,false);
                if(multiplayerParityFrame==130&&guest){std::printf("MULTIPLAYER_TEST_GUEST_MELEE frame=%d\n",multiplayerParityFrame);std::fflush(stdout);}
                if(vacuum&&!multiplayerVacuumPredicted){
                    multiplayerVacuumPredicted=true;
                    std::printf("MULTIPLAYER_VACUUM_PREDICTED frame=%d\n",
                                multiplayerParityFrame);
                    std::fflush(stdout);
                }
                if(jump){
                    std::printf("MULTIPLAYER_TEST_GUEST_JUMP frame=%d kind=%s\n",
                                multiplayerParityFrame,
                                multiplayerParityFrame==30?"jump":"double_jump");
                    std::fflush(stdout);
                }
                if(multiplayerParityFrame==100)setMouseCaptured(window,host,false);
                if(multiplayerParityFrame==130)setMouseCaptured(window,host,true);
            }
            host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));
            if(multiplayerParityTest&&host.multiplayer.phase()==dbmultiplayer::Phase::Playing){
                if(host.multiplayer.role()==DesktopMultiplayer::Role::Host&&!multiplayerMeleeDurable&&
                   multiplayerParityFrame>=100){
                    const auto& enemy=host.game.state().targets[0];
                    if(enemy.alive&&(enemy.slurpable||enemy.armor<0.44f)){
                        multiplayerMeleeDurable=true;
                        std::printf("MULTIPLAYER_DURABLE_MELEE enemy=0 armor=%.3f shell_broken=%d\n",
                                    enemy.armor,enemy.slurpable?1:0);
                        std::fflush(stdout);
                    }
                }
                const bool parityHost=host.multiplayer.role()==DesktopMultiplayer::Role::Host;
                const int durableGuestSouls=parityHost
                    ?host.game.state().multiplayer.peers[1].player.souls
                    :host.game.state().player.souls;
                if(!multiplayerSoulDurable&&durableGuestSouls>0){
                    multiplayerSoulDurable=true;
                    std::printf("MULTIPLAYER_DURABLE_SOUL role=%s inventory=%d\n",
                                parityHost?"host":"guest",durableGuestSouls);
                    std::fflush(stdout);
                }
                const bool activeProjectile=std::any_of(
                    host.game.state().bullets.begin(),host.game.state().bullets.end(),
                    [](const BulletState& bullet){return bullet.alive;});
                if(!multiplayerProjectileDurable&&activeProjectile){
                    multiplayerProjectileDurable=true;
                    std::printf("MULTIPLAYER_DURABLE_PROJECTILE role=%s state=active\n",
                                host.multiplayer.role()==DesktopMultiplayer::Role::Host?"host":"guest");
                    std::fflush(stdout);
                }else if(multiplayerProjectileDurable&&!multiplayerProjectileTerminal&&
                         !activeProjectile&&durableGuestSouls==0){
                    multiplayerProjectileTerminal=true;
                    std::printf("MULTIPLAYER_DURABLE_PROJECTILE role=%s state=terminal\n",
                                host.multiplayer.role()==DesktopMultiplayer::Role::Host?"host":"guest");
                    std::fflush(stdout);
                }
                if(host.multiplayer.role()==DesktopMultiplayer::Role::Guest&&
                   (multiplayerParityFrame==30||multiplayerParityFrame==75)){
                    std::printf("MULTIPLAYER_TEST_GUEST_JUMP_PREDICTED frame=%d jump_vel=%.3f y=%.3f\n",
                                multiplayerParityFrame,host.game.state().player.jumpVel,
                                host.game.state().player.pos.y);
                    std::fflush(stdout);
                }
                if(multiplayerParityFrame==60&&host.multiplayer.role()==DesktopMultiplayer::Role::Guest){
                    std::printf("MULTIPLAYER_TEST_GUEST_MOVEMENT x=%.3f y=%.3f z=%.3f\n",
                                host.game.state().player.pos.x,host.game.state().player.pos.y,
                                host.game.state().player.pos.z);
                    std::fflush(stdout);
                }
                if((multiplayerParityFrame%30)==0){
                    GameState parityRender=host.game.state();
                    host.multiplayer.applyPresentation(parityRender);
                    const int remoteId=host.multiplayer.role()==DesktopMultiplayer::Role::Guest?0:1;
                    const auto& remote=parityRender.multiplayer.peers[remoteId];
                    if(remote.active){
                        std::printf("MULTIPLAYER_VISUAL_STATE entity=player id=%d tick=%d x=%.3f y=%.3f z=%.3f yaw=%.3f action=%d\n",
                          remoteId,parityRender.frame,remote.player.pos.x,remote.player.pos.y,
                          remote.player.pos.z,remote.player.yaw,remote.phonePose.actionState);
                        std::fflush(stdout);
                    }
                }
                if(!multiplayerMetricsPrinted&&multiplayerParityFrame>=150){
                    host.multiplayer.printMetrics();
                    multiplayerMetricsPrinted=true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(multiplayerParityTest?4:1));
            continue;
        }
        glfwPollEvents();

        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        if (!capturePath&&!captureDemo) simulationAccumulator += std::min(elapsed, MAX_FRAME_DELTA_SECONDS);

        const DesktopGamepadInput gamepad=pollGamepad(window,host);
        if(host.game.state().attractMode&&(std::abs(gamepad.moveX)>0.25f||std::abs(gamepad.moveZ)>0.25f||std::abs(gamepad.lookX)>0.25f||std::abs(gamepad.lookY)>0.25f||gamepad.vacuumHeld||gamepad.sprintHeld||gamepad.jumpPressed||gamepad.meleePressed||gamepad.shootPressed||gamepad.cameraPressed)){
            host.game.dismissAttractMode();setMouseCaptured(window,host,false);
        }
        const bool leftMouseDown=glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if(!leftMouseDown)host.suppressLeftMouseUntilRelease=false;
        const bool vacuumHeld = captureSoul || (leftMouseDown&&!host.suppressLeftMouseUntilRelease) || gamepad.vacuumHeld;
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
        if(captureDemo){
            const int f=captureFrames;
            bool exposedSoul=false;
            for(const auto& target:host.game.state().targets)if(target.alive&&target.slurpable){exposedSoul=true;break;}
            if(captureCinematic&&!exposedSoul&&f<=10){
                GameState& fixture=host.game.networkMutableState();
                fixture.targets[0].pos=fixture.player.pos+Vec3{0.15f,-fixture.player.pos.y,-2.65f};
                fixture.targets[0].walkTarget=fixture.targets[0].pos;
                fixture.targets[0].vel={};
            }
            const bool approach=!captureCinematic&&!exposedSoul&&f<35;
            const bool circle=false;
            const bool retreat=!captureCinematic&&!exposedSoul&&f>=230&&f<270;
            const bool melee=!exposedSoul&&(captureCinematic?f==10:(f==50||f==110||f==170));
            const bool jump=!exposedSoul&&(captureCinematic?f==2:f==210);
            host.game.setTouchControls(
                circle?0.52f:(retreat?-0.28f:0.0f),
                approach?0.66f:(circle?0.14f:(retreat?-0.42f:0.0f)),
                circle?0.26f:(retreat?-0.10f:0.0f),
                0.0f,
                exposedSoul,
                false,
                jump,
                melee,
                false,
                false
            );
        }
        host.lookX = 0.0;
        host.lookY = 0.0;

        const auto updateBegin=std::chrono::steady_clock::now();
        host.multiplayer.update(host.game);
        if(!multiplayerAutoStartIssued&&hasArg(argc,argv,"--auto-start-multiplayer")&&
           host.multiplayer.role()==DesktopMultiplayer::Role::Host&&
           host.multiplayer.phase()==dbmultiplayer::Phase::Lobby&&host.multiplayer.playerCount()==2){
            multiplayerAutoStartIssued=host.multiplayer.startMatch();
        }
        if(host.multiplayer.failed()&&!host.game.state().started&&host.game.state().localSettings.menuPage==LocalMenuPage::JoinCode){
            host.enteringJoinCode=false;
            host.joinCode.clear();
            popMenuPage(host);
        }
        if((!host.game.state().started||host.game.state().upgradeMenu.active)&&host.mouseCaptured)setMouseCaptured(window,host,false);
        if (host.game.state().started && host.game.state().multiplayer.connected && !host.game.state().upgradeMenu.active && !host.game.state().uiPaused && !host.mouseCaptured) {
            setMouseCaptured(window, host, true);
        }

        if(captureMosh&&captureFrames==10){GameState& fixture=const_cast<GameState&>(host.game.state());fixture.doorTransition.active=true;fixture.doorTransition.progress=1.0f;fixture.doorTransition.distanceTravelled=0;fixture.doorTransition.lastPlayerPos=fixture.player.pos;}
        int simulationSteps=0;
        bool droppedAccumulator=false;
        if (capturePath||captureDemo) {
            previousCamera = host.game.state().camera;
            previousPhoneTransform = host.game.state().phoneTransform;
            host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));
            simulationSteps=1;
        } else {
            while (simulationAccumulator >= SIMULATION_STEP_SECONDS && simulationSteps < MAX_SIMULATION_STEPS_PER_FRAME) {
                previousCamera = host.game.state().camera;
                previousPhoneTransform = host.game.state().phoneTransform;
                host.game.update(static_cast<float>(SIMULATION_STEP_SECONDS));
                simulationAccumulator -= SIMULATION_STEP_SECONDS;
                ++simulationSteps;
            }
            if (simulationSteps == MAX_SIMULATION_STEPS_PER_FRAME && simulationAccumulator >= SIMULATION_STEP_SECONDS){
                droppedAccumulator=true;
                simulationAccumulator = std::fmod(simulationAccumulator, SIMULATION_STEP_SECONDS);
            }
        }
        const auto updateEnd=std::chrono::steady_clock::now();
        if(capturePhone){GameState& fixture=const_cast<GameState&>(host.game.state());fixture.camera.pos=fixture.phoneTransform.position+Vec3{0,0.035f,0.38f};fixture.camera.lookTarget=fixture.phoneTransform.position;fixture.camera.forward=normalized(fixture.camera.lookTarget-fixture.camera.pos);}
        updateOutcomeRumble(host);
        const auto audioBegin=std::chrono::steady_clock::now();
        host.audio.update(host.game.state());
        const auto audioEnd=std::chrono::steady_clock::now();
        const auto& permanent=host.game.state().progression.permanent;const auto& settings=host.game.state().localSettings;if(permanent.revision!=host.savedProgressionRevision||!samePersistentSettings(settings,host.savedSettings)){if(saveProgression(permanent,settings,host.progressionPath)){host.savedProgressionRevision=permanent.revision;host.savedSettings=settings;}}
        GameState renderState = host.game.state();
        host.multiplayer.applyPresentation(renderState);
        if(captureMenuPause){
            renderState.started=true;
            renderState.uiPaused=true;
            renderState.multiplayer.enabled=false;
            renderState.upgradeMenu.active=false;
            renderState.camera.firstPerson=false;
            renderState.cinematic.introActive=false;
            renderState.localSettings.menuPage=LocalMenuPage::Main;
        }
        if (!capturePath) {
            const float alpha = clampf(static_cast<float>(simulationAccumulator / SIMULATION_STEP_SECONDS), 0.0f, 1.0f);
            const auto& currentCamera = host.game.state().camera;
            const auto& currentPhoneTransform = host.game.state().phoneTransform;

            const bool cameraCut =
                previousCamera.firstPerson != currentCamera.firstPerson ||
                lengthSq(previousCamera.pos - currentCamera.pos) > 25.0f ||
                lengthSq(previousCamera.lookTarget - currentCamera.lookTarget) > 25.0f;

            const bool phoneCut =
                lengthSq(
                    previousPhoneTransform.position -
                    currentPhoneTransform.position
                ) > 4.0f;

            if (!cameraCut) {
                renderState.camera.pos =
                    previousCamera.pos +
                    (currentCamera.pos - previousCamera.pos) * alpha;
                renderState.camera.lookTarget =
                    previousCamera.lookTarget +
                    (currentCamera.lookTarget - previousCamera.lookTarget) * alpha;
                renderState.camera.forward =
                    normalized(
                        renderState.camera.lookTarget -
                        renderState.camera.pos
                    );
                renderState.time =
                    std::max(
                        0.0f,
                        host.game.state().time -
                        (1.0f - alpha) *
                            static_cast<float>(SIMULATION_STEP_SECONDS)
                    );
            }

            if (!cameraCut && !phoneCut) {
                renderState.phoneTransform =
                    interpolatePhoneTransform(
                        previousPhoneTransform,
                        currentPhoneTransform,
                        alpha
                    );
            }
        }
        if(captureCinematic){
            const Vec3 phone=renderState.phoneTransform.position;
            Vec3 subject=phone;
            bool exposedSoul=false;
            for(const auto& target:renderState.targets){
                if(target.alive){subject=(phone+target.pos)*0.5f;exposedSoul=target.slurpable;break;}
            }
            const float action=clampf(static_cast<float>(captureFrames)/95.0f,0.0f,1.0f);
            renderState.camera.pos=subject+(exposedSoul
                ?Vec3{3.25f,1.35f,2.05f}
                :Vec3{3.35f-action*0.20f,1.18f+action*0.20f,1.45f});
            renderState.camera.lookTarget=subject+Vec3{0.0f,0.52f,exposedSoul?-0.10f:0.0f};
            renderState.camera.forward=normalized(renderState.camera.lookTarget-renderState.camera.pos);
            renderState.camera.verticalFovDegrees=44.0f;
            renderState.camera.firstPerson=false;
        }
        const auto renderBegin=std::chrono::steady_clock::now();
        host.renderer.draw(renderState);
        const auto renderEnd=std::chrono::steady_clock::now();
        if(captureDemo){
            const auto swapBegin=std::chrono::steady_clock::now();
            glfwSwapBuffers(window);
            const auto frameEnd=std::chrono::steady_clock::now();
            perfTrace.sample(host.game.state(),std::chrono::duration<double,std::milli>(frameEnd-frameBegin).count(),std::chrono::duration<double,std::milli>(updateEnd-updateBegin).count(),std::chrono::duration<double,std::milli>(audioEnd-audioBegin).count(),std::chrono::duration<double,std::milli>(renderEnd-renderBegin).count(),std::chrono::duration<double,std::milli>(frameEnd-swapBegin).count(),simulationSteps,droppedAccumulator);
            if((captureFrames%2)==0){
                glReadBuffer(GL_FRONT);
                char frameName[32];
                std::snprintf(frameName,sizeof(frameName),"frame-%04d.ppm",captureFrames/2);
                const auto framePath=std::filesystem::path(captureDemoDir)/frameName;
                const bool captured=captureFramebuffer(framePath,framebufferWidth,framebufferHeight);
                glReadBuffer(GL_BACK);
                if(!captured)std::printf("CAPTURE_CPU_DEMO_FAILED %s\n",framePath.string().c_str());
            }
            const int captureLimit=captureCinematic?180:300;
            if(++captureFrames>=captureLimit){
                captureDemoSucceeded=host.game.state().player.souls>0;
                std::printf("CAPTURE_CPU_DEMO_%s %s stored_souls=%d\n",captureDemoSucceeded?"OK":"FAILED",captureDemoDir,host.game.state().player.souls);
                glfwSetWindowShouldClose(window,GLFW_TRUE);
            }
            continue;
        }
        const int stillCaptureFrames=captureStart?120:30;
        if(capturePath&&++captureFrames>=stillCaptureFrames){
            const bool attractRunning=!captureStart||(host.game.state().attractMode&&!host.game.state().uiPaused);
            const bool captured=attractRunning&&captureFramebuffer(capturePath,framebufferWidth,framebufferHeight);
            std::printf("CAPTURE_FRAME_%s %s\n",captured?"OK":"FAILED",capturePath);
            glfwSetWindowShouldClose(window,GLFW_TRUE);
        }
        const auto swapBegin=std::chrono::steady_clock::now();
        glfwSwapBuffers(window);
        const auto frameEnd=std::chrono::steady_clock::now();
        perfTrace.sample(host.game.state(),std::chrono::duration<double,std::milli>(frameEnd-frameBegin).count(),std::chrono::duration<double,std::milli>(updateEnd-updateBegin).count(),std::chrono::duration<double,std::milli>(audioEnd-audioBegin).count(),std::chrono::duration<double,std::milli>(renderEnd-renderBegin).count(),std::chrono::duration<double,std::milli>(frameEnd-swapBegin).count(),simulationSteps,droppedAccumulator);
    }

    const bool finalSaveOk=saveProgression(host.game.state().progression.permanent,host.game.state().localSettings,host.progressionPath);
    std::printf("Persistent save %s: %s\n",finalSaveOk?"written":"FAILED",host.progressionPath.string().c_str());
    glfwDestroyWindow(window);
    host.audio.stopAll();
    host.multiplayer.disconnect();
    host.updater.disconnect();
    glfwTerminate();
    return captureDemo&&!captureDemoSucceeded?1:0;
}
