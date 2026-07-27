#pragma once

#include "BuildIdentity.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class DesktopUpdateService {
public:
    enum class State {
        Idle,
        Checking,
        Current,
        Available,
        Incompatible,
        Failed
    };

    ~DesktopUpdateService();
    void checkForUpdates(const BuildIdentity& current);
    State state() const { return state_.load(); }
    std::string status() const;
    void disconnect();

private:
    std::atomic<State> state_{State::Idle};
    mutable std::mutex mutex_;
    std::thread worker_;
    std::string status_ = "Idle";

    void set(State state, const std::string& status);
    void workerMain(BuildIdentity current);
    static std::string stateLabel(State state);
};
