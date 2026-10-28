// The web server's --selftest cases, one function per area (selftest_*.cpp).
#pragma once

#include "common/selftest.h"
#include "common/wheel.h"
#include "web/site.h"

#include <httplib.h>

#include <cmath>
#include <string>
#include <thread>

inline bool near(double a, double b) { return std::fabs(a - b) < 1e-4; }

// A real database, and a real server answering on a port of this machine, over files in the scratch folder.
struct Fixture {
    purr::Check& check;
    const purr::WheelPresets& presets;
    std::string tmpDir, conf, over, prof, db;
    purr::web::Site srv;
    httplib::Server http;
    int port = 0;
    std::thread listener;

    Fixture(purr::Check& check, const purr::WheelPresets& presets, const std::string& tmpDir);
    ~Fixture();
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;
};

void selftestWheels(purr::Check& check, const purr::WheelPresets& presets);
void selftestConfig(purr::Check& check, const purr::WheelPresets& presets, const std::string& tmpDir);
void selftestDashboard(Fixture& f);
void selftestSettings(Fixture& f);
void selftestSetupPage(Fixture& f);
void selftestWrites(Fixture& f);
