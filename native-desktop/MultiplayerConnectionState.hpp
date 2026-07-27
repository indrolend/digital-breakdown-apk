#pragma once

#include <cctype>
#include <string>

namespace dbmultiplayer {

enum class Phase {
    Offline,
    CreatingRoom,
    JoiningRoom,
    Connecting,
    Lobby,
    Starting,
    Synchronizing,
    Playing,
    Failed,
    HostLeft
};
enum class Event {
    CreateRoom,
    JoinRoom,
    RoomReady,
    Welcome,
    StartRequested,
    StartReceived,
    InitialSnapshot,
    StartConfirmed,
    Failure,
    HostDisconnected,
    Reset
};

inline Phase transition(Phase current, Event event) {
    switch (event) {
    case Event::CreateRoom:
        return current == Phase::Offline || current == Phase::Failed || current == Phase::HostLeft
            ? Phase::CreatingRoom : current;
    case Event::JoinRoom:
        return current == Phase::Offline || current == Phase::Failed || current == Phase::HostLeft
            ? Phase::JoiningRoom : current;
    case Event::RoomReady:
        return current == Phase::CreatingRoom || current == Phase::JoiningRoom
            ? Phase::Connecting : current;
    case Event::Welcome:
        return current == Phase::Connecting ? Phase::Lobby : current;
    case Event::StartRequested:
        return current == Phase::Lobby ? Phase::Starting : current;
    case Event::StartReceived:
        return current == Phase::Lobby || current == Phase::Starting
            ? Phase::Synchronizing : current;
    case Event::InitialSnapshot:
        return current == Phase::Synchronizing ? Phase::Synchronizing : current;
    case Event::StartConfirmed:
        return current == Phase::Synchronizing ? Phase::Playing : current;
    case Event::Failure:
        return current == Phase::Offline || current == Phase::HostLeft ? current : Phase::Failed;
    case Event::HostDisconnected:
        return current == Phase::Lobby || current == Phase::Starting ||
                       current == Phase::Synchronizing || current == Phase::Playing
            ? Phase::HostLeft : current;
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
