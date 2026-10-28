// purrdometer-logger's command line: where its files are.
#pragma once

#include "common/config.h"

#include <optional>
#include <string>

namespace purr::logger {

struct Paths {
    std::string db = "/var/lib/purrdometer/purrdometer.db";
    std::string sensorFile = "/var/lib/purrdometer/sensor";   // the first sensor heard, kept
    std::string config = CONFIG_PATH;
    std::string overrides = OVERRIDES_PATH;
    std::string scanRequest = "/var/lib/purrdometer/scan-request";
};

struct Options {
    Paths paths;
    bool selftest = false;
};

// nullopt after printing the usage.
std::optional<Options> parseOptions(int argc, char** argv);

} // namespace purr::logger
