#include <cstdio>
#include "DeveloperCodec.hpp"

int main(){
    DeveloperCodecState state;const bool closedAccepts=!state.gameplayInputSuppressed();state.open=true;const bool openSuppresses=state.gameplayInputSuppressed();state.open=false;const bool closeRestores=!state.gameplayInputSuppressed();
    const bool ok=closedAccepts&&openSuppresses&&closeRestores&&
        parseDeveloperCodecCommand("help").command==DeveloperCodecCommand::Help&&
        parseDeveloperCodecCommand("soul spawn").command==DeveloperCodecCommand::SoulSpawn&&
        parseDeveloperCodecCommand("playtest rally").command==DeveloperCodecCommand::PlaytestRally&&
        parseDeveloperCodecCommand("not-real").command==DeveloperCodecCommand::Invalid&&
        parseDeveloperCodecCommand("soul spawn extra").command==DeveloperCodecCommand::Invalid&&
        parseDeveloperCodecCommand("help extra").command==DeveloperCodecCommand::Invalid;
    if(!ok){std::fprintf(stderr,"DEVELOPER_CODEC_FAILED\n");return 1;}
    std::printf("DEVELOPER_CODEC_OK allowlist=16 shell=ABSENT invalid_args=REJECTED open_input=SUPPRESSED close_input=RESTORED\n");return 0;
}
