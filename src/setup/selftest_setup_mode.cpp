// Setup mode's state machine over a NetworkManager whose nmcli is scripted,
// so every failure it must survive can be made to happen.

#include "common/setup_state.h"
#include "setup/network_manager.h"
#include "setup/selftest.h"
#include "setup/setup_mode.h"

#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <sys/stat.h>

using namespace purr::setup;

namespace {

CommandResult ok(std::string out = "")
{
    CommandResult r;
    r.status = 0;
    r.out = std::move(out);
    return r;
}

CommandResult failure(std::string why = "boom")
{
    CommandResult r;
    r.status = 4;
    r.err = std::move(why);
    return r;
}

CommandResult timedOut()
{
    CommandResult r;
    r.timedOut = true;
    return r;
}

// A NetworkManager that answers by the nmcli subcommand and remembers what it was asked.
struct FakeNmcli {
    std::map<std::string, CommandResult> answers; // keyed by "connection show", "device show", ...
    std::vector<std::string> asked;

    static std::string subcommand(const std::vector<std::string>& argv)
    {
        std::string all;
        for (auto& a : argv)
            all += a + " ";
        for (const char* key : {"device wifi hotspot", "device wifi connect", "device wifi list", "device connect",
                                "device show", "connection show", "connection delete", "connection down",
                                "connection modify"})
            if (all.contains(key))
                return key;
        return all;
    }

    Runner runner()
    {
        return [this](const std::vector<std::string>& argv, const std::string&, std::chrono::milliseconds) {
            std::string key = subcommand(argv);
            asked.push_back(key);
            auto it = answers.find(key);
            return it == answers.end() ? ok() : it->second;
        };
    }

    int count(const std::string& key) const
    {
        int n = 0;
        for (auto& a : asked)
            n += a == key;
        return n;
    }
};


const std::string WIFI_PROFILE = "home:802-11-wireless\nlo:loopback\n";

struct Rig {
    FakeNmcli nm;
    NetworkManager wifi;
    std::string state, request, apply, networks;
    std::vector<std::string> loginAsked;
    std::vector<std::string> loginFails;

    explicit Rig(const std::string& tmp, const std::string& name)
        : wifi("wlan0", nm.runner()), state(tmp + "/" + name + "-state"), request(tmp + "/" + name + "-request"),
          apply(tmp + "/" + name + "-apply"), networks(tmp + "/" + name + "-networks")
    {
        for (auto* f : {&state, &request, &apply, &networks})
            std::remove(f->c_str());
    }
    SetupMode mode(int minutes = 10)
    {
        return SetupMode(wifi, minutes, SetupPaths{state, request, apply, networks}, [this](const Apply& a) {
            loginAsked.push_back(a.ssid);
            return loginFails;
        });
    }
    State published() const { return readState(state); }
    void submit(const std::string& ssid = "Home")
    {
        writeApply(Apply{.ssid = ssid, .wifiPassword = "longenough", .loginPassword = "longenough"}, apply);
    }
    ~Rig()
    {
        for (auto* f : {&state, &request, &apply, &networks})
            std::remove(f->c_str());
    }
};

} // namespace

void selftestSetupMode(purr::Check& check)
{
    std::string tmp = selftestTmp();

    {
        Rig r(tmp, "configured-but-unreadable");
        for (auto failing : {failure("Error: NetworkManager is not running."), timedOut()}) {
            r.nm.answers["connection show"] = failing;
            auto mode = r.mode();
            mode.begin(1000);
            mode.tick(1001);
            mode.tick(1000 + SetupMode::RETRY_S - 1);
            check(!r.published().active() && r.nm.count("device wifi hotspot") == 0 && r.nm.count("connection delete") == 0,
                  "nmcli failing or timing out is not an unconfigured Pi: no hotspot, no profile touched");
        }
        r.nm.answers["connection show"] = ok(WIFI_PROFILE);
        auto mode = r.mode();
        mode.begin(1000); // failing answers gone: a configured Pi
        mode.tick(5000);
        check(!r.published().active() && r.nm.count("device wifi hotspot") == 0, "a configured Pi stays on its network");
    }
    {
        Rig r(tmp, "late");
        r.nm.answers["connection show"] = failure();
        auto mode = r.mode();
        mode.begin(1000);
        r.nm.answers["connection show"] = ok("lo:loopback\n");
        mode.tick(1000 + SetupMode::RETRY_S - 1);
        check(!r.published().active(), "a profile check that failed is asked again only after the retry delay");
        mode.tick(1000 + SetupMode::RETRY_S);
        State s = r.published();
        check(s.phase == Phase::Hotspot && s.deadline == 0 && r.nm.count("device wifi hotspot") == 1,
              "once NetworkManager says there is no WiFi, setup starts and stays up until one is joined");
    }
    {
        Rig r(tmp, "hotspot-fails");
        r.nm.answers["connection show"] = ok("lo:loopback\n");
        r.nm.answers["device wifi hotspot"] = failure("Error: Failed to start hotspot");
        auto mode = r.mode();
        mode.begin(1000);
        check(r.published().phase == Phase::Starting && r.published().message.contains("did not start") &&
                  r.published().message.contains("Failed to start hotspot"),
              "a hotspot that did not start is never published as up: the screen says why");
        r.nm.answers["device wifi hotspot"] = ok();
        mode.tick(1000 + SetupMode::RETRY_S);
        check(r.published().phase == Phase::Hotspot, "with no WiFi to go back to, the hotspot is tried again");
    }
    {
        Rig r(tmp, "hotspot-autoconnect");
        r.nm.answers["connection show"] = ok("lo:loopback\n");
        r.nm.answers["connection modify"] = failure("cannot modify");
        auto mode = r.mode();
        mode.begin(1000);
        check(r.published().phase == Phase::Starting && r.published().message.contains("cannot modify") &&
                  r.nm.count("connection delete") > 0,
              "a hotspot that could come back at boot is taken down, not published");
    }
    {
        Rig r(tmp, "asked-hotspot-fails");
        r.nm.answers["connection show"] = ok(WIFI_PROFILE);
        r.nm.answers["device wifi hotspot"] = failure();
        auto mode = r.mode();
        mode.begin(1000);
        std::ofstream(r.request) << "now\n";
        mode.tick(1001);
        check(!r.published().active() && r.nm.count("device connect") == 1,
              "a hotspot that cannot start, asked for on a configured Pi, leaves it on its WiFi");
    }
    {
        Rig r(tmp, "joined");
        r.nm.answers["connection show"] = ok(WIFI_PROFILE);
        r.nm.answers["device show"] = ok("192.168.1.196/24\n");
        auto mode = r.mode();
        mode.begin(1000);
        std::ofstream(r.request) << "now\n";
        mode.tick(1001);
        check(r.published().phase == Phase::Hotspot && r.published().deadline == 1001 + 600, "setup on request");
        r.submit();
        mode.tick(1002);
        State s = r.published();
        check(s.phase == Phase::Joined && s.message == "192.168.1.196" && s.warning.empty() && r.loginAsked.size() == 1,
              "joined with an address, the login applied");
    }
    {
        Rig r(tmp, "no-address");
        r.nm.answers["connection show"] = ok(WIFI_PROFILE);
        r.nm.answers["device show"] = ok("\n");
        auto mode = r.mode();
        mode.begin(1000);
        std::ofstream(r.request) << "now\n";
        mode.tick(1001);
        r.submit();
        mode.tick(1002);
        State s = r.published();
        check(s.phase == Phase::Hotspot && s.message.contains("no address") && r.loginAsked.empty(),
              "a join that gave no address is a failed join: the hotspot comes back with the reason");
        check(r.nm.count("connection delete") >= 3, "and its profile is removed");
    }
    {
        Rig r(tmp, "address-unreadable");
        r.nm.answers["connection show"] = ok(WIFI_PROFILE);
        r.nm.answers["device show"] = timedOut();
        auto mode = r.mode();
        mode.begin(1000);
        std::ofstream(r.request) << "now\n";
        mode.tick(1001);
        r.submit();
        mode.tick(1002);
        check(r.published().phase == Phase::Hotspot && !r.published().message.empty(),
              "an address that cannot be read is a failed join");
    }
    {
        Rig r(tmp, "wrong-password");
        r.nm.answers["connection show"] = ok(WIFI_PROFILE);
        r.nm.answers["device wifi connect"] = failure("Error: Connection activation failed: Secrets were required");
        auto mode = r.mode();
        mode.begin(1000);
        std::ofstream(r.request) << "now\n";
        mode.tick(1001);
        r.submit("Cafe");
        mode.tick(1002);
        check(r.published().phase == Phase::Hotspot && r.published().message == "could not join Cafe: wrong password?",
              "a wrong WiFi password brings the hotspot back with the reason");
    }
    {
        Rig r(tmp, "join-then-hotspot-fails");
        r.nm.answers["connection show"] = ok(WIFI_PROFILE);
        auto mode = r.mode();
        mode.begin(1000);
        std::ofstream(r.request) << "now\n";
        mode.tick(1001);
        r.nm.answers["device wifi connect"] = failure("Error: No network with SSID 'Cafe' found");
        r.nm.answers["device wifi hotspot"] = failure();
        r.submit("Cafe");
        mode.tick(1002);
        check(!r.published().active() && r.nm.count("device connect") == 1,
              "when the hotspot cannot come back after a failed join, the Pi returns to its WiFi");
    }
    {
        Rig r(tmp, "login-fails");
        r.nm.answers["connection show"] = ok(WIFI_PROFILE);
        r.nm.answers["device show"] = ok("192.168.1.196/24\n");
        r.loginFails = {"login password", "SSH on"};
        auto mode = r.mode();
        mode.begin(1000);
        std::ofstream(r.request) << "now\n";
        mode.tick(1001);
        r.submit();
        mode.tick(1002);
        State s = r.published();
        check(s.phase == Phase::Joined && s.message == "192.168.1.196" && s.warning == "login password, SSH on",
              "joined, with what the form asked of the login that was not done said so");
        mode.tick(1002 + SetupMode::JOINED_SHOWN_S + 1);
        check(!r.published().active(), "and shown for a minute, then the normal screens");
    }
    {
        Rig r(tmp, "timeout");
        r.nm.answers["connection show"] = ok(WIFI_PROFILE);
        auto mode = r.mode(1);
        mode.begin(1000);
        std::ofstream(r.request) << "now\n";
        mode.tick(1001);
        mode.tick(1001 + 61);
        check(!r.published().active() && r.nm.count("device connect") == 1, "an unused hotspot goes back to the WiFi there was");
    }
    {
        // A listing that failed, a setup asked for meanwhile: when it ends, the Pi is asked about again.
        Rig r(tmp, "asked-while-unknown");
        r.nm.answers["connection show"] = failure("Error: NetworkManager is not running.");
        auto mode = r.mode(1);
        mode.begin(1000);
        std::ofstream(r.request) << "now\n";
        mode.tick(1001);
        check(r.published().phase == Phase::Hotspot && r.published().deadline == 1001 + 60,
              "setup asked for while the WiFi could not be listed runs as an ordinary one");
        mode.tick(1001 + SetupMode::RETRY_S + 1);
        check(r.nm.count("connection show") == 1, "and the listing is not asked again while it runs");
        r.nm.answers["connection show"] = ok("lo:loopback\n");
        mode.tick(1001 + 61);
        check(!r.published().active(), "an unused hotspot ends as always");
        mode.tick(1001 + 61 + SetupMode::RETRY_S - 1);
        check(!r.published().active(), "the listing waits its turn");
        mode.tick(1001 + 61 + SetupMode::RETRY_S);
        check(r.published().phase == Phase::Hotspot && r.published().deadline == 0,
              "a Pi that turns out to have no WiFi reaches setup that stays until one is joined");
    }
    {
        Rig r(tmp, "asked-while-unknown-joined");
        r.nm.answers["connection show"] = failure();
        r.nm.answers["device show"] = ok("192.168.1.9/24\n");
        auto mode = r.mode();
        mode.begin(1000);
        std::ofstream(r.request) << "now\n";
        mode.tick(1001);
        r.submit();
        mode.tick(1002);
        mode.tick(1002 + SetupMode::JOINED_SHOWN_S + SetupMode::RETRY_S + 5);
        check(r.published().phase == Phase::Off && r.nm.count("connection show") == 1,
              "a join made meanwhile settles it: no WiFi listing, no new setup");
    }
    {
        Rig r(tmp, "failed-join-and-hotspot");
        r.nm.answers["connection show"] = ok("lo:loopback\n");
        auto mode = r.mode();
        mode.begin(1000);
        r.nm.answers["device wifi connect"] = failure("Error: Connection activation failed: Secrets were required");
        r.nm.answers["device wifi hotspot"] = failure("Error: radio busy");
        r.submit("Cafe");
        mode.tick(1001);
        State s = r.published();
        check(s.phase == Phase::Starting && s.message.contains("could not join Cafe: wrong password?") &&
                  s.message.contains("radio busy") && s.message.contains("trying again"),
              "with no WiFi to return to, a failed join and a hotspot that will not start are both said");
        r.nm.answers["device wifi hotspot"] = ok();
        mode.tick(1001 + SetupMode::RETRY_S);
        s = r.published();
        check(s.phase == Phase::Hotspot && s.message == "could not join Cafe: wrong password?",
              "the hotspot that comes back carries the reason the join failed");
    }
    {
        Rig r(tmp, "unwritable-files");
        r.nm.answers["connection show"] = ok("lo:loopback\n");
        r.networks = tmp + "/no-such-folder/networks";
        auto mode = r.mode();
        mode.begin(1000);
        check(r.published().phase == Phase::Hotspot, "networks that cannot be published do not stop setup");
        Rig q(tmp, "unwritable-state");
        q.nm.answers["connection show"] = ok("lo:loopback\n");
        q.state = tmp + "/no-such-folder/state";
        auto other = q.mode();
        other.begin(1000);
        other.tick(1001);
        check(other.state().phase == Phase::Hotspot, "a state that cannot be published does not stop the helper");
    }
}
