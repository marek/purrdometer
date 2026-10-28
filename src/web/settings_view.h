// What the settings page shows (docs/intents/web/settings.md).
#pragma once

#include "common/config.h"
#include "common/stats.h"
#include "common/wheel.h"

#include <ctime>
#include <string>
#include <vector>

namespace purr {

struct SettingsView {
    Editable values;               // what the form shows: saved values, or a rejected submission
    const WheelPresets* presets = nullptr;
    Wheel current;                 // the wheel in effect now
    SettingsAccess access = SettingsAccess::Lan;
    bool canSave = false;          // this client, under the access rule
    std::string whyNot;            // when !canSave
    std::string token;             // form token, also set as a cookie
    bool overridden = false;       // settings.ini exists
    bool saved = false, wasReset = false;
    std::vector<std::string> problems;     // a rejected submission
    std::vector<std::string> configErrors; // problems in the files themselves

    // The sensor section.
    std::vector<SeenSensor> sensors;   // heard by the logger, newest first
    Live live;                         // the sensor in use, its kind and battery
    bool scanning = false;             // a scan the page asked for is still running
    std::time_t now = 0;
};

} // namespace purr
