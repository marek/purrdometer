// What the setup page shows (docs/intents/network/setup.md#the-setup-page).
#pragma once

#include "common/profiles.h"
#include "common/setup_state.h"
#include "common/wheel.h"

#include <string>
#include <vector>

namespace purr::web {

struct SetupView {
    setup::State state;
    bool allowed = false;                // setup runs and this client is on the hotspot
    std::vector<setup::Network> networks;
    std::string token;
    std::string login;                   // the account whose password is set
    std::string units = DEFAULT_UNITS;
    const WheelPresets* presets = nullptr;
    Profile wheel;                       // the wheel in use
    std::vector<std::string> problems;   // a rejected submission
    bool submitted = false;
};

} // namespace purr::web
