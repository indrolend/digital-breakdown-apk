#pragma once

#include "Game.hpp"
#include <filesystem>
#include <memory>

class DesktopAudio {
public:
    struct Impl;
    DesktopAudio();
    ~DesktopAudio();
    DesktopAudio(const DesktopAudio&)=delete;
    DesktopAudio& operator=(const DesktopAudio&)=delete;
    void setAssetRoot(const std::filesystem::path& root);
    void update(const GameState& state);
    void playMenuCue(bool confirm);
    void stopAll();
private:
    std::filesystem::path root_;
    unsigned int lastSerial_=0;
    unsigned int nextVoice_=0;
    bool slurpPlaying_=false;
    float sfxLevel_=0.55f;
    float musicLevel_=0.70f;
    std::unique_ptr<Impl> impl_;
    void play(const AudioEventState& event);
};
