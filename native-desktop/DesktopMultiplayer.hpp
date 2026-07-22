#pragma once

#include "Game.hpp"
#include "MultiplayerProtocol.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class DesktopMultiplayer {
public:
    enum class Role { Offline, Host, Guest };
    DesktopMultiplayer() = default;
    ~DesktopMultiplayer();
    void host(const std::string& serviceUrl);
    void join(const std::string& serviceUrl, const std::string& roomCode);
    void disconnect();
    void update(Game& game);
    Role role() const { return role_.load(); }
    bool connected() const { return connected_.load(); }
    std::string roomCode() const;
    std::string status() const;
private:
    struct Incoming { bool binary=false;std::vector<std::uint8_t> bytes;std::string text; };
    std::atomic<Role> role_{Role::Offline};
    std::atomic<bool> stop_{false},connected_{false};
    std::atomic<int> playerId_{0};
    std::thread worker_;
    mutable std::mutex stateMutex_,queueMutex_,sendMutex_;
    std::string roomCode_,status_="OFFLINE",serviceUrl_,hostKey_;
    std::deque<Incoming> incoming_;
    void* webSocket_=nullptr;
    std::uint32_t outgoingSequence_=0,lastSnapshotTick_=0;
    std::uint32_t lastSnapshotSequence_=0;
    std::array<std::uint32_t, NETWORK_PLAYER_COUNT> lastInputSequence_{};
    bool configuredGame_=false;
    void begin(Role role,const std::string& service,const std::string& code);
    void workerMain();
    bool createRoom();
    bool connectWebSocket();
    void receiveLoop();
    bool sendBinary(const std::vector<std::uint8_t>& packet);
    void setStatus(const std::string& value);
    static std::string jsonString(const std::string& json,const char* key);
    static int jsonInt(const std::string& json,const char* key,int fallback=-1);
};
