// Setup mode's state machine (docs/intents/network/setup.md#endings): starts
// the hotspot when asked or when no WiFi is configured, applies the form,
// falls back to the WiFi there was, and publishes each step for the screen
// and the setup page.
#pragma once

#include "common/file_watch.h"
#include "common/setup_state.h"
#include "setup/network_manager.h"

#include <ctime>

namespace purr::setup {

class SetupMode {
public:
    static constexpr int JOINED_SHOWN_S = 60;

    SetupMode(NetworkManager& wifi, int hotspotMinutes, std::string statePath = STATE_PATH,
              std::string requestPath = REQUEST_PATH, std::string applyPath = APPLY_PATH);

    void begin();                   // after a restart: no hotspot left behind; setup if there is no WiFi
    void tick(std::time_t now);     // one look at the requests and the clock

    const State& state() const { return state_; }

private:
    void start();
    void openHotspot(std::time_t now);
    void apply(const Apply& a);
    void timeOut();
    void publish();

    NetworkManager& wifi_;
    int hotspotMinutes_;
    std::string statePath_, applyPath_;
    FileWatch request_, form_;
    bool untilJoined_ = false;      // no WiFi to go back to: the hotspot stays up
    State state_;
};

} // namespace purr::setup
