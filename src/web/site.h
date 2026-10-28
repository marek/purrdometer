// What every route reads: where the files are, the wheel presets, and the
// config, read on every request (the files are tiny, and a change applies at
// once).
#pragma once

#include "common/config.h"
#include "common/setup_form.h"
#include "common/setup_state.h"
#include "common/wheel.h"

#include <optional>
#include <string>

namespace purr::web {

struct Paths {
    std::string db = DB_PATH;
    std::string config = CONFIG_PATH;
    std::string overrides = OVERRIDES_PATH;
    std::string profiles = PROFILES_PATH;
    std::string wheels = WHEELS_PATH;
    std::string scanRequest = SCAN_REQUEST_PATH; // touched to ask the logger for a scan
    std::string setupState = setup::STATE_PATH, setupNetworks = setup::NETWORKS_PATH, setupApply = setup::APPLY_PATH;
};

struct Site {
    Paths paths;
    WheelPresets presets;
    std::optional<std::string> bind;
    std::optional<int> port;
    std::optional<int> legacyPort;                // overrides [web] legacy_port
    std::string hotspotPrefix = setup::HOTSPOT_PREFIX; // who may use /setup

    Config config() const { return loadConfig(presets, paths.config, paths.overrides, paths.profiles); }
};

} // namespace purr::web
