#pragma once

#include <cctype>
#include <string>

namespace dbmultiplayer {

enum class Phase { Offline, Connecting, Connected, Failed, HostLeft };
enum class Event { Begin, Welcome, Failure, HostDisconnected, Reset };

inline Phase transition(Phase current, Event event) {
    switch (event) {
    case Event::Begin:
        return current == Phase::Connecting || current == Phase::Connected ? current : Phase::Connecting;
    case Event::Welcome:
        return current == Phase::Connecting ? Phase::Connected : current;
    case Event::Failure:
        return current == Phase::Connecting || current == Phase::Connected ? Phase::Failed : current;
    case Event::HostDisconnected:
        return current == Phase::Connected ? Phase::HostLeft : current;
    case Event::Reset:
        return Phase::Offline;
    }
    return current;
}

inline bool isRoomCharacter(char value) {
    const unsigned char c = static_cast<unsigned char>(value);
    const char upper = static_cast<char>(std::toupper(c));
    return (upper >= 'A' && upper <= 'Z' && upper != 'I' && upper != 'O') ||
           (upper >= '2' && upper <= '9');
}

inline std::string normalizeRoomCode(const std::string& value) {
    std::string result;
    result.reserve(6);
    for (const unsigned char c : value) {
        if (std::isspace(c) || c == '-')
            continue;
        const char upper = static_cast<char>(std::toupper(c));
        if (!isRoomCharacter(upper) || result.size() == 6)
            return {};
        result.push_back(upper);
    }
    return result.size() == 6 ? result : std::string{};
}

} // namespace dbmultiplayer
