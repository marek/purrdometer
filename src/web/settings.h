// What the settings page shows (docs/intents/web/settings.md).
#pragma once

#include "common/config.h"
#include "common/wheel.h"

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
};

} // namespace purr
