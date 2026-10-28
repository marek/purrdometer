// Setup mode's state as the helper publishes it, and the networks it scanned:
// what the screen and the setup page show (docs/intents/network/setup.md).
#pragma once

#include "common/paths.h"

#include <ctime>
#include <string>
#include <vector>

namespace purr::setup {

constexpr const char* STATE_PATH = SETUP_STATE_PATH;
constexpr const char* NETWORKS_PATH = SETUP_NETWORKS_PATH;
constexpr const char* REQUEST_PATH = SETUP_REQUEST_PATH; // touched to start setup
constexpr const char* HOTSPOT_ADDRESS = "10.42.0.1";
constexpr const char* HOTSPOT_PREFIX = "10.42.0.";

enum class Phase { Off, Starting, Hotspot, Joining, Joined };
const char* phaseName(Phase p);
Phase phaseFrom(const std::string& s);

struct State {
    Phase phase = Phase::Off;
    std::string ssid{}, password{}; // the hotspot
    std::string network{};          // the WiFi being joined, or joined
    std::string message{};          // why a join failed, or the address the Pi got
    std::time_t deadline = 0;       // when an unused hotspot stops; 0: not until setup is done
    std::time_t until = 0;          // how long "joined" is shown

    bool active() const { return phase != Phase::Off; }
};

struct Network {
    std::string ssid;
    int signal = 0;                 // 0-100, as NetworkManager reports it
    std::string security;           // empty for an open network
};

State readState(const std::string& path = STATE_PATH);
void writeState(const State& s, const std::string& path = STATE_PATH);

// Asks the helper to start setup (it watches the file's modification time).
void requestSetup(const std::string& path = REQUEST_PATH);

std::vector<Network> readNetworks(const std::string& path = NETWORKS_PATH);
void writeNetworks(const std::vector<Network>& n, const std::string& path = NETWORKS_PATH);

} // namespace purr::setup
