// Settings from /etc and the web UI's overrides, wheel profiles on disk, and the logger's sensor choice.

#include "common/config.h"
#include "common/wheel.h"
#include "web/selftest.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

using namespace purr;

void selftestConfig(Check& check, const WheelPresets& presets, const std::string& tmpDir)
{
    // Config: /etc, then the web UI's overrides over it.
    std::string conf = tmpDir + "/selftest.conf", over = tmpDir + "/selftest-settings.conf",
                prof = tmpDir + "/selftest-profiles.conf";
    std::remove(over.c_str());
    std::remove(prof.c_str());
    std::ofstream(conf) << "[wheel]\nunits = in\nmodel = one-fast-cat\n[web]\nport = 9000\nsettings_access = pin\n";
    Config cfg = loadConfig(presets, conf, over, prof);
    check(cfg.access == SettingsAccess::Off && !cfg.errors.empty(), "pin without a PIN turns saving off");
    std::ofstream(conf) << "[wheel]\nunits = in\nmodel = one-fast-cat\ndiameter = 43.5\n[web]\nport = 9000\n";
    cfg = loadConfig(presets, conf, over, prof);
    check(cfg.access == SettingsAccess::Lan && cfg.port == 9000 && near(cfg.wheel.diameterM, 43.5 * INCH) &&
              !cfg.overridden && cfg.profilesSeeded && cfg.profiles.list.size() == 1 &&
              cfg.profiles.list[0].preset == "one-fast-cat",
          "settings from /etc; the wheel is profile 1, seeded from [wheel]");
    std::ofstream(conf) << "[web]\n";
    Config defaults = loadConfig(presets, conf, over, prof);
    check(defaults.port == 80 && defaults.legacyPort == 8080, "the dashboard on port 80, the old port redirecting");
    std::ofstream(conf) << "[wheel]\nunits = in\nmodel = one-fast-cat\ndiameter = 43.5\n[web]\nport = 9000\n";
    Editable e = cfg.editable;
    e.units = "cm";
    e.model = "grand-ferris";
    e.diameter.reset();
    e.rotate180 = true;
    e.rawLog = true;
    e.rawLogDays = 7;
    check(validate(e).empty(), "valid settings pass");
    writeProfiles(cfg.profiles, prof);
    saveOverrides(e, over);
    cfg = loadConfig(presets, conf, over, prof);
    check(cfg.overridden && cfg.editable.units == "cm" && cfg.editable.rotate180 && cfg.editable.rawLog &&
              cfg.editable.rawLogDays == 7 && cfg.port == 9000 && !cfg.profilesSeeded &&
              near(cfg.wheel.diameterM, 43.5 * INCH),
          "overrides win over /etc; the wheel stays the profile's");
    {
        std::vector<std::string> errs;
        auto back = readProfiles(prof, errs);
        std::ifstream in(prof);
        std::string text((std::istreambuf_iterator<char>(in)), {});
        check(back && errs.empty() && near(*back->list[0].diameterM, 43.5 * INCH) &&
                  text.find("diameter = 110.49cm") != std::string::npos,
              "a profile's diameter is written in cm, whatever is shown");
        Profiles two = *back;
        two.list.push_back({.id = two.nextId(), .name = "Spare", .preset = std::string("star-cat-l")});
        writeProfiles(two, prof);
        Profiles deleted = *readProfiles(prof, errs);
        deleted.list.pop_back();
        writeProfiles(deleted, prof);
        check(readProfiles(prof, errs)->nextId() == 3, "a deleted profile's number is not reused");
        writeProfiles(*back, prof);
        check(validate(Profile{.id = 9, .name = ""}, presets).size() == 2, "a wheel needs a name and a model or size");
    }
    check(normalizeAddress("ed:7b:c2:c9:c1:d3") == "ED:7B:C2:C9:C1:D3" && !normalizeAddress("ED:7B:C2:C9:C1") &&
              !normalizeAddress("ED-7B-C2-C9-C1-D3"),
          "sensor addresses");
    std::ofstream(over) << "[sensor]\naddress = aa:bb:cc:dd:ee:ff\n";
    check(sensorChoice(conf, over) == "AA:BB:CC:DD:EE:FF", "the logger reads the sensor choice");
    std::ofstream(over) << "[sensor]\naddress =\n";
    check(!sensorChoice(conf, over), "an empty choice is automatic");
    std::remove(over.c_str());
    saveOverrides(e, over);
    Editable bad = e;
    bad.units = "mm";
    bad.idleRefreshS = 1;
    check(validate(bad).size() == 2, "invalid settings are refused");
    std::remove(over.c_str());
    std::remove(prof.c_str());
    std::remove(conf.c_str());
}
