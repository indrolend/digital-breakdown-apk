#pragma once

#include "Game.hpp"
#include "MultiplayerProtocol.hpp"
#include "MultiplayerConnectionState.hpp"

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
    struct MultiplayerMetrics {
        std::uint64_t snapshotsReceived=0,staleSnapshotsRejected=0;
        std::uint64_t eventsReceived=0,duplicateEventsRejected=0,staleEventsRejected=0;
        std::uint64_t predictedActions=0,confirmedActions=0,correctedActions=0,cancelledActions=0;
        std::uint64_t hashMatches=0,hashMismatches=0;
        float maximumPositionCorrection=0,maximumActionPhaseCorrection=0;
    };
    enum class Role { Offline, Host, Guest };
    DesktopMultiplayer() = default;
    ~DesktopMultiplayer();
    void host(const std::string& serviceUrl);
    void join(const std::string& serviceUrl, const std::string& roomCode);
    void disconnect();
    bool startMatch();
    void update(Game& game);
    void applyPresentation(GameState& renderState) const;
    void configureImpairment(int latencyMs,int jitterMs,int dropSnapshotEvery,
                             int dropInputEvery,std::uint32_t seed);
    void printMetrics() const;
    Role role() const { return role_.load(); }
    bool connected() const { return connected_.load(); }
    dbmultiplayer::Phase phase() const { return phase_.load(); }
    int playerCount() const { return playerCount_.load(); }
    bool pending() const {
        const auto p=phase_.load();
        return p==dbmultiplayer::Phase::CreatingRoom||p==dbmultiplayer::Phase::JoiningRoom||
               p==dbmultiplayer::Phase::Connecting||p==dbmultiplayer::Phase::Starting||
               p==dbmultiplayer::Phase::Synchronizing;
    }
    bool failed() const { const auto p=phase_.load();return p==dbmultiplayer::Phase::Failed||p==dbmultiplayer::Phase::HostLeft; }
    std::string roomCode() const;
    std::string status() const;
private:
    struct Incoming { bool binary=false;std::vector<std::uint8_t> bytes;std::string text; };
    std::atomic<Role> role_{Role::Offline};
    std::atomic<dbmultiplayer::Phase> phase_{dbmultiplayer::Phase::Offline};
    std::atomic<bool> stop_{false},connected_{false};
    std::atomic<int> playerId_{0},playerCount_{0};
    std::atomic<std::int64_t> lastValidMessageMs_{0},lastHeartbeatMs_{0},phaseStartedMs_{0};
    std::thread worker_;
    mutable std::mutex stateMutex_,queueMutex_,sendMutex_,handleMutex_;
    std::string roomCode_,status_="OFFLINE",serviceUrl_,hostKey_;
    std::deque<Incoming> incoming_;
    void* webSocket_=nullptr;
    void* session_=nullptr;
    void* connection_=nullptr;
    void* request_=nullptr;
    std::uint32_t outgoingSequence_=0,lastSnapshotTick_=0;
    std::uint32_t localInputTick_=0;
    std::int64_t lastInputSendMs_=0;
    std::uint32_t lastSnapshotSequence_=0;
    std::int64_t lastSnapshotReceiveMs_=0;
    std::uint32_t startId_=0;
    dbnet::NetworkWorldContext worldContext_{};
    std::array<std::uint32_t, NETWORK_PLAYER_COUNT> lastInputSequence_{};
    bool configuredGame_=false;
    bool loggedInput_=false,loggedSnapshot_=false;
    dbnet::SnapshotInterpolator snapshotInterpolator_;
    dbnet::GameplayEventTracker eventTracker_;
    dbnet::WorldSnapshot previousEventWorld_{};
    bool hasPreviousEventWorld_=false;
    std::uint32_t nextEventId_=0;
    int netLatencyMs_=0,netJitterMs_=0,dropSnapshotEvery_=0,dropInputEvery_=0;
    std::uint32_t impairmentSeed_=1,snapshotSendCount_=0,inputSendCount_=0;
    MultiplayerMetrics metrics_{};
    std::uint16_t lastPredictedButtons_=0;
    bool acceptWorldContext(const dbnet::NetworkWorldContext& packet,
                            const dbnet::PacketHeader& header,
                            bool allowNewerRoom);
    void setWorldContext(const dbnet::NetworkWorldContext& world,const char* reason);
    void emitCombatEvents(const dbnet::WorldSnapshot& world);
    void begin(Role role,const std::string& service,const std::string& code);
    void workerMain();
    bool createRoom();
    bool checkServiceCompatibility();
    bool connectWebSocket();
    void receiveLoop();
    bool sendBinary(const std::vector<std::uint8_t>& packet);
    bool sendText(const std::string& message);
    bool acceptWelcome(const std::string& message);
    void fail(const std::string& visibleStatus,const char* stage,unsigned long error=0);
    void publishHandles(void* session,void* connection,void* request,void* socket);
    bool releaseHandles(void* session,void* connection,void* request,void* socket);
    void setStatus(const std::string& value);
    static std::string jsonString(const std::string& json,const char* key);
    static int jsonInt(const std::string& json,const char* key,int fallback=-1);
};
