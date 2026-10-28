// purrdometer.conf plus the web UI's overrides, as both C++ programs see them.
// docs/intents/web/settings.md owns which keys exist, their ranges, and which
// of them the web UI may change.
#pragma once

#include "common/paths.h"
#include "common/profiles.h"
#include "common/wheel.h"

#include <optional>
#include <string>
#include <vector>

namespace purr {


// Who may save settings from the web UI. Off: nobody, edit the config file.
enum class SettingsAccess { Lan, Pin, Open, Off };

// The keys the web UI may change. Everything else is /etc only.
struct Editable {
    std::string units = DEFAULT_UNITS;
    std::string model = DEFAULT_WHEEL;
    std::optional<std::string> diameter; // as written, in `units` when bare; with model, seeds profile 1
    bool rotate180 = false;
    int fullRefreshEvery = 20;
    int activeRefreshS = 5;
    int idleRefreshS = 60;
    std::optional<std::string> sensor; // a sensor's address; empty for the first one heard
    int wakeTurns = 2; // turns the sensor misses waking up (docs/intents/data/accounting.md)
};

struct Config {
    Editable editable;
    Profiles profiles;         // profiles.conf, or profile 1 seeded from [wheel]
    bool profilesSeeded = false; // no profiles.conf yet
    std::map<int, Wheel> wheels; // each profile's wheel
    Wheel wheel;               // the current profile's
    std::string bind = "0.0.0.0";
    int port = 8080;
    SettingsAccess access = SettingsAccess::Lan;
    std::string pin;
    std::vector<std::string> errors;
    bool overridden = false;   // an overrides file exists
};

// Reads conf, then overrides over it. Invalid values are reported in errors and fall back.
Config loadConfig(const WheelPresets& presets, const std::string& conf = CONFIG_PATH,
                  const std::string& overrides = OVERRIDES_PATH, const std::string& profiles = PROFILES_PATH);

// Each profile's wheel circumference in metres, for distance from stored rotations.
std::map<int, double> circumferences(const Config& c);

// Checks a set of editable values; returns the problems, empty when they are valid.
std::vector<std::string> validate(const Editable& e);

// Writes the editable values atomically (temp file + rename). Throws std::runtime_error.
void saveOverrides(const Editable& e, const std::string& path = OVERRIDES_PATH);

const char* accessName(SettingsAccess a);

// "AA:BB:CC:DD:EE:FF", uppercase; nullopt if s is not a Bluetooth address.
std::optional<std::string> normalizeAddress(const std::string& s);

// The editable values alone, without wheel presets: what the logger reads.
Editable loadEditable(const std::string& conf = CONFIG_PATH, const std::string& overrides = OVERRIDES_PATH);

// The sensor the settings choose.
std::optional<std::string> sensorChoice(const std::string& conf = CONFIG_PATH,
                                        const std::string& overrides = OVERRIDES_PATH);

} // namespace purr
