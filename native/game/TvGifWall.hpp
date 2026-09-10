#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

class TvGifWall {
public:
    static constexpr int Columns=12;
    static constexpr int Rows=8;
    static constexpr float ChannelSeconds=7.0f;
    struct Color { float r,g,b; };

    void load(const std::filesystem::path& root) {
        clips_.clear();
        for(int index=1;index<=15;++index){
            char name[20]{};std::snprintf(name,sizeof(name),"tv%02d.dbgif",index);
            std::ifstream input(root/name,std::ios::binary);if(!input)continue;
            char magic[4]{};std::uint16_t width=0,height=0,count=0;
            input.read(magic,4);read(input,width);read(input,height);read(input,count);
            if(std::string(magic,4)!="DBGF"||width!=Columns||height!=Rows||count==0||count>240)continue;
            Clip clip;clip.frames.reserve(count);
            for(std::uint16_t frame=0;frame<count&&input;++frame){Frame value;read(input,value.delayMs);value.pixels.resize(width*height);input.read(reinterpret_cast<char*>(value.pixels.data()),static_cast<std::streamsize>(value.pixels.size()*sizeof(std::uint16_t)));if(input){value.delayMs=std::max<std::uint16_t>(20,value.delayMs);clip.durationMs+=value.delayMs;clip.frames.push_back(std::move(value));}}
            if(!clip.frames.empty())clips_.push_back(std::move(clip));
        }
    }
    bool available() const { return !clips_.empty(); }
    Color sample(int x,int y,float time,int signal) const {
        if(clips_.empty())return {0.12f,0.28f,0.32f};
        const std::size_t channel=(static_cast<std::size_t>(std::max(0,static_cast<int>(time/ChannelSeconds)))+static_cast<std::size_t>(std::max(0,signal))*3u)%clips_.size();
        const Clip& clip=clips_[channel];const std::uint32_t clock=clip.durationMs?static_cast<std::uint32_t>(time*1000.0f)%clip.durationMs:0;std::uint32_t elapsed=0;const Frame* selected=&clip.frames.back();for(const Frame& frame:clip.frames){elapsed+=frame.delayMs;if(clock<elapsed){selected=&frame;break;}}
        const std::uint16_t pixel=selected->pixels[static_cast<std::size_t>(Rows-1-y)*Columns+x];
        return {((pixel>>11)&31)/31.0f,((pixel>>5)&63)/63.0f,(pixel&31)/31.0f};
    }
private:
    struct Frame{std::uint16_t delayMs=100;std::vector<std::uint16_t> pixels;};
    struct Clip{std::uint32_t durationMs=0;std::vector<Frame> frames;};
    template<typename T>static void read(std::ifstream& input,T& value){input.read(reinterpret_cast<char*>(&value),sizeof(value));}
    std::vector<Clip> clips_;
};
