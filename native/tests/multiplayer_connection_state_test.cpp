#include "MultiplayerConnectionState.hpp"

#include <cassert>
#include <iostream>

int main() {
    using dbmultiplayer::Event;
    using dbmultiplayer::Phase;

    Phase phase = Phase::Offline;
    phase = dbmultiplayer::transition(phase, Event::CreateRoom);
    assert(phase == Phase::CreatingRoom);
    phase = dbmultiplayer::transition(phase, Event::RoomReady);
    assert(phase == Phase::Connecting);
    phase = dbmultiplayer::transition(phase, Event::Welcome);
    assert(phase == Phase::Lobby);
    assert(dbmultiplayer::transition(phase, Event::Welcome) == Phase::Lobby);
    phase = dbmultiplayer::transition(phase, Event::StartRequested);
    assert(phase == Phase::Starting);
    assert(dbmultiplayer::transition(phase, Event::StartRequested) == Phase::Starting);
    phase = dbmultiplayer::transition(phase, Event::StartReceived);
    assert(phase == Phase::Synchronizing);
    assert(dbmultiplayer::transition(phase, Event::InitialSnapshot) == Phase::Synchronizing);
    phase = dbmultiplayer::transition(phase, Event::StartConfirmed);
    assert(phase == Phase::Playing);

    phase = dbmultiplayer::transition(Phase::Connecting, Event::Failure);
    assert(phase == Phase::Failed);
    phase = dbmultiplayer::transition(phase, Event::JoinRoom);
    assert(phase == Phase::JoiningRoom);
    phase = dbmultiplayer::transition(phase, Event::RoomReady);
    assert(phase == Phase::Connecting);
    phase = dbmultiplayer::transition(phase, Event::Welcome);
    assert(phase == Phase::Lobby);
    phase = dbmultiplayer::transition(phase, Event::HostDisconnected);
    assert(phase == Phase::HostLeft);
    assert(dbmultiplayer::transition(phase, Event::Reset) == Phase::Offline);

    assert(dbmultiplayer::normalizeRoomCode(" abcd-23 ") == "ABCD23");
    assert(dbmultiplayer::normalizeRoomCode("abcd01").empty());
    assert(dbmultiplayer::normalizeRoomCode("TOOLNG7").empty());
    assert(dbmultiplayer::normalizeRoomCode("SHORT").empty());

    std::cout << "Multiplayer connection state tests passed\n";
    return 0;
}
