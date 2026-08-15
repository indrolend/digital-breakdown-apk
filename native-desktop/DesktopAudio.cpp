#include "DesktopAudio.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <string>
#include "miniaudio.h"

namespace {
const char* cueFile(AudioCue cue) {
    constexpr std::array<const char*,22> names={{
        "vc_ended.mp3","vc_invitation.mp3","connect_power.mp3","low_power.mp3","negative_ack.mp3",
        "received_message.mp3","sent_message.mp3","phone_attack.mp3","payment_success.mp3","payment_failure.mp3",
        "end_call_tone.mp3","slurp_ringtone.mp3","slurp_ringtone.mp3","capture_1.mp3","capture_2.mp3",
        "capture_3.mp3","capture_4.mp3","capture_5.mp3","headshot.mp3","headshot_critical.mp3",
        "reward_woah.mp3","reward_nice.mp3"
    }};
    const int index=static_cast<int>(cue);
    return index>=0&&index<static_cast<int>(names.size())?names[index]:nullptr;
}
constexpr ma_uint32 LONG_LOOP_FLAGS=MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_ASYNC|MA_SOUND_FLAG_NO_SPATIALIZATION;
constexpr ma_uint32 LONG_ONESHOT_FLAGS=MA_SOUND_FLAG_DECODE|MA_SOUND_FLAG_ASYNC|MA_SOUND_FLAG_NO_SPATIALIZATION;
}

struct DesktopAudio::Impl {
    struct Voice { ma_sound sound{}; bool initialized=false; };
    ma_engine engine{};
    bool initialized=false;
    std::array<Voice,16> voices{};
    Voice slurp;
    Voice music;
    Voice menuMusic;
    Voice tvRoomPad;
    Voice secretKnock;
    Voice gameOver;
    bool musicActive=false;
    bool menuMusicActive=false;
    bool secretKnockActive=false;
    float menuCuePulse=0.0f;
    float menuCueBend=0.0f;
    float tvRoomMix=0.0f;
    float rewardDuck=0.0f;
    bool deadPrevious=false;
    ma_lpf_node menuFilter{};
    bool menuFilterInitialized=false;
    double menuCutoff=20000.0;
    std::array<std::vector<float>,2> uiSamples;
    std::array<ma_audio_buffer,2> uiBuffers{};
    std::array<Voice,2> uiVoices{};
    std::array<bool,2> uiBufferInitialized{{false,false}};
    unsigned int uiVariation=0;
    std::vector<std::string> registeredCueFiles;
};

namespace {
std::vector<float> makeMenuPluck(float frequency) {
    constexpr int rate=22050,frames=2867,reflection=640,bottomOut=154;
    std::vector<float> result(frames,0.0f);
    unsigned int noise=0x53A91u;
    for(int i=0;i<frames;++i){const float t=static_cast<float>(i)/rate,phase=t*frequency;noise=noise*1664525u+1013904223u;const float grain=static_cast<float>((noise>>24)&255)/127.5f-1.0f;const float contact=grain*std::exp(-t*125.0f),body=(std::sin(phase*6.2831853f)*0.62f+std::sin(phase*12.5663706f)*0.16f)*std::exp(-t*34.0f);float dry=contact*0.24f+body;if(i>=bottomOut){const float bt=static_cast<float>(i-bottomOut)/rate;dry+=result[i-bottomOut]*0.18f*std::exp(-bt*70.0f);}dry=std::round(std::tanh(dry*1.18f)*48.0f)/48.0f;result[i]=dry*0.18f;if(i>=reflection)result[i]+=result[i-reflection]*0.13f;}
    return result;
}
}

DesktopAudio::DesktopAudio():impl_(std::make_unique<Impl>()) {
    impl_->initialized=ma_engine_init(nullptr,&impl_->engine)==MA_SUCCESS;
    if(impl_->initialized){const auto config=ma_lpf_node_config_init(ma_engine_get_channels(&impl_->engine),ma_engine_get_sample_rate(&impl_->engine),impl_->menuCutoff,2);impl_->menuFilterInitialized=ma_lpf_node_init(ma_engine_get_node_graph(&impl_->engine),&config,nullptr,&impl_->menuFilter)==MA_SUCCESS;if(impl_->menuFilterInitialized)ma_node_attach_output_bus(&impl_->menuFilter,0,ma_engine_get_endpoint(&impl_->engine),0);for(int i=0;i<2;++i){impl_->uiSamples[i]=makeMenuPluck(i?293.66f:220.0f);const auto bufferConfig=ma_audio_buffer_config_init(ma_format_f32,1,impl_->uiSamples[i].size(),impl_->uiSamples[i].data(),nullptr);impl_->uiBufferInitialized[i]=ma_audio_buffer_init(&bufferConfig,&impl_->uiBuffers[i])==MA_SUCCESS;if(impl_->uiBufferInitialized[i]&&ma_sound_init_from_data_source(&impl_->engine,&impl_->uiBuffers[i],0,nullptr,&impl_->uiVoices[i].sound)==MA_SUCCESS)impl_->uiVoices[i].initialized=true;}}
}
DesktopAudio::~DesktopAudio(){stopAll();if(impl_)for(int i=0;i<2;++i)if(impl_->uiBufferInitialized[i])ma_audio_buffer_uninit(&impl_->uiBuffers[i]);if(impl_&&impl_->menuFilterInitialized)ma_lpf_node_uninit(&impl_->menuFilter,nullptr);if(impl_&&impl_->initialized){ma_resource_manager* resources=ma_engine_get_resource_manager(&impl_->engine);for(const auto& path:impl_->registeredCueFiles)ma_resource_manager_unregister_file(resources,path.c_str());ma_engine_uninit(&impl_->engine);}}

void DesktopAudio::setAssetRoot(const std::filesystem::path& root) {
    root_=root;
    if(!impl_||!impl_->initialized)return;
    ma_resource_manager* resources=ma_engine_get_resource_manager(&impl_->engine);
    for(const auto& path:impl_->registeredCueFiles)ma_resource_manager_unregister_file(resources,path.c_str());
    impl_->registeredCueFiles.clear();
    for(int index=0;index<22;++index){const char* filename=cueFile(static_cast<AudioCue>(index));if(!filename)continue;const std::string path=(root_/filename).u8string();if(!std::filesystem::exists(path)||std::find(impl_->registeredCueFiles.begin(),impl_->registeredCueFiles.end(),path)!=impl_->registeredCueFiles.end())continue;if(ma_resource_manager_register_file(resources,path.c_str(),MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE)==MA_SUCCESS)impl_->registeredCueFiles.push_back(path);}
    const auto loadPersistent=[&](Impl::Voice& voice,const char* filename,ma_uint32 flags){if(voice.initialized){ma_sound_stop(&voice.sound);ma_sound_uninit(&voice.sound);voice.initialized=false;}const std::string path=(root_/filename).u8string();if(std::filesystem::exists(path)&&ma_sound_init_from_file(&impl_->engine,path.c_str(),flags,nullptr,nullptr,&voice.sound)==MA_SUCCESS)voice.initialized=true;};
    loadPersistent(impl_->menuMusic,"menu_music.mp3",LONG_LOOP_FLAGS);
    loadPersistent(impl_->music,"game_music.mp3",LONG_LOOP_FLAGS);
    loadPersistent(impl_->tvRoomPad,"tv_room_pad.mp3",LONG_LOOP_FLAGS);
    loadPersistent(impl_->secretKnock,"door_knock.mp3",LONG_ONESHOT_FLAGS);
    loadPersistent(impl_->gameOver,"game_over.mp3",LONG_ONESHOT_FLAGS);
}

namespace {
void stopVoice(DesktopAudio::Impl::Voice& voice){if(!voice.initialized)return;ma_sound_stop(&voice.sound);ma_sound_uninit(&voice.sound);voice.initialized=false;}
void pauseVoice(DesktopAudio::Impl::Voice& voice){if(voice.initialized)ma_sound_stop(&voice.sound);}
void playLoadedVoice(DesktopAudio::Impl::Voice& voice,float volume,bool loop){if(!voice.initialized)return;ma_sound_stop(&voice.sound);ma_sound_seek_to_pcm_frame(&voice.sound,0);ma_sound_set_volume(&voice.sound,volume);ma_sound_set_looping(&voice.sound,loop?MA_TRUE:MA_FALSE);ma_sound_start(&voice.sound);}
bool startVoice(DesktopAudio::Impl& impl,DesktopAudio::Impl::Voice& voice,const std::filesystem::path& path,float volume,bool loop,ma_uint32 flags=0){
    stopVoice(voice);
    if(!impl.initialized||!std::filesystem::exists(path))return false;
    const std::string utf8=path.u8string();
    if(ma_sound_init_from_file(&impl.engine,utf8.c_str(),flags,nullptr,nullptr,&voice.sound)!=MA_SUCCESS)return false;
    voice.initialized=true;ma_sound_set_volume(&voice.sound,volume);ma_sound_set_looping(&voice.sound,loop?MA_TRUE:MA_FALSE);ma_sound_start(&voice.sound);return true;
}
}

void DesktopAudio::play(const AudioEventState& event) {
    if(!impl_||!impl_->initialized)return;
    if(event.cue==AudioCue::SlurpRingtoneStop){stopVoice(impl_->slurp);slurpPlaying_=false;return;}
    if(event.cue==AudioCue::RewardWoah)impl_->rewardDuck=std::max(impl_->rewardDuck,0.18f);else if(event.cue==AudioCue::RewardNice)impl_->rewardDuck=std::max(impl_->rewardDuck,0.09f);
    const char* filename=cueFile(event.cue);if(!filename||root_.empty())return;
    constexpr ma_uint32 cueFlags=MA_SOUND_FLAG_DECODE|MA_SOUND_FLAG_NO_SPATIALIZATION;
    if(event.cue==AudioCue::SlurpRingtoneStart){startVoice(*impl_,impl_->slurp,root_/filename,event.volume*sfxLevel_,true,cueFlags);slurpPlaying_=true;return;}
    auto& voice=impl_->voices[nextVoice_++%impl_->voices.size()];
    startVoice(*impl_,voice,root_/filename,event.volume*sfxLevel_,false,cueFlags);
}

void DesktopAudio::playMenuCue(bool confirm){if(!impl_||!impl_->initialized||sfxLevel_<=0)return;constexpr float ratios[]={0.94f,1.0f,1.035f,0.975f,1.07f,0.92f,1.015f};const unsigned int variation=impl_->uiVariation++;impl_->menuCuePulse=std::max(impl_->menuCuePulse,confirm?0.34f:0.22f);impl_->menuCueBend=confirm?0.010f:-0.006f;for(auto& voice:impl_->uiVoices)if(voice.initialized)ma_sound_stop(&voice.sound);auto& voice=impl_->uiVoices[confirm?1:0];if(!voice.initialized)return;ma_sound_seek_to_pcm_frame(&voice.sound,0);ma_sound_set_pitch(&voice.sound,ratios[(variation+(confirm?2u:0u))%7u]);ma_sound_set_pan(&voice.sound,((static_cast<int>(variation%5u)-2)*0.018f));ma_sound_set_volume(&voice.sound,sfxLevel_*(confirm?0.76f:0.54f)*(0.96f+static_cast<float>(variation%3u)*0.018f));ma_sound_start(&voice.sound);}

void DesktopAudio::update(const GameState& state) {
    sfxLevel_=state.localSettings.sfxMuted?0.0f:clampf(state.localSettings.sfxVolume,0.0f,1.0f);
    const float musicLevel=state.localSettings.musicMuted?0.0f:clampf(state.localSettings.musicVolume,0.0f,1.0f);
    if(impl_&&impl_->initialized&&!root_.empty()){
        // The attract simulation may die and reset repeatedly, but it is still
        // one continuous title presentation.  Its music belongs to the title,
        // not to the disposable demo session.
        const bool shouldPlayMenuMusic=!state.started||state.attractMode;
        if(shouldPlayMenuMusic&&!impl_->menuMusicActive){playLoadedVoice(impl_->menuMusic,0.0f,true);impl_->menuMusicActive=true;}
        else if(!shouldPlayMenuMusic&&impl_->menuMusicActive){pauseVoice(impl_->menuMusic);impl_->menuMusicActive=false;impl_->menuCuePulse=0.0f;impl_->menuCueBend=0.0f;}
        if(impl_->menuMusicActive&&impl_->menuMusic.initialized){impl_->menuCuePulse=std::max(0.0f,impl_->menuCuePulse-0.018f);impl_->menuCueBend*=0.92f;const float pulse=impl_->menuCuePulse,breath=0.5f+0.5f*std::sin(state.time*0.42f);ma_sound_set_volume(&impl_->menuMusic.sound,(0.34f+breath*0.045f+pulse*0.075f)*musicLevel);ma_sound_set_pitch(&impl_->menuMusic.sound,1.0f+std::sin(state.time*0.17f)*0.0025f+impl_->menuCueBend+pulse*0.0035f);}
        const bool shouldPlayMusic=state.started&&!state.attractMode&&!state.dead;
        if(shouldPlayMusic&&!impl_->musicActive){pauseVoice(impl_->gameOver);playLoadedVoice(impl_->music,0.52f,true);playLoadedVoice(impl_->tvRoomPad,0.0f,true);impl_->musicActive=true;}
        else if(!shouldPlayMusic&&impl_->musicActive){pauseVoice(impl_->music);pauseVoice(impl_->tvRoomPad);pauseVoice(impl_->secretKnock);impl_->musicActive=false;impl_->secretKnockActive=false;impl_->tvRoomMix=0.0f;}
        const bool humanGameOver=state.dead&&!state.attractMode;
        if(humanGameOver&&!impl_->deadPrevious){pauseVoice(impl_->music);pauseVoice(impl_->secretKnock);impl_->musicActive=false;impl_->secretKnockActive=false;playLoadedVoice(impl_->gameOver,0.62f*musicLevel,false);}
        else if(!humanGameOver&&impl_->deadPrevious)pauseVoice(impl_->gameOver);
        impl_->deadPrevious=humanGameOver;
        if(impl_->musicActive){impl_->rewardDuck=std::max(0.0f,impl_->rewardDuck-0.006f);const float duck=1.0f-impl_->rewardDuck,tvTarget=state.player.inSecretRoom?1.0f:0.0f;impl_->tvRoomMix+=(tvTarget-impl_->tvRoomMix)*0.035f;impl_->tvRoomMix=clampf(impl_->tvRoomMix,0.0f,1.0f);const float crush=clampf(state.hud.headshotPulse+state.hud.perfectPulse*0.22f,0.0f,1.0f),step=(state.frame%3)==0?1.0f:0.0f;if(impl_->music.initialized){ma_sound_set_volume(&impl_->music.sound,(0.52f-crush*(0.010f+step*0.018f))*musicLevel*(1.0f-impl_->tvRoomMix)*duck);ma_sound_set_pitch(&impl_->music.sound,1.0f-crush*step*0.006f);}if(impl_->tvRoomPad.initialized){const Vec3 tv{41.82f,0.78f,0};float proximity=state.player.inSecretRoom?1.0f-clampf(length(state.player.pos-tv)/6.0f,0.0f,1.0f):0.0f;if(state.multiplayer.enabled)for(const auto& peer:state.multiplayer.peers)if(peer.active&&peer.player.inSecretRoom)proximity=std::max(proximity,1.0f-clampf(length(peer.player.pos-tv)/6.0f,0.0f,1.0f));const float warble=proximity*(-0.012f+std::sin(state.time*2.7f)*0.018f+std::sin(state.time*0.61f)*0.010f);ma_sound_set_volume(&impl_->tvRoomPad.sound,0.48f*musicLevel*impl_->tvRoomMix*duck);ma_sound_set_pitch(&impl_->tvRoomPad.sound,1.0f+std::sin(state.time*0.19f)*0.0025f+warble);}const bool knockReady=state.secretTv.available&&!state.player.inSecretRoom&&state.secretTv.knockVolume>0.0001f;if(knockReady&&!impl_->secretKnockActive){playLoadedVoice(impl_->secretKnock,0.0f,false);impl_->secretKnockActive=true;}else if(!knockReady&&impl_->secretKnockActive){pauseVoice(impl_->secretKnock);impl_->secretKnockActive=false;}if(impl_->secretKnock.initialized){const float knockVolume=state.secretTv.knockVolume*sfxLevel_*0.74f;ma_sound_set_volume(&impl_->secretKnock.sound,knockVolume);ma_sound_set_pan(&impl_->secretKnock.sound,state.secretTv.knockPan);ma_sound_set_pitch(&impl_->secretKnock.sound,0.985f+std::sin(state.time*0.37f)*0.010f+state.secretTv.knockPulse*0.006f);}}
    }
    const unsigned int newest=state.audio.nextSerial>0?state.audio.nextSerial-1:0;
    const unsigned int first=std::max(lastSerial_+1,newest>=AUDIO_EVENT_COUNT?newest-AUDIO_EVENT_COUNT+1:1u);
    for(unsigned int serial=first;serial<=newest;++serial){const AudioEventState& event=state.audio.events[(serial-1u)%AUDIO_EVENT_COUNT];if(event.serial==serial)play(event);}
    lastSerial_=newest;
}

void DesktopAudio::stopAll() {
    if(!impl_)return;
    for(auto& voice:impl_->voices)stopVoice(voice);
    stopVoice(impl_->slurp);stopVoice(impl_->music);stopVoice(impl_->menuMusic);stopVoice(impl_->tvRoomPad);stopVoice(impl_->secretKnock);stopVoice(impl_->gameOver);
    for(auto& voice:impl_->uiVoices)stopVoice(voice);
    impl_->musicActive=false;impl_->menuMusicActive=false;impl_->secretKnockActive=false;impl_->menuCuePulse=0.0f;impl_->menuCueBend=0.0f;impl_->tvRoomMix=0.0f;slurpPlaying_=false;
}
