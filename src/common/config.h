// purrdometer.conf plus the web UI's overrides, as both C++ programs see them.
// docs/intents/web/settings.md owns which keys exist, their ranges, and which
// of them the web UI may change.
#pragma once

#include "common/wheel.h"

#include <optional>
#include <string>
#include <vector>

namespace purr {

constexpr const char* CONFIG_PATH = "/etc/purrdometer/purrdometer.conf";
constexpr const char* OVERRIDES_PATH = "/var/lib/purrdometer/settings.ini"; // written by the web UI
constexpr const char* WHEELS_PATH = "/usr/share/purrdometer/wheels.ini";

// Who may save settings from the web UI. Off: nobody, edit the config file.
enum class SettingsAccess { Lan, Pin, Open, Off };

// The keys the web UI may change. Everything else is /etc only.
struct Editable {
    std::string units = DEFAULT_UNITS;
    std::string model = DEFAULT_WHEEL;
    std::optional<std::string> diameter; // as written, in `units` when bare
    bool rotate180 = false;
    int fullRefreshEvery = 20;
    int activeRefreshS = 5;
    int idleRefreshS = 60;
};

struct Config {
    Editable editable;
    Wheel wheel;               // resolved from editable
    std::string bind = "0.0.0.0";
    int port = 8080;
    SettingsAccess access = SettingsAccess::Lan;
    std::string pin;
    std::vector<std::string> errors;
    bool overridden = false;   // an overrides file exists
};

// Reads conf, then overrides over it. Invalid values are reported in errors and fall back.
Config loadConfig(const WheelPresets& presets, const std::string& conf = CONFIG_PATH,
                  const std::string& overrides = OVERRIDES_PATH);

// Checks a set of editable values; returns the problems, empty when they are valid.
std::vector<std::string> validate(const Editable& e, const WheelPresets& presets);

// Writes the editable values atomically (temp file + rename). Throws std::runtime_error.
void saveOverrides(const Editable& e, const std::string& path = OVERRIDES_PATH);

const char* accessName(SettingsAccess a);

} // namespace purr
