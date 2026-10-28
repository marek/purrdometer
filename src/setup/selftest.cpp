// purrdometer-setup --selftest: the parts that need no root and no radio.

#include "common/setup_form.h"
#include "common/setup_state.h"
#include "setup/command.h"
#include "setup/nmcli_parse.h"
#include "setup/password.h"

#include <cstdio>
#include <cstdlib>
#include <print>
#include <string>
#include <sys/stat.h>

using namespace purr::setup;

int selftest()
{
    int failures = 0;
    auto check = [&](bool ok, const std::string& what) {
        std::println("{} {}", ok ? "ok  " : "FAIL", what);
        failures += !ok;
    };
    const char* tmpEnv = std::getenv("PURR_TMP");
    std::string tmp = tmpEnv ? tmpEnv : "tmp";
    mkdir(tmp.c_str(), 0755);

    check(splitTerse("My\\:Net:73:WPA2") == std::vector<std::string>{"My:Net", "73", "WPA2"}, "nmcli escapes");
    auto nets = parseScan("home:45:WPA2\n:80:WPA2\nhome:70:WPA2\ncafe:90:--\n");
    check(nets.size() == 2 && nets[0].ssid == "cafe" && nets[0].security.empty() && nets[1].ssid == "home" &&
              nets[1].signal == 70,
          "scan: strongest first, one per name, no hidden");
    check(parseWifiProfiles("preconfigured:802-11-wireless\nlo:loopback\npurrdometer-setup:802-11-wireless\n",
                            "purrdometer-setup") == std::vector<std::string>{"preconfigured"},
          "the hotspot is not a WiFi profile");
    check(parseAddress("192.168.1.196/24\n") == "192.168.1.196", "the address without its prefix");

    check(PASSWORD_ALPHABET.find_first_of("0Oo1lIi2Z5S8B") == std::string_view::npos,
          "the password alphabet has no look-alike characters");
    bool fromAlphabet = true;
    for (int i = 0; i < 200; ++i)
        for (char c : newPassword())
            fromAlphabet = fromAlphabet && PASSWORD_ALPHABET.find(c) != std::string_view::npos;
    std::string p = newPassword();
    check(fromAlphabet && p.size() == PASSWORD_LENGTH && newPassword() != p,
          "hotspot passwords: from the alphabet, new each time");

    State s{.phase = Phase::Hotspot, .ssid = "purrdometer-372a", .password = "a#b=c d%e", .deadline = 1234};
    writeState(s, tmp + "/setup-state");
    State back = readState(tmp + "/setup-state");
    check(back.phase == Phase::Hotspot && back.password == "a#b=c d%e" && back.deadline == 1234, "state round trip");

    Apply a{.ssid = "Home WiFi", .wifiPassword = "pass:word#1", .loginPassword = "longenough",
            .sshKey = "ssh-ed25519 AAAA me@laptop"};
    writeApply(a, tmp + "/setup-apply");
    struct stat st{};
    stat((tmp + "/setup-apply").c_str(), &st);
    check((st.st_mode & 0777) == 0600, "the form is readable by its owner only");
    auto got = takeApply(tmp + "/setup-apply");
    check(got && got->wifiPassword == "pass:word#1" && got->sshKey == a.sshKey, "the form round trip");
    check(stat((tmp + "/setup-apply").c_str(), &st) != 0, "the form is gone once read");
    check(validate(a, "longenough").empty(), "a complete form passes");
    Apply bad = a;
    bad.wifiPassword = "short";
    bad.sshKey = "AAAA";
    check(validate(bad, "different").size() == 3, "short WiFi password, mismatched login, bad key");
    check(!validate(Apply{}, "").empty(), "no network chosen");

    auto echo = runCommand({"cat"}, "fed through stdin");
    check(echo.ok() && echo.out == "fed through stdin", "commands run without a shell, with input");
    std::remove((tmp + "/setup-state").c_str());
    std::println("{}", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
