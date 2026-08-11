#include <jni.h>
#include <android/log.h>
#include <array>
#include <chrono>
#include <algorithm>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "game/Game.hpp"
#include "render/Renderer.hpp"
#include "MultiplayerProtocol.hpp"
#include "BoundedEventQueue.hpp"

#ifndef DIGITAL_BREAKDOWN_SOURCE_COMMIT
#define DIGITAL_BREAKDOWN_SOURCE_COMMIT "unknown"
#endif

namespace {
constexpr double FIXED_STEP_SECONDS = 1.0 / 60.0;
constexpr double MAX_FRAME_SECONDS = 0.25;
constexpr int MAX_STEPS_PER_FRAME = 4;

Game gGame;
Renderer gRenderer;
auto gLastFrame = std::chrono::steady_clock::now();
double gAccumulatorSeconds = 0.0;
unsigned int gLastAudioSerial = 0;
jclass gBridgeClass = nullptr;
jmethodID gPlayAudioCue = nullptr;
jmethodID gSyncMusic = nullptr;
jmethodID gSendNetworkPacket = nullptr;
jmethodID gSaveProgression = nullptr;
std::uint64_t gLastProgressionRevision = 0;
struct NetworkEvent { enum Kind { Solo, Configure, Control, Binary } kind; bool host=false; int playerId=0; std::string room,status,text; std::vector<std::uint8_t> bytes; };
std::mutex gNetworkMutex;
std::deque<NetworkEvent> gNetworkEvents;
void enqueueNetworkEvent(NetworkEvent&& event){std::lock_guard<std::mutex> lock(gNetworkMutex);if(dbnet::pushBoundedIncoming(gNetworkEvents,std::move(event)))__android_log_print(ANDROID_LOG_WARN,"DBNATIVE","network queue drop limit=%zu",dbnet::MAX_INCOMING_EVENTS);}
bool gNetworkHost=false;
bool gNetworkConfigured=false;
int gNetworkPlayerId=0;
std::uint32_t gNetworkSequence=0;
std::uint32_t gLastSnapshotTick=0;
std::uint32_t gLastSnapshotSequence=0;
std::array<std::uint32_t, NETWORK_PLAYER_COUNT> gLastInputSequence{};
dbnet::NetworkWorldContext gWorldContext{};

int jsonInt(const std::string& json,const char* key,int fallback=-1){const std::string needle=std::string("\"")+key+"\"";std::size_t at=json.find(needle);if(at==std::string::npos)return fallback;at=json.find(':',at+needle.size());if(at==std::string::npos)return fallback;try{return std::stoi(json.substr(at+1));}catch(...){return fallback;}}
bool jsonType(const std::string& json,const char* type){return json.find(std::string("\"type\":\"")+type+"\"")!=std::string::npos;}
void sendPacket(JNIEnv* env,const std::vector<std::uint8_t>& packet){if(!gBridgeClass||!gSendNetworkPacket||packet.empty())return;jbyteArray bytes=env->NewByteArray(static_cast<jsize>(packet.size()));env->SetByteArrayRegion(bytes,0,static_cast<jsize>(packet.size()),reinterpret_cast<const jbyte*>(packet.data()));env->CallStaticVoidMethod(gBridgeClass,gSendNetworkPacket,bytes);env->DeleteLocalRef(bytes);}

void updateNetwork(JNIEnv* env){
    std::deque<NetworkEvent> events;{std::lock_guard<std::mutex> lock(gNetworkMutex);events.swap(gNetworkEvents);}
    for(auto& event:events){
        if(event.kind==NetworkEvent::Solo){gNetworkConfigured=false;gNetworkSequence=0;gLastSnapshotTick=0;gLastSnapshotSequence=0;gLastInputSequence.fill(0);gGame.restart();}
        else if(event.kind==NetworkEvent::Configure){gGame.restart();gNetworkHost=event.host;gNetworkPlayerId=event.playerId;gNetworkConfigured=true;gNetworkSequence=0;gLastSnapshotTick=0;gLastSnapshotSequence=0;gLastInputSequence.fill(0);if(gNetworkHost)gGame.configureNetworkHost();else gGame.configureNetworkGuest(gNetworkPlayerId);gGame.setNetworkRoom(event.room.c_str(),event.status.c_str(),true);}
        else if(event.kind==NetworkEvent::Control){if(jsonType(event.text,"player_joined")&&gNetworkHost)gGame.setNetworkPeerActive(jsonInt(event.text,"playerId"),true);else if(jsonType(event.text,"player_left"))gGame.setNetworkPeerActive(jsonInt(event.text,"playerId"),false);else if(jsonType(event.text,"host_disconnected"))gGame.setNetworkRoom(nullptr,"HOST RECONNECTING",false);else if(jsonType(event.text,"host_reconnected"))gGame.setNetworkRoom(nullptr,gNetworkHost?"ROOM":"JOINED",true);else if(jsonType(event.text,"match_closed")){gNetworkConfigured=false;gGame.setNetworkRoom("","HOST LEFT",false);}}
        else if(event.kind==NetworkEvent::Binary&&gNetworkConfigured){dbnet::PacketHeader header;if(!dbnet::decodeHeader(event.bytes.data(),event.bytes.size(),header))continue;if(gNetworkHost&&header.type==dbnet::MessageType::Input){if(header.playerId>=gLastInputSequence.size()||header.sequence<=gLastInputSequence[header.playerId])continue;dbnet::NetworkWorldContext packetWorld;dbnet::InputCommand input;if(dbnet::decodeInput(event.bytes.data(),event.bytes.size(),header,packetWorld,input)&&dbnet::compareWorldContext(packetWorld,gWorldContext)==dbnet::WorldContextCompatibility::Compatible){gLastInputSequence[header.playerId]=header.sequence;gGame.setNetworkPeerInput(header.playerId,input.sequence,input.moveX,input.moveZ,input.yaw,input.pitch,input.buttons);}}else if(!gNetworkHost&&header.type==dbnet::MessageType::Snapshot){if(header.sequence<=gLastSnapshotSequence)continue;dbnet::WorldSnapshot snapshot;if(dbnet::decodeSnapshot(event.bytes.data(),event.bytes.size(),header,snapshot)){gWorldContext=snapshot.world;gLastSnapshotSequence=header.sequence;dbnet::applyWorld(gGame.networkMutableState(),snapshot,static_cast<std::uint8_t>(gNetworkPlayerId));}}}
    }
    if(!gNetworkConfigured)return;const GameState& state=gGame.state();
    if(!gNetworkHost&&(state.frame%2==0||state.input.commSignalPressed!=0)){dbnet::InputCommand input;input.sequence=++gNetworkSequence;input.localTick=static_cast<std::uint32_t>(std::max(0,state.frame));input.moveX=clampf((state.input.right?1.0f:0.0f)-(state.input.left?1.0f:0.0f)+state.input.touchMoveX,-1,1);input.moveZ=clampf((state.input.forward?1.0f:0.0f)-(state.input.back?1.0f:0.0f)+state.input.touchMoveZ,-1,1);input.yaw=state.camera.yaw;input.pitch=state.camera.pitch;if(state.input.forward)input.buttons|=dbnet::Forward;if(state.input.back)input.buttons|=dbnet::Back;if(state.input.left)input.buttons|=dbnet::Left;if(state.input.right)input.buttons|=dbnet::Right;if(state.input.sprint||state.input.touchSprint)input.buttons|=dbnet::Sprint;if(state.input.jumpPressed)input.buttons|=dbnet::Jump;if(state.input.primaryHeld||state.input.touchPrimaryHeld)input.buttons|=dbnet::Vacuum;if(state.input.meleePressed)input.buttons|=dbnet::Melee;if(state.input.shootPressed)input.buttons|=dbnet::Shoot;if(state.input.cameraTogglePressed)input.buttons|=dbnet::CameraToggle;if(state.input.wiggleAxis<0)input.buttons|=dbnet::WiggleLeft;else if(state.input.wiggleAxis>0)input.buttons|=dbnet::WiggleRight;if(state.input.commSignalPressed==1)input.buttons|=dbnet::CommHelp;else if(state.input.commSignalPressed==2)input.buttons|=dbnet::CommPing;else if(state.input.commSignalPressed==3)input.buttons|=dbnet::CommGroup;else if(state.input.commSignalPressed==4)input.buttons|=dbnet::CommOk;sendPacket(env,dbnet::encodeInput(static_cast<std::uint8_t>(gNetworkPlayerId),input));}
    else if(gNetworkHost&&static_cast<std::uint32_t>(state.frame)>=gLastSnapshotTick+3){gLastSnapshotTick=static_cast<std::uint32_t>(state.frame);sendPacket(env,dbnet::encodeSnapshot(0,dbnet::captureWorld(state,dbnet::capturePlayers(state),gLastSnapshotTick),++gNetworkSequence));}
}

void resetFrameClock() {
    gLastFrame = std::chrono::steady_clock::now();
    gAccumulatorSeconds = 0.0;
}

void advanceSimulation(JNIEnv* env) {
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - gLastFrame;
    gLastFrame = now;
    gAccumulatorSeconds += std::min(elapsed.count(), MAX_FRAME_SECONDS);

    int steps = 0;
    while (gAccumulatorSeconds >= FIXED_STEP_SECONDS && steps < MAX_STEPS_PER_FRAME) {
        updateNetwork(env);
        gGame.update(static_cast<float>(FIXED_STEP_SECONDS));
        gAccumulatorSeconds -= FIXED_STEP_SECONDS;
        ++steps;
    }

    if (steps == MAX_STEPS_PER_FRAME && gAccumulatorSeconds >= FIXED_STEP_SECONDS) {
        gAccumulatorSeconds = 0.0;
    }
}

} // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onSurfaceCreated(JNIEnv* env, jclass bridgeClass) {
    __android_log_print(
        ANDROID_LOG_INFO,
        "DBNATIVE",
        "surface created native_source=%s",
        DIGITAL_BREAKDOWN_SOURCE_COMMIT
    );
    gGame.prepareAttractScreen();
    gLastAudioSerial=0;
    if(!gBridgeClass) gBridgeClass=static_cast<jclass>(env->NewGlobalRef(bridgeClass));
    if(!gPlayAudioCue) gPlayAudioCue=env->GetStaticMethodID(gBridgeClass,"playAudioCue","(IF)V");
    if(!gSyncMusic) gSyncMusic=env->GetStaticMethodID(gBridgeClass,"syncMusic","(ZZZZFF)V");
    if(!gSendNetworkPacket) gSendNetworkPacket=env->GetStaticMethodID(gBridgeClass,"sendNetworkPacket","([B)V");
    if(!gSaveProgression) gSaveProgression=env->GetStaticMethodID(gBridgeClass,"saveProgression","(JJIII)V");
    gLastProgressionRevision=gGame.state().progression.permanent.revision;
    gRenderer.surfaceCreated();
    resetFrameClock();
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_setAssetRoot(JNIEnv* env, jclass, jstring path) {
    if (!path) return;
    const char* value = env->GetStringUTFChars(path, nullptr);
    if (!value) return;
    gRenderer.setAssetRoot(value);
    env->ReleaseStringUTFChars(path, value);
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onSurfaceChanged(JNIEnv*, jclass, jint width, jint height) {
    __android_log_print(ANDROID_LOG_INFO, "DBNATIVE", "surface changed %d x %d", width, height);
    gRenderer.surfaceChanged(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onDrawFrame(JNIEnv* env, jclass) {
    advanceSimulation(env);
    gRenderer.draw(gGame.state());

    const GameState& s = gGame.state();
    const auto& permanent=s.progression.permanent;
    if(gBridgeClass&&gSaveProgression&&permanent.revision!=gLastProgressionRevision){env->CallStaticVoidMethod(gBridgeClass,gSaveProgression,static_cast<jlong>(permanent.revision),static_cast<jlong>(permanent.tokens),permanent.levels[0],permanent.levels[1],permanent.levels[2]);gLastProgressionRevision=permanent.revision;}
    if(gBridgeClass&&gSyncMusic){const Vec3 tv{41.82f,0.78f,0};float proximity=s.player.inSecretRoom?1.0f-clampf(length(s.player.pos-tv)/6.0f,0,1):0;if(s.multiplayer.enabled)for(const auto& peer:s.multiplayer.peers)if(peer.active&&peer.player.inSecretRoom)proximity=std::max(proximity,1.0f-clampf(length(peer.player.pos-tv)/6.0f,0,1));env->CallStaticVoidMethod(gBridgeClass,gSyncMusic,(s.started&&!s.attractMode)?JNI_TRUE:JNI_FALSE,s.dead?JNI_TRUE:JNI_FALSE,(s.uiPaused||s.upgradeMenu.active)?JNI_TRUE:JNI_FALSE,s.player.inSecretRoom?JNI_TRUE:JNI_FALSE,clampf(s.hud.headshotPulse+s.hud.perfectPulse*0.22f,0.0f,1.0f),proximity);}
    if(gBridgeClass && gPlayAudioCue) {
        const unsigned int newest=s.audio.nextSerial>0?s.audio.nextSerial-1:0;
        const unsigned int first=std::max(gLastAudioSerial+1,newest>=AUDIO_EVENT_COUNT?newest-AUDIO_EVENT_COUNT+1:1u);
        for(unsigned int serial=first;serial<=newest;++serial){const AudioEventState& event=s.audio.events[(serial-1u)%AUDIO_EVENT_COUNT];if(event.serial==serial)env->CallStaticVoidMethod(gBridgeClass,gPlayAudioCue,static_cast<jint>(event.cue),event.volume);}
        gLastAudioSerial=newest;
    }
    if ((s.frame % 180) == 0) {
        __android_log_print(
            ANDROID_LOG_INFO,
            "DBNATIVE",
            "frame=%d room=%d pos=%.2f,%.2f,%.2f souls=%d battery=%.0f target=%d",
            s.frame,
            s.roomIndex,
            s.player.pos.x,
            s.player.pos.y,
            s.player.pos.z,
            s.player.souls,
            s.player.battery,
            s.vacuum.target
        );
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_restart(JNIEnv*, jclass) {
    gGame.restart();
    resetFrameClock();
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onTouch(
    JNIEnv*,
    jclass,
    jint action,
    jfloat x,
    jfloat y,
    jint pointerCount
) {
    gGame.setTouch(action, x, y, pointerCount);
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onTouchControls(
    JNIEnv*,
    jclass,
    jfloat moveX,
    jfloat moveZ,
    jfloat lookDeltaX,
    jfloat lookDeltaY,
    jboolean vacuumHeld,
    jboolean sprintHeld,
    jboolean jumpPressed,
    jboolean meleePressed,
    jboolean shootPressed,
    jboolean cameraTogglePressed
) {
    gGame.setTouchControls(
        moveX,
        moveZ,
        lookDeltaX,
        lookDeltaY,
        vacuumHeld == JNI_TRUE,
        sprintHeld == JNI_TRUE,
        jumpPressed == JNI_TRUE,
        meleePressed == JNI_TRUE,
        shootPressed == JNI_TRUE,
        cameraTogglePressed == JNI_TRUE
    );
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onCommSignal(
    JNIEnv*,
    jclass,
    jint signal
) {
    gGame.setCommSignal(static_cast<int>(signal));
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onKey(JNIEnv*, jclass, jint keyCode, jboolean down) {
    gGame.setKey(keyCode, down == JNI_TRUE);
}
extern "C" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_onWiggle(JNIEnv*,jclass,jfloat axis){gGame.setWiggle(axis);}
extern "C" JNIEXPORT jboolean JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_isGrabbed(JNIEnv*,jclass){return gGame.state().player.grabbedByTarget>=0?JNI_TRUE:JNI_FALSE;}

extern "C" JNIEXPORT jint JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_getMenuMode(JNIEnv*,jclass){return gGame.state().upgradeMenu.active?1:(gGame.state().uiPaused?2:0);}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_setPaused(JNIEnv*,jclass,jboolean paused){gGame.setUiPaused(paused==JNI_TRUE);}

extern "C" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_setLocalSettings(JNIEnv*,jclass,jfloat music,jfloat sfx,jboolean musicMuted,jboolean sfxMuted,jint preset,jboolean shadows,jboolean portal,jboolean particles,jboolean fps){auto& settings=gGame.networkMutableState().localSettings;settings.musicVolume=clampf(music,0,1);settings.sfxVolume=clampf(sfx,0,1);settings.musicMuted=musicMuted==JNI_TRUE;settings.sfxMuted=sfxMuted==JNI_TRUE;settings.graphicsPreset=std::max(0,std::min(2,static_cast<int>(preset)));settings.shadows=shadows==JNI_TRUE;settings.portalWindow=portal==JNI_TRUE;settings.particles=particles==JNI_TRUE;settings.fpsCounter=fps==JNI_TRUE;settings.mobileFraming=true;}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_chooseUpgrade(JNIEnv*,jclass,jint track,jboolean permanent){if(permanent)gGame.purchasePermanentUpgrade(track);else gGame.chooseTemporaryUpgrade(track);}

extern "C" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_setPersistentProgression(JNIEnv*,jclass,jlong tokens,jint shot,jint lunge,jint attack){gGame.setPersistentProgression(static_cast<std::int64_t>(tokens),shot,lunge,attack);gLastProgressionRevision=gGame.state().progression.permanent.revision;}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_isIntroActive(JNIEnv*, jclass) {
    return gGame.state().cinematic.introActive ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_isStarted(JNIEnv*, jclass) {
    return gGame.state().started ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_startSolo(JNIEnv*,jclass){NetworkEvent event;event.kind=NetworkEvent::Solo;std::lock_guard<std::mutex> lock(gNetworkMutex);gNetworkEvents.clear();gNetworkEvents.push_back(std::move(event));}
extern "C" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_configureNetwork(JNIEnv* env,jclass,jboolean host,jint playerId,jstring room,jstring status){const char* r=env->GetStringUTFChars(room,nullptr);const char* s=env->GetStringUTFChars(status,nullptr);NetworkEvent event;event.kind=NetworkEvent::Configure;event.host=host==JNI_TRUE;event.playerId=playerId;event.room=r?r:"";event.status=s?s:"";if(r)env->ReleaseStringUTFChars(room,r);if(s)env->ReleaseStringUTFChars(status,s);enqueueNetworkEvent(std::move(event));}
extern "C" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_onNetworkControl(JNIEnv* env,jclass,jstring value){const char* chars=env->GetStringUTFChars(value,nullptr);NetworkEvent event;event.kind=NetworkEvent::Control;event.text=chars?chars:"";if(chars)env->ReleaseStringUTFChars(value,chars);if(event.text.size()>dbnet::MAX_INCOMING_MESSAGE_BYTES){__android_log_print(ANDROID_LOG_WARN,"DBNATIVE","network control rejected bytes=%zu",event.text.size());return;}enqueueNetworkEvent(std::move(event));}
extern "C" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_onNetworkPacket(JNIEnv* env,jclass,jbyteArray value){const jsize size=env->GetArrayLength(value);if(size<0||static_cast<std::size_t>(size)>dbnet::MAX_INCOMING_MESSAGE_BYTES){__android_log_print(ANDROID_LOG_WARN,"DBNATIVE","network packet rejected bytes=%d",static_cast<int>(size));return;}NetworkEvent event;event.kind=NetworkEvent::Binary;event.bytes.resize(static_cast<std::size_t>(size));env->GetByteArrayRegion(value,0,size,reinterpret_cast<jbyte*>(event.bytes.data()));enqueueNetworkEvent(std::move(event));}
