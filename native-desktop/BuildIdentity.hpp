#pragma once

#include <cstdint>
#include <string>

struct BuildIdentity {
    std::string humanVersion;
    std::string commit;
    std::string commitShort;
    std::uint16_t protocolVersion = 0;
    std::uint16_t gameplayVersion = 0;
    int saveFormatVersion = 0;
    std::string platform;
    std::string architecture;
    std::string buildConfiguration;
    std::string buildTime;
    std::string channel;
};

const BuildIdentity& desktopBuildIdentity();
std::string desktopBuildIdentityLine();
