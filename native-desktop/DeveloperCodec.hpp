#pragma once

#include <array>
#include <sstream>
#include <string>
#include <vector>

enum class DeveloperCodecCommand : unsigned char {
    Invalid, Help, State, SoulSpawn, BatteryFull,
    EnemiesOn, EnemiesOff, EnemiesToggle,
    RoomNext, RoomReroll,
    CollidersShow, CollidersHide, CollidersToggle,
    InspectHuman, InspectClear,
    MotorRelentless, MotorExisting, ZombieDebugOn, ZombieDebugOff,
    PlaytestZombie, PlaytestRally, PlaytestTraversal, PlaytestRooms, PlaytestCart
};

struct DeveloperCodecParseResult {
    DeveloperCodecCommand command=DeveloperCodecCommand::Invalid;
    bool recognizedVerb=false;
};

inline DeveloperCodecParseResult parseDeveloperCodecCommand(const std::string& input){
    std::istringstream stream(input);std::vector<std::string> words;std::string word;
    while(stream>>word)words.push_back(word);
    if(words.empty())return {};
    const auto exact=[&](const char* a,const char* b=nullptr){return words.size()==(b?2u:1u)&&words[0]==a&&(!b||words[1]==b);};
    if(exact("help"))return {DeveloperCodecCommand::Help,true};
    if(exact("state"))return {DeveloperCodecCommand::State,true};
    if(exact("soul","spawn"))return {DeveloperCodecCommand::SoulSpawn,true};
    if(exact("battery","full"))return {DeveloperCodecCommand::BatteryFull,true};
    if(exact("enemies","on"))return {DeveloperCodecCommand::EnemiesOn,true};
    if(exact("enemies","off"))return {DeveloperCodecCommand::EnemiesOff,true};
    if(exact("enemies","toggle"))return {DeveloperCodecCommand::EnemiesToggle,true};
    if(exact("room","next"))return {DeveloperCodecCommand::RoomNext,true};
    if(exact("room","reroll"))return {DeveloperCodecCommand::RoomReroll,true};
    if(exact("colliders","show"))return {DeveloperCodecCommand::CollidersShow,true};
    if(exact("colliders","hide"))return {DeveloperCodecCommand::CollidersHide,true};
    if(exact("colliders","toggle"))return {DeveloperCodecCommand::CollidersToggle,true};
    if(exact("inspect","human"))return {DeveloperCodecCommand::InspectHuman,true};
    if(exact("inspect","clear"))return {DeveloperCodecCommand::InspectClear,true};
    if(exact("motor","relentless"))return {DeveloperCodecCommand::MotorRelentless,true};
    if(exact("motor","existing"))return {DeveloperCodecCommand::MotorExisting,true};
    if(words.size()==3&&words[0]=="zombie"&&words[1]=="debug"&&words[2]=="on")return {DeveloperCodecCommand::ZombieDebugOn,true};
    if(words.size()==3&&words[0]=="zombie"&&words[1]=="debug"&&words[2]=="off")return {DeveloperCodecCommand::ZombieDebugOff,true};
    if(exact("playtest","zombie"))return {DeveloperCodecCommand::PlaytestZombie,true};
    if(exact("playtest","rally"))return {DeveloperCodecCommand::PlaytestRally,true};
    if(exact("playtest","traversal"))return {DeveloperCodecCommand::PlaytestTraversal,true};
    if(exact("playtest","rooms"))return {DeveloperCodecCommand::PlaytestRooms,true};
    if(exact("playtest","cart"))return {DeveloperCodecCommand::PlaytestCart,true};
    const bool known=words[0]=="help"||words[0]=="state"||words[0]=="soul"||words[0]=="battery"||words[0]=="enemies"||words[0]=="room"||words[0]=="colliders"||words[0]=="inspect"||words[0]=="motor"||words[0]=="zombie"||words[0]=="playtest";
    return {DeveloperCodecCommand::Invalid,known};
}

struct DeveloperCodecState {
    static constexpr int LineCount=9;
    bool open=false;
    bool showColliders=false;
    bool showZombieDiagnostics=false;
    int inspectedTarget=-1;
    std::string input;
    std::array<std::string,LineCount> output{};
    std::array<std::string,LineCount> history{};
    int outputCount=0;
    int historyCount=0;
    int historyCursor=0;

    constexpr bool gameplayInputSuppressed() const noexcept{return open;}

    void write(const std::string& line){
        if(outputCount<LineCount)output[outputCount++]=line;
        else{for(int i=1;i<LineCount;++i)output[i-1]=output[i];output.back()=line;}
    }
    void remember(const std::string& line){
        if(line.empty())return;
        if(historyCount<LineCount)history[historyCount++]=line;
        else{for(int i=1;i<LineCount;++i)history[i-1]=history[i];history.back()=line;}
        historyCursor=historyCount;
    }
};
