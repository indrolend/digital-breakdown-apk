#include "DesktopAudio.hpp"

#include <algorithm>
#include <array>
#include <string>
#include "miniaudio.h"

namespace {
const char* cueFile(AudioCue cue) {
    constexpr std::array<const char*,18> names={{
        "vc_ended.mp3","vc_invitation.mp3","connect_power.mp3","low_power.mp3","negative_ack.mp3",
        "received_message.mp3","sent_message.mp3","phone_attack.mp3","payment_success.mp3","payment_failure.mp3",
        "end_call_tone.mp3","slurp_ringtone.mp3","slurp_ringtone.mp3","capture_1.mp3","capture_2.mp3",
        "capture_3.mp3","capture_4.mp3","capture_5.mp3"
    }};
    const int index=static_cast<int>(cue);
    return index>=0&&index<static_cast<int>(names.size())?names[index]:nullptr;
}
}

struct DesktopAudio::Impl {
    struct Voice { ma_sound sound{}; bool initialized=false; };
    ma_engine engine{};
    bool initialized=false;
    std::array<Voice,16> voices{};
    Voice slurp;
    Voice music;
    Voice gameOver;
    bool musicActive=false;
    bool deadPrevious=false;
};

DesktopAudio::DesktopAudio():impl_(std::make_unique<Impl>()) {
    impl_->initialized=ma_engine_init(nullptr,&impl_->engine)==MA_SUCCESS;
}
DesktopAudio::~DesktopAudio(){stopAll();if(impl_&&impl_->initialized)ma_engine_uninit(&impl_->engine);}

namespace {
void stopVoice(DesktopAudio::Impl::Voice& voice){if(!voice.initialized)return;ma_sound_stop(&voice.sound);ma_sound_uninit(&voice.sound);voice.initialized=false;}
bool startVoice(DesktopAudio::Impl& impl,DesktopAudio::Impl::Voice& voice,const std::filesystem::path& path,float volume,bool loop){
    stopVoice(voice);
    if(!impl.initialized||!std::filesystem::exists(path))return false;
    const std::string utf8=path.u8string();
    if(ma_sound_init_from_file(&impl.engine,utf8.c_str(),0,nullptr,nullptr,&voice.sound)!=MA_SUCCESS)return false;
    voice.initialized=true;ma_sound_set_volume(&voice.sound,volume);ma_sound_set_looping(&voice.sound,loop?MA_TRUE:MA_FALSE);ma_sound_start(&voice.sound);return true;
}
}

void DesktopAudio::play(const AudioEventState& event) {
    if(!impl_||!impl_->initialized)return;
    if(event.cue==AudioCue::SlurpRingtoneStop){stopVoice(impl_->slurp);slurpPlaying_=false;return;}
    const char* filename=cueFile(event.cue);if(!filename||root_.empty())return;
    if(event.cue==AudioCue::SlurpRingtoneStart){startVoice(*impl_,impl_->slurp,root_/filename,event.volume,true);slurpPlaying_=true;return;}
    auto& voice=impl_->voices[nextVoice_++%impl_->voices.size()];
    startVoice(*impl_,voice,root_/filename,event.volume,false);
}

void DesktopAudio::update(const GameState& state) {
    if(impl_&&impl_->initialized&&!root_.empty()){
        const bool shouldPlayMusic=state.started&&!state.dead;
        if(shouldPlayMusic&&!impl_->musicActive){stopVoice(impl_->gameOver);startVoice(*impl_,impl_->music,root_/"game_music.mp3",0.52f,true);impl_->musicActive=true;}
        else if(!shouldPlayMusic&&impl_->musicActive){stopVoice(impl_->music);impl_->musicActive=false;}
        if(state.dead&&!impl_->deadPrevious){stopVoice(impl_->music);impl_->musicActive=false;startVoice(*impl_,impl_->gameOver,root_/"game_over.mp3",0.62f,false);}
        else if(!state.dead&&impl_->deadPrevious)stopVoice(impl_->gameOver);
        impl_->deadPrevious=state.dead;
    }
    const unsigned int newest=state.audio.nextSerial>0?state.audio.nextSerial-1:0;
    const unsigned int first=std::max(lastSerial_+1,newest>=AUDIO_EVENT_COUNT?newest-AUDIO_EVENT_COUNT+1:1u);
    for(unsigned int serial=first;serial<=newest;++serial){const AudioEventState& event=state.audio.events[(serial-1u)%AUDIO_EVENT_COUNT];if(event.serial==serial)play(event);}
    lastSerial_=newest;
}

void DesktopAudio::stopAll() {
    if(!impl_)return;
    for(auto& voice:impl_->voices)stopVoice(voice);
    stopVoice(impl_->slurp);stopVoice(impl_->music);stopVoice(impl_->gameOver);
    impl_->musicActive=false;slurpPlaying_=false;
}
