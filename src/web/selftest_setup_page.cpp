// Setup mode's page and the captive redirect, and the old port's redirect.

#include "common/setup_form.h"
#include "common/setup_state.h"
#include "web/legacy.h"
#include "web/save_guard.h"
#include "web/selftest.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <thread>

using namespace purr;

void selftestSetupPage(Fixture& f)
{
    purr::Check& check = f.check;
    web::Site& srv = f.srv;
    const std::string &tmpDir = f.tmpDir, &conf = f.conf;
    const int port = f.port;
    const std::string prefix = std::string(web::TOKEN_COOKIE) + "=";
    // Setup mode's page and captive redirect, with this machine standing in for the hotspot.
    srv.paths.setupState = tmpDir + "/selftest-setup-state";
    srv.paths.setupNetworks = tmpDir + "/selftest-setup-networks";
    srv.paths.setupApply = tmpDir + "/selftest-setup-apply";
    for (auto& f : {srv.paths.setupState, srv.paths.setupNetworks, srv.paths.setupApply})
        std::remove(f.c_str());
    std::ofstream(conf) << "[wheel]\nmodel = ferris-g5\n";
    httplib::Client phone("127.0.0.1", port); // the same server: the setup page is the dashboard's
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

    auto probe = phone.Get("/generate_204");
    auto anySite = phone.Get("/");
    check(probe && probe->status == 302 && probe->get_header_value("Location") == "http://10.42.0.1/setup" && anySite &&
              anySite->status == 302,
          "in setup, a hotspot client's connectivity check and any page land on the setup page");
    setup::writeState({}, srv.paths.setupState);
    auto home = phone.Get("/");
    check(home && home->status == 200 && home->body.find("href=\"/settings\"") != std::string::npos,
          "outside setup, the same client gets the dashboard");
    srv.hotspotPrefix = setup::HOTSPOT_PREFIX;

    httplib::Server old;
    web::addLegacyRedirect(80, old);
    int oldPort = old.bind_to_any_port("127.0.0.1");
    std::thread oldThread([&] { old.listen_after_bind(); });
    httplib::Client bookmark("127.0.0.1", oldPort);
    httplib::Result moved;
    for (int i = 0; i < 50 && !(moved = bookmark.Get("/settings?saved=1")); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(moved && moved->status == 301 && moved->get_header_value("Location") == "http://127.0.0.1/settings?saved=1",
          "the old port redirects to the same page on port 80");
    old.stop();
    oldThread.join();
    for (auto& f : {srv.paths.setupState, srv.paths.setupNetworks, srv.paths.setupApply})
        std::remove(f.c_str());
}
