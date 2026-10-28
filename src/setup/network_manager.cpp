#include "setup/network_manager.h"

#include "setup/command.h"
#include "setup/nmcli_parse.h"

#include <cctype>
#include <fstream>
#include <print>

namespace purr::setup {

namespace {

std::string friendlyReason(const std::string& why)
{
    if (why.find("Secrets were required") != std::string::npos || why.find("802.1X") != std::string::npos)
        return "wrong password?";
    if (why.find("No network with SSID") != std::string::npos)
        return "not in range";
    return why;
}

} // namespace

CommandResult NetworkManager::nmcli(std::vector<std::string> args, std::chrono::milliseconds timeout)
{
    args.insert(args.begin(), "nmcli");
    return run_(args, "", timeout);
}

std::expected<bool, std::string> NetworkManager::hasWifiProfile()
{
    auto r = nmcli({"-t", "-f", "NAME,TYPE", "connection", "show"});
    if (!r.ok())
        return std::unexpected(r.why());
    return !parseWifiProfiles(r.out, HOTSPOT_PROFILE).empty();
}

std::vector<Network> NetworkManager::scan()
{
    auto r = nmcli({"-t", "-f", "SSID,SIGNAL,SECURITY", "device", "wifi", "list", "--rescan", "yes"});
    if (!r.ok()) {
        std::println(stderr, "WiFi scan failed: {}", r.why());
        return {};
    }
    return parseScan(r.out);
}

std::string NetworkManager::hotspotName() const
{
    std::ifstream f("/sys/class/net/" + iface_ + "/address");
    std::string mac, hex;
    std::getline(f, mac);
    for (char c : mac)
        if (c != ':')
            hex += char(std::tolower(static_cast<unsigned char>(c)));
    return "purrdometer-" + (hex.size() >= 4 ? hex.substr(hex.size() - 4) : std::string("setup"));
}

std::expected<void, std::string> NetworkManager::startHotspot(const std::string& ssid, const std::string& password)
{
    nmcli({"connection", "delete", HOTSPOT_PROFILE});
    auto r = nmcli({"device", "wifi", "hotspot", "ifname", iface_, "con-name", HOTSPOT_PROFILE, "ssid", ssid,
                    "password", password});
    if (!r.ok()) {
        std::println(stderr, "hotspot did not start: {}", r.why());
        stopHotspot();
        return std::unexpected(r.why());
    }
    // Started by hand and never again by itself: at boot the Pi must return to its WiFi.
    auto keep = nmcli({"connection", "modify", HOTSPOT_PROFILE, "connection.autoconnect", "no"});
    if (!keep.ok()) {
        std::println(stderr, "hotspot could not be kept from starting at boot: {}", keep.why());
        stopHotspot();
        return std::unexpected("it could not be kept from starting at boot: " + keep.why());
    }
    return {};
}

void NetworkManager::stopHotspot()
{
    nmcli({"connection", "down", HOTSPOT_PROFILE}); // each fails when there is nothing to do, which is as good
    nmcli({"connection", "delete", HOTSPOT_PROFILE});
}

void NetworkManager::reconnect()
{
    auto r = nmcli({"device", "connect", iface_}, std::chrono::seconds(JOIN_WAIT_S + 15));
    if (!r.ok())
        std::println(stderr, "could not reconnect {}: {}", iface_, r.why());
}

std::expected<std::string, std::string> NetworkManager::join(const Apply& a)
{
    std::string profile = JOINED_PREFIX + a.ssid;
    nmcli({"connection", "delete", profile});
    std::vector<std::string> args = {"--wait", std::to_string(JOIN_WAIT_S), "device", "wifi", "connect", a.ssid,
                                     "ifname", iface_, "name", profile};
    if (!a.wifiPassword.empty())
        args.insert(args.end(), {"password", a.wifiPassword});
    if (a.hidden)
        args.insert(args.end(), {"hidden", "yes"});
    auto r = nmcli(args, std::chrono::seconds(JOIN_WAIT_S + 15));
    if (!r.ok()) {
        nmcli({"connection", "delete", profile});
        std::println(stderr, "could not join {}: {}", a.ssid, r.why());
        return std::unexpected(friendlyReason(r.why()));
    }
    auto shown = nmcli({"-g", "IP4.ADDRESS", "device", "show", iface_});
    std::string address = shown.ok() ? parseAddress(shown.out) : "";
    if (address.empty()) {
        // Joined is a network with an address; without one the Pi is not on it in any way that counts.
        nmcli({"connection", "delete", profile});
        std::println(stderr, "joined {} but got no address: {}", a.ssid, shown.ok() ? "none shown" : shown.why());
        return std::unexpected("it gave the Pi no address");
    }
    return address;
}

} // namespace purr::setup
