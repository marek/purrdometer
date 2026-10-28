// Setup mode's state machine (docs/intents/network/setup.md#endings): starts
// the hotspot when asked or when no WiFi is configured, applies the form,
// falls back to the WiFi there was, and publishes each step for the screen
// and the setup page.
#pragma once

#include "common/file_watch.h"
#include "common/setup_state.h"
#include "setup/login_account.h"
#include "setup/network_manager.h"

#include <ctime>
#include <functional>
#include <string>
#include <vector>

namespace purr::setup {

// The files setup mode reads and writes.
struct SetupPaths {
    std::string state = STATE_PATH, request = REQUEST_PATH, apply = APPLY_PATH, networks = NETWORKS_PATH;
};

class SetupMode {
public:
    static constexpr int JOINED_SHOWN_S = 60;
    static constexpr int RETRY_S = 30; // between looks for a WiFi profile, and between tries at a hotspot

    // What the login part of a form came to: what could not be applied (see applyLogin), empty when all was.
    using LoginApplier = std::function<std::vector<std::string>(const Apply&)>;

    SetupMode(NetworkManager& wifi, int hotspotMinutes, SetupPaths paths = {}, LoginApplier login = applyLogin);

    // After a restart: no hotspot left behind; setup if NetworkManager says there is no WiFi.
    // When it cannot say, nothing starts and tick() asks again.
    void begin(std::time_t now = std::time(nullptr));
    void tick(std::time_t now);     // one look at the requests and the clock

    const State& state() const { return state_; }

private:
    void checkProfiles();
    void start();
    bool openHotspot();             // the hotspot up and published; otherwise the way back, or another try
    void idle();                    // setup is over: nothing running, and what is still owed is asked again
    void apply(const Apply& a);
    void timeOut();
    void publish();

    NetworkManager& wifi_;
    int hotspotMinutes_;
    SetupPaths paths_;
    FileWatch request_, form_;
    LoginApplier login_;
    bool untilJoined_ = false;      // no WiFi to go back to: the hotspot stays up
    bool profilesKnown_ = false;    // NetworkManager has said whether a WiFi profile exists
    std::time_t now_ = 0;
    std::time_t retryAt_ = 0;       // when to ask again or try the hotspot again; 0: not waiting
    std::string joinFailure_;       // why the last join failed, kept until a hotspot shows it
    State state_;
};

} // namespace purr::setup
