// purrdometer-web --selftest: diameters, presets, config and overrides,
// access rules, and a real server answering a real client.

#include "common/calendar.h"
#include "common/config.h"
#include "common/format.h"
#include "common/stats.h"
#include "common/wheel.h"
#include "web/access.h"
#include "web/dashboard_routes.h"
#include "web/html.h"
#include "web/save_guard.h"
#include "web/portal.h"
#include "web/server.h"
#include "common/setup_form.h"
#include "common/setup_state.h"

#include <sys/stat.h>
#include "web/site.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <cmath>
#include <cstdio>
#include <ctime>
#include <format>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>

using namespace purr;

int selftest(const std::string& wheelsPath, const std::string& tmpDir)
{
    int failures = 0;
    auto check = [&](bool ok, const std::string& what) {
        std::cout << (ok ? "ok   " : "FAIL ") << what << '\n';
        failures += !ok;
    };
    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-4; };
    auto throws = [](auto&& f) {
        try {
            f();
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };

    for (const char* t : {"48in", "48\"", "48 inches", "121.92cm", "1219.2 mm", "1.2192m"})
        check(near(parseDiameter(t), 1.2192), std::string("diameter ") + t);
    for (const char* t : {"48ft", "5in", "", "abc", "3m"})
        check(throws([&] { parseDiameter(t); }), std::string("rejects diameter '") + t + "'");
    check(near(parseDiameter("123"), 1.23), "a bare number is cm by default");
    check(near(parseDiameter("48.5", "in"), 48.5 * INCH), "a bare number in inches when units = in");
    check(near(parseDiameter("48.5in", "cm"), 48.5 * INCH), "an explicit unit wins");
    auto hint = [](const std::string& t, const std::string& u) {
        try {
            parseDiameter(t, u);
        } catch (const std::invalid_argument& e) {
            return std::string(e.what());
        }
        return std::string();
    };
    check(hint("48.5", "cm").find("did you mean 48.5in?") != std::string::npos, "hints at inches");
    check(hint("123", "in").find("did you mean 123cm?") != std::string::npos, "hints at centimetres");
    check(throws([&] { parseLength("48"); }), "no unit and none chosen");

    WheelPresets presets = loadPresets(wheelsPath);
    check(presets.find("ferris-g5") && near(presets.find("ferris-g5")->circumferenceM(), 3.8303),
          "Ferris G5 runs on its 48\" inner diameter");
    check(presets.find("grand-ferris") && near(presets.find("grand-ferris")->diameterM, 60 * INCH), "Grand Ferris 60\"");
    check(presets.find("one-fast-cat") && near(presets.find("one-fast-cat")->diameterM, 43 * INCH), "One Fast Cat 43\"");
    check(presets.find("star-cat-xl") && presets.find("star-cat-xl")->estimated, "Star Cat inner diameters are estimates");
    Wheel measured = resolveWheel(presets, std::string("ferris-g5"), std::string("48.5in"));
    check(near(measured.diameterM, 48.5 * INCH) && measured.name.find("measured") != std::string::npos,
          "measured diameter overrides the model");
    check(resolveWheel(presets, std::nullopt, std::nullopt).key == DEFAULT_WHEEL, "default wheel");
    check(describe(*presets.find("ferris-g5")).find("running diameter 121.9 cm (48.0 in), outer 129.5 cm") !=
              std::string::npos, "sizes in cm by default");
    check(describe(*presets.find("ferris-g5"), "in").find("running diameter 48.0 in (121.9 cm), outer 51.0 in") !=
              std::string::npos, "sizes in inches when units = in");
    check(throws([&] { resolveWheel(presets, std::string("hamster-3000"), std::nullopt); }), "unknown model");

    check(cal::weekday(cal::fromIso("2026-10-26")) == 0, "2026-10-26 is a Monday");
    check(cal::iso(cal::fromIso("2024-02-29") + 1) == "2024-03-01", "leap day arithmetic");

    check(web::escape("<a href=\"x\">&'") == "&lt;a href=&quot;x&quot;&gt;&amp;&#39;", "HTML escaping");
    check(thousands(223867) == "223,867" && km(4.84) == "4.8" && km(857.4) == "857", "number formats");

    check(web::isLanAddress("192.168.1.20") && web::isLanAddress("10.0.0.5") && web::isLanAddress("172.20.1.1") &&
              web::isLanAddress("127.0.0.1") && web::isLanAddress("::1") && web::isLanAddress("fe80::1") &&
              web::isLanAddress("fd12::3") && web::isLanAddress("::ffff:192.168.1.20"),
          "home-network addresses");
    check(!web::isLanAddress("8.8.8.8") && !web::isLanAddress("172.32.0.1") && !web::isLanAddress("2001:db8::1") &&
              !web::isLanAddress("") && !web::isLanAddress("::ffff:8.8.8.8"),
          "internet addresses");
    check(web::sameSecret("abc", "abc") && !web::sameSecret("abc", "abd") && !web::sameSecret("", ""), "secret compare");

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
    Editable e = cfg.editable;
    e.units = "cm";
    e.model = "grand-ferris";
    e.diameter.reset();
    e.rotate180 = true;
    check(validate(e).empty(), "valid settings pass");
    writeProfiles(cfg.profiles, prof);
    saveOverrides(e, over);
    cfg = loadConfig(presets, conf, over, prof);
    check(cfg.overridden && cfg.editable.units == "cm" && cfg.editable.rotate180 && cfg.port == 9000 &&
              !cfg.profilesSeeded && near(cfg.wheel.diameterM, 43.5 * INCH),
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

    // A real database, a real server, a real client.
    std::string db = tmpDir + "/purrdometer-web-selftest.db";
    std::remove(db.c_str());
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
    web::Site srv;
    srv.paths.profiles = prof;
    std::remove(prof.c_str());
    srv.paths.db = db;
    srv.paths.config = conf;
    srv.paths.overrides = over;
    srv.presets = presets;
    Stats st = loadStats(db, cal::localDay(std::time(nullptr)), circumferences(srv.config()));
    check(st.total == 150 && st.today == 100 && st.live.connected && st.live.rpm == 54, "database read");
    std::string html = web::renderDashboard(srv);
    check(html.find("150") != std::string::npos && html.find("Running") != std::string::npos &&
              html.find("battery 85%") != std::string::npos && html.find("<polyline") != std::string::npos &&
              html.find("href=\"/settings\"") != std::string::npos,
          "dashboard renders totals, live state, charts and the gear");
    auto api = nlohmann::json::parse(web::renderApi(srv));
    check(api["total"]["rotations"] == 150 && api["daily"].size() == 2, "JSON API");

    std::remove(over.c_str());
    httplib::Server http;
    web::addRoutes(srv, http);
    int port = http.bind_to_any_port("127.0.0.1");
    std::thread listener([&] { http.listen_after_bind(); });
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
    check(page && page->body.find("CAD-BLE0384139") != std::string::npos &&
              page->body.find("SPD-BLE1") != std::string::npos && page->body.find("battery 85%") != std::string::npos &&
              page->body.find("cadence sensor") != std::string::npos,
          "the sensors heard, the one connected and its mounting help");

    httplib::Headers withCookie = {{"Cookie", prefix + token}};
    auto form = [&](const std::string& diameter) {
        return httplib::Params{{"token", token}, {"units", "cm"}, {"model", "star-cat-xl"}, {"diameter", diameter},
                               {"active_refresh_s", "5"}, {"idle_refresh_s", "60"}, {"full_refresh_every", "20"}};
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

    // Setup mode's page and portal, with this machine standing in for the hotspot.
    srv.paths.setupState = tmpDir + "/selftest-setup-state";
    srv.paths.setupNetworks = tmpDir + "/selftest-setup-networks";
    srv.paths.setupApply = tmpDir + "/selftest-setup-apply";
    for (auto& f : {srv.paths.setupState, srv.paths.setupNetworks, srv.paths.setupApply})
        std::remove(f.c_str());
    std::ofstream(conf) << "[wheel]\nmodel = ferris-g5\n";
    httplib::Server portal;
    addPortalRoutes(srv, portal);
    int portalPort = portal.bind_to_any_port("127.0.0.1");
    std::thread portalThread([&] { portal.listen_after_bind(); });
    httplib::Client phone("127.0.0.1", portalPort);
    httplib::Result probe;
    for (int i = 0; i < 50 && !(probe = phone.Get("/setup")); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto notRunning = phone.Get("/setup");
    check(notRunning && notRunning->status == 403 && notRunning->body.find("Setup is not running") != std::string::npos,
          "the setup page is closed while setup is not running");
    srv.hotspotPrefix = "127.0.0.";
    setup::writeState({.phase = setup::Phase::Hotspot, .ssid = "purrdometer-c12e", .password = "abcdefghjk",
                       .deadline = std::time(nullptr) + 600},
                      srv.paths.setupState);
    setup::writeNetworks({{"Periphery", 73, "WPA2"}, {"Cafe", 40, ""}}, srv.paths.setupNetworks);
    auto setupPage = phone.Get("/setup");
    std::string setupCookie = setupPage ? setupPage->get_header_value("Set-Cookie") : "";
    std::string setupToken = setupCookie.starts_with(prefix) ? setupCookie.substr(prefix.size(), 32) : "";
    check(setupPage && setupPage->status == 200 && setupPage->body.find("Periphery") != std::string::npos &&
              setupPage->body.find("Cafe") != std::string::npos && setupCookie.find("Path=/setup") != std::string::npos,
          "on the hotspot: the networks scanned, and a form token for /setup");
    httplib::Headers setupHeaders = {{"Cookie", prefix + setupToken}};
    auto setupForm = [&](const std::string& confirm) {
        return httplib::Params{{"token", setupToken},           {"network", "Periphery"},
                               {"wifi_password", "pass:word#1"}, {"login_password", "longenough"},
                               {"login_confirm", confirm},       {"ssh", "1"},
                               {"ssh_key", "ssh-ed25519 AAAA me@laptop"}, {"preset", "ferris-g5"},
                               {"diameter", ""}};
    };
    struct stat file{};
    auto noSetupToken = phone.Post("/setup", setupForm("longenough"));
    check(noSetupToken && noSetupToken->status == 403 && stat(srv.paths.setupApply.c_str(), &file) != 0,
          "a setup form without its token is refused");
    auto mismatch = phone.Post("/setup", setupHeaders, setupForm("different"));
    check(mismatch && mismatch->status == 400 && mismatch->body.find("differ") != std::string::npos &&
              stat(srv.paths.setupApply.c_str(), &file) != 0,
          "mismatched login passwords are refused, and nothing reaches the helper");
    auto sent = phone.Post("/setup", setupHeaders, setupForm("longenough"));
    auto applied = setup::readApply(srv.paths.setupApply);
    stat(srv.paths.setupApply.c_str(), &file);
    check(sent && sent->status == 303 && applied && applied->ssid == "Periphery" &&
              applied->wifiPassword == "pass:word#1" && applied->ssh && (file.st_mode & 0777) == 0600,
          "a complete form goes to the helper, readable by its owner only");

    probe = phone.Get("/generate_204");
    check(probe && probe->status == 302 && probe->get_header_value("Location") == "http://10.42.0.1/setup",
          "a phone's connectivity check lands on the setup page");
    setup::writeState({}, srv.paths.setupState);
    srv.port = port;
    auto home = phone.Get("/");
    check(home && home->status == 302 && home->get_header_value("Location") == std::format("http://127.0.0.1:{}/", port),
          "outside setup, port 80 sends to the dashboard");
    portal.stop();
    portalThread.join();
    for (auto& f : {srv.paths.setupState, srv.paths.setupNetworks, srv.paths.setupApply})
        std::remove(f.c_str());

    http.stop();
    listener.join();
    std::remove(db.c_str());
    std::remove(conf.c_str());

    std::cout << (failures ? "FAILED\n" : "all passed\n");
    return failures ? 1 : 0;
}
