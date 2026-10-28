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

    // The wheels (docs/intents/web/settings.md#wheels).
    struct WheelRow {
        Profile profile;
        Wheel wheel;
        double km = 0;             // its rotations on its wheel
        bool inUse = false, hasRotations = false;
    };
    std::vector<WheelRow> wheels;
    int nextWheel = 0;             // the number a new wheel gets
    std::string wheelSaved, wheelError;
    SettingsAccess access = SettingsAccess::Lan;
    bool canSave = false;          // this client, under the access rule
    std::string whyNot;            // when !canSave
    std::string token;             // form token, also set as a cookie
    bool overridden = false;       // settings.conf exists
    bool saved = false, wasReset = false;
    std::vector<std::string> problems;     // a rejected submission
    std::vector<std::string> configErrors; // problems in the files themselves

    // The sensor section.
    std::vector<SeenSensor> sensors;   // heard by the logger, newest first
    Live live;                         // the sensor in use, its kind and battery
    bool scanning = false;             // a scan the page asked for is still running
    std::vector<std::string> sensorLogDays; // newest first, for download
    std::time_t now = 0;
};

} // namespace purr
