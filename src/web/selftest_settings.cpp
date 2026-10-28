// The settings page and the wheels form, over HTTP: tokens, access, saving,
// resetting, and what the logger reads back.

#include "common/calendar.h"
#include "common/config.h"
#include "common/stats.h"
#include "common/wheel.h"
#include "web/save_guard.h"
#include "web/selftest.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

using namespace purr;

void selftestSettings(Fixture& f)
{
    purr::Check& check = f.check;
    web::Site& srv = f.srv;
    const std::string &tmpDir = f.tmpDir, &conf = f.conf, &over = f.over, &prof = f.prof, &db = f.db;
    const int port = f.port;
    auto& presets = f.presets;
    (void)presets;
    httplib::Client client("127.0.0.1", port);
    httplib::Result page;
    for (int i = 0; i < 50 && !(page = client.Get("/settings")); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string setCookie = page ? page->get_header_value("Set-Cookie") : "";
    std::string prefix = std::string(web::TOKEN_COOKIE) + "=";
    std::string token = setCookie.starts_with(prefix) ? setCookie.substr(prefix.size(), 32) : "";
    check(page && page->status == 200 && token.size() == 32 &&
              page->body.find("value=\"one-fast-cat\" selected") != std::string::npos,
          "settings page and its form token");
    check(page && page->body.find("/settings/sensor-log?day=2026-10-28") != std::string::npos &&
              page->body.find("2026-10-27") == std::string::npos,
          "settings page links ordinary raw sensor-log days, not links");
    auto rawDownload = client.Get("/settings/sensor-log?day=2026-10-28");
    auto notLog = client.Get("/settings/sensor-log?day=notes");
    auto linkedLog = client.Get("/settings/sensor-log?day=2026-10-27");
    check(rawDownload && rawDownload->status == 200 && rawDownload->body == "a raw measurement\n" &&
              rawDownload->get_header_value("Content-Disposition").find("2026-10-28.log") != std::string::npos &&
              notLog && notLog->status == 404 && linkedLog && linkedLog->status == 404,
          "a raw sensor-log day downloads, and nothing else in the folder does");
    check(page && page->body.find("CAD-BLE0384139") != std::string::npos &&
              page->body.find("SPD-BLE1") != std::string::npos && page->body.find("battery 85%") != std::string::npos &&
              page->body.find("cadence sensor") != std::string::npos,
          "the sensors heard, the one connected and its mounting help");

    httplib::Headers withCookie = {{"Cookie", prefix + token}};
    auto form = [&](const std::string& diameter) {
        return httplib::Params{{"token", token}, {"units", "cm"}, {"model", "star-cat-xl"}, {"diameter", diameter},
                               {"active_refresh_s", "5"}, {"idle_refresh_s", "60"}, {"full_refresh_every", "20"},
                               {"raw_log_days", "2"}};
    };
    auto noToken = client.Post("/settings", httplib::Params{{"units", "cm"}, {"model", "star-cat-xl"}});
    check(noToken && noToken->status == 403 && access(over.c_str(), F_OK) != 0, "a save without the token is refused");
    auto saved = client.Post("/settings", withCookie, form("115"));
    Config after = srv.config();
    check(saved && saved->status == 303 && after.editable.units == "cm" && !after.profilesSeeded &&
              near(after.wheel.diameterM, 43.5 * INCH),
          "a save from the home network is written and used; the wheel went to profiles.conf first");
    auto wheel = [&](httplib::Params p) {
        p.emplace("token", token);
        return client.Post("/settings/wheels", withCookie, p);
    };
    double km1 = loadStats(db, cal::localDay(std::time(nullptr)), circumferences(srv.config())).totalKm;
    auto edited = wheel({{"do", "edit"}, {"id", "1"}, {"name", "Living room"}, {"preset", "one-fast-cat"}, {"diameter", "120"}});
    Config ed = srv.config();
    double km2 = loadStats(db, cal::localDay(std::time(nullptr)), circumferences(ed)).totalKm;
    check(edited && edited->status == 303 && edited->get_header_value("Location").find("wheel_saved") != std::string::npos &&
              near(ed.wheel.diameterM, 1.20) && ed.profiles.list[0].name == "Living room" && near(km2, km1 * 1.20 / (43.5 * INCH)),
          "editing a wheel corrects the distance of its rotations");
    auto badSize = wheel({{"do", "edit"}, {"id", "1"}, {"name", "Living room"}, {"preset", ""}, {"diameter", "48.5"}});
    check(badSize && badSize->get_header_value("Location").find("48.5in") != std::string::npos &&
              near(srv.config().wheel.diameterM, 1.20),
          "an implausible diameter is refused with a hint");
    auto added = wheel({{"do", "new"}, {"name", "Big wheel"}, {"preset", "grand-ferris"}, {"diameter", ""}});
    Config two = srv.config();
    Stats afterNew = loadStats(db, cal::localDay(std::time(nullptr)), circumferences(two));
    check(added && two.profiles.current == 2 && two.wheel.key == "grand-ferris" && near(afterNew.totalKm, km2),
          "a new wheel is in use, and history keeps its distance");
    auto keep = wheel({{"do", "delete"}, {"id", "1"}});
    check(keep && srv.config().profiles.find(1), "a wheel with rotations is not deleted");
    auto back = wheel({{"do", "use"}, {"id", "1"}});
    auto gone = wheel({{"do", "delete"}, {"id", "2"}});
    Config one = srv.config();
    check(back && gone && one.profiles.current == 1 && !one.profiles.find(2) && one.profiles.nextId() == 3,
          "switching back, and deleting an unused wheel");
    auto pageNow = client.Get("/settings");
    check(pageNow && pageNow->body.find("Living room") != std::string::npos &&
              pageNow->body.find("in use") != std::string::npos,
          "the settings page lists the wheels");
    auto chosen = form("115");
    chosen.emplace("sensor", "aa:bb:cc:dd:ee:ff");
    auto savedSensor = client.Post("/settings", withCookie, chosen);
    check(savedSensor && savedSensor->status == 303 && sensorChoice(conf, over) == "AA:BB:CC:DD:EE:FF",
          "a chosen sensor is saved for the logger");
    auto tuned = form("115");
    tuned.emplace("wake_turns", "3");
    auto savedTurns = client.Post("/settings", withCookie, tuned);
    Editable turns = loadEditable(conf, over);
    check(savedTurns && savedTurns->status == 303 && turns.wakeTurns == 3,
          "wake turns are saved for the logger");
    tuned = form("115");
    tuned.erase("raw_log_days");
    tuned.emplace("raw_log", "on");
    tuned.emplace("raw_log_days", "5");
    auto savedRawLog = client.Post("/settings", withCookie, tuned);
    Editable rawSettings = loadEditable(conf, over);
    check(savedRawLog && savedRawLog->status == 303 && rawSettings.rawLog && rawSettings.rawLogDays == 5,
          "raw sensor logging and its retention are saved for the logger");
    tuned = form("115");
    tuned.erase("raw_log_days");
    tuned.emplace("raw_log", "on");
    tuned.emplace("raw_log_days", "15");
    auto tooManyLogDays = client.Post("/settings", withCookie, tuned);
    check(tooManyLogDays && tooManyLogDays->status == 400 && loadEditable(conf, over).rawLogDays == 5,
          "raw sensor log retention is 1 to 14 days");
    tuned = form("115");
    tuned.emplace("wake_turns", "11");
    auto tooMany = client.Post("/settings", withCookie, tuned);
    check(tooMany && tooMany->status == 400 && loadEditable(conf, over).wakeTurns == 3, "more than 10 is refused");
    srv.paths.scanRequest = tmpDir + "/selftest-scan-request";
    std::remove(srv.paths.scanRequest.c_str());
    auto scan = client.Post("/settings/scan", withCookie, httplib::Params{{"token", token}});
    check(scan && scan->status == 303 && access(srv.paths.scanRequest.c_str(), F_OK) == 0,
          "scan asks the logger through its request file");
    std::remove(srv.paths.scanRequest.c_str());
    auto reset = client.Post("/settings/reset", withCookie, httplib::Params{{"token", token}});
    check(reset && reset->status == 303 && access(over.c_str(), F_OK) != 0 && access(prof.c_str(), F_OK) == 0,
          "reset removes the overrides and keeps the wheels");
    std::ofstream(conf) << "[wheel]\nmodel = ferris-g5\n[web]\nsettings_access = off\n";
    auto off = client.Post("/settings", withCookie, form("115"));
    auto offWheel = wheel({{"do", "use"}, {"id", "1"}});
    check(off && off->status == 403 && access(over.c_str(), F_OK) != 0 && offWheel &&
              offWheel->get_header_value("Location").find("wheel_error") != std::string::npos,
          "settings_access = off refuses saving, wheels included");
    std::remove(prof.c_str());
}
