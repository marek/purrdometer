// The config as it is now: loaded once, and again whenever purrdometer.conf
// or the web UI's overrides change, so a saved setting applies without a
// restart.
#pragma once

#include "common/config.h"
#include "common/file_watch.h"

#include <print>
#include <string>

namespace purr {

class LiveConfig {
public:
    LiveConfig(const WheelPresets& presets, std::string conf = CONFIG_PATH, std::string overrides = OVERRIDES_PATH)
        : presets_(presets), conf_(std::move(conf)), overrides_(std::move(overrides)), watch_({conf_, overrides_}),
          config_(loadConfig(presets_, conf_, overrides_))
    {
    }

    const Config& get() const { return config_; }

    // True when a change was read in; problems are logged.
    bool reload()
    {
        if (!watch_.changed())
            return false;
        config_ = loadConfig(presets_, conf_, overrides_);
        std::println("config reloaded: {}", describe(config_.wheel, config_.editable.units));
        for (auto& e : config_.errors)
            std::println("config error: {}", e);
        return true;
    }

private:
    const WheelPresets& presets_;
    std::string conf_, overrides_;
    FileWatch watch_;
    Config config_;
};

} // namespace purr
