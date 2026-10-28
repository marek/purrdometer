// purrdometer-web --selftest: diameters, presets, config and overrides,
// access rules, and a real server answering a real client.

#include "web/selftest.h"

#include "common/wheel.h"
#include "web/server.h"

#include <sqlite3.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace purr;

Fixture::Fixture(Check& c, const WheelPresets& p, const std::string& tmp)
    : check(c), presets(p), tmpDir(tmp), conf(tmp + "/selftest.conf"), over(tmp + "/selftest-settings.conf"),
      prof(tmp + "/selftest-profiles.conf"), db(tmp + "/purrdometer-web-selftest.db")
{
    std::ofstream(conf) << "[wheel]\nunits = in\nmodel = one-fast-cat\ndiameter = 43.5\n[web]\nport = 9000\n";
    for (auto* file : {&over, &prof, &db})
        std::remove(file->c_str());
    {
        sqlite3* raw = nullptr;
        sqlite3_open(db.c_str(), &raw);
        std::time_t now = std::time(nullptr);
        std::string sql = "CREATE TABLE minutes (minute INTEGER PRIMARY KEY, revs INTEGER NOT NULL);"
                          "CREATE TABLE state (key TEXT PRIMARY KEY, value TEXT);"
                          "INSERT INTO minutes VALUES (" + std::to_string(now / 60) + ", 100), (" +
                          std::to_string(now / 60 - 3 * 1440) + ", 50);"
                          "INSERT INTO state VALUES ('connected','1'), ('rpm','54'), ('last_motion','" +
                          std::to_string(now) + "'), ('battery','85'), ('sensor','ED:7B:C2:C9:C1:D3'), "
                          "('sensor_kind','cadence');"
                          "CREATE TABLE sensors (address TEXT PRIMARY KEY, name TEXT, kind TEXT, rssi INTEGER, "
                          "last_seen REAL);"
                          "INSERT INTO sensors VALUES ('ED:7B:C2:C9:C1:D3', 'CAD-BLE0384139', 'cadence', -54, " +
                          std::to_string(now) + "), ('AA:BB:CC:DD:EE:FF', 'SPD-BLE1', NULL, -80, " +
                          std::to_string(now - 60) + ");";
        sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr);
        sqlite3_close(raw);
    }
    srv.paths.profiles = prof;
    srv.paths.db = db;
    srv.paths.sensorLogDir = tmpDir + "/selftest-sensor-log";
    std::filesystem::remove_all(srv.paths.sensorLogDir);
    std::filesystem::create_directories(srv.paths.sensorLogDir);
    std::ofstream(srv.paths.sensorLogDir + "/2026-10-28.log") << "a raw measurement\n";
    std::ofstream(srv.paths.sensorLogDir + "/notes.txt") << "not a log\n";
    std::error_code symlinkError;
    std::filesystem::create_symlink(srv.paths.sensorLogDir + "/notes.txt",
                                    srv.paths.sensorLogDir + "/2026-10-27.log", symlinkError);
    srv.paths.config = conf;
    srv.paths.overrides = over;
    srv.presets = presets;
    web::addRoutes(srv, http);
    port = http.bind_to_any_port("127.0.0.1");
    listener = std::thread([this] { http.listen_after_bind(); });
}

Fixture::~Fixture()
{
    http.stop();
    listener.join();
    for (auto* file : {&over, &prof, &db, &conf})
        std::remove(file->c_str());
    std::filesystem::remove_all(srv.paths.sensorLogDir);
}

int selftest(const std::string& wheelsPath, const std::string& tmpDir)
{
    Check check;
    WheelPresets presets = loadPresets(wheelsPath);
    selftestWheels(check, presets);
    selftestConfig(check, presets, tmpDir);
    Fixture fixture(check, presets, tmpDir);
    selftestDashboard(fixture);
    selftestSettings(fixture);
    selftestSetupPage(fixture);
    selftestWrites(fixture);
    return check.finish();
}
