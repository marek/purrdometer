// Which sensor to follow: [sensor] address from the settings page or the
// config file; without one, the first sensor heard, remembered in the sensor
// file. Follows the settings without a restart.
#pragma once

#include "common/file_watch.h"
#include "logger/options.h"

#include <optional>
#include <string>

namespace purr::logger {

class SensorChoice {
public:
    explicit SensorChoice(const Paths& paths);

    // The address to follow; nullopt: any CSC sensor, the first one heard.
    std::optional<std::string> target() const;
    // True once after the settings chose differently.
    bool changed();
    // The first sensor heard, when the settings name none.
    void remember(const std::string& address);

    bool automatic() const { return !choice_; }
    std::string describe() const { return choice_.value_or("the first one heard"); }

private:
    Paths paths_;
    FileWatch settings_;
    std::optional<std::string> choice_;
};

} // namespace purr::logger
