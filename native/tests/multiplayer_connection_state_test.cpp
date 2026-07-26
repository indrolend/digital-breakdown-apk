#include "MultiplayerConnectionState.hpp"

#include <cassert>
#include <iostream>

int main() {
    using dbmultiplayer::Event;
    using dbmultiplayer::Phase;

    Phase phase = Phase::Offline;
    phase = dbmultiplayer::transition(phase, Event::Begin);
    assert(phase == Phase::Connecting);
    phase = dbmultiplayer::transition(phase, Event::Welcome);
    assert(phase == Phase::Connected);

    phase = dbmultiplayer::transition(Phase::Connecting, Event::Failure);
    assert(phase == Phase::Failed);
    phase = dbmultiplayer::transition(phase, Event::Begin);
    assert(phase == Phase::Connecting);
    phase = dbmultiplayer::transition(phase, Event::Welcome);
    assert(phase == Phase::Connected);
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
