// purrdometer-logger's command line: where its files are.
#pragma once

#include "common/config.h"

#include <optional>
#include <string>

namespace purr::logger {

struct Paths {
    std::string db = DB_PATH;
    std::string sensorLogDir = SENSOR_LOG_DIR;
    std::string sensorFile = SENSOR_PATH; // the first sensor heard, kept
    std::string config = CONFIG_PATH;
    std::string overrides = OVERRIDES_PATH;
    std::string profiles = PROFILES_PATH;
    std::string scanRequest = SCAN_REQUEST_PATH;
};

struct Options {
    Paths paths;
    bool selftest = false;
};

// nullopt after printing the usage.
std::optional<Options> parseOptions(int argc, char** argv);

} // namespace purr::logger
