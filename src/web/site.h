// What every route reads: where the files are, the wheel presets, and the
// config, read on every request (the files are tiny, and a change applies at
// once).
#pragma once

#include "common/config.h"
#include "common/wheel.h"

#include <optional>
#include <string>

namespace purr::web {

struct Paths {
    std::string db = "/var/lib/purrdometer/purrdometer.db";
    std::string config = CONFIG_PATH;
    std::string overrides = OVERRIDES_PATH;
    std::string wheels = WHEELS_PATH;
    std::string scanRequest = "/var/lib/purrdometer/scan-request"; // touched to ask the logger for a scan
};

struct Site {
    Paths paths;
    WheelPresets presets;
    std::optional<std::string> bind;
    std::optional<int> port;

    Config config() const { return loadConfig(presets, paths.config, paths.overrides); }
};

} // namespace purr::web
