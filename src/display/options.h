// purrdometer-display's command line.
#pragma once

#include "common/config.h"
#include "common/setup_state.h"

#include <ctime>
#include <optional>
#include <string>

namespace purr::display {

struct Options {
    std::string config = CONFIG_PATH, overrides = OVERRIDES_PATH, profiles = PROFILES_PATH, wheels = WHEELS_PATH;
    std::string db = DB_PATH;
    std::string setupState = setup::STATE_PATH, setupRequest = setup::REQUEST_PATH;
    std::optional<std::string> demoDb, preview;
    std::optional<std::time_t> now;
    bool listWheels = false, checkConfig = false, selftest = false, verbose = false;
};

// nullopt after printing the usage.
std::optional<Options> parseOptions(int argc, char** argv);

} // namespace purr::display
