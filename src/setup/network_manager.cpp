#include "setup/network_manager.h"

#include "setup/command.h"
#include "setup/nmcli_parse.h"

#include <cctype>
#include <fstream>
#include <print>

namespace purr::setup {

namespace {

CommandResult nmcli(std::vector<std::string> args, int timeoutS = 60)
{
    args.insert(args.begin(), "nmcli");
    return runCommand(args, "", timeoutS);
}

std::string friendlyReason(const std::string& why)
{
    if (why.find("Secrets were required") != std::string::npos || why.find("802.1X") != std::string::npos)
        return "wrong password?";
    if (why.find("No network with SSID") != std::string::npos)
        return "not in range";
    return why;
}

} // namespace

bool NetworkManager::hasWifiProfile()
{
    return !parseWifiProfiles(nmcli({"-t", "-f", "NAME,TYPE", "connection", "show"}).out, HOTSPOT_PROFILE).empty();
}

std::vector<Network> NetworkManager::scan()
{
    return parseScan(nmcli({"-t", "-f", "SSID,SIGNAL,SECURITY", "device", "wifi", "list", "--rescan", "yes"}).out);
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

bool NetworkManager::startHotspot(const std::string& ssid, const std::string& password)
{
    nmcli({"connection", "delete", HOTSPOT_PROFILE});
    auto r = nmcli({"device", "wifi", "hotspot", "ifname", iface_, "con-name", HOTSPOT_PROFILE, "ssid", ssid,
                    "password", password});
    if (!r.ok())
        std::println(stderr, "hotspot did not start: {}", r.why());
    nmcli({"connection", "modify", HOTSPOT_PROFILE, "connection.autoconnect", "no"});
    return r.ok();
}

void NetworkManager::stopHotspot()
{
    nmcli({"connection", "down", HOTSPOT_PROFILE});
    nmcli({"connection", "delete", HOTSPOT_PROFILE});
}

void NetworkManager::reconnect() { nmcli({"device", "connect", iface_}, JOIN_WAIT_S + 15); }

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
    auto r = nmcli(args, JOIN_WAIT_S + 15);
    if (!r.ok()) {
        nmcli({"connection", "delete", profile});
        std::println(stderr, "could not join {}: {}", a.ssid, r.why());
        return std::unexpected(friendlyReason(r.why()));
    }
    return parseAddress(nmcli({"-g", "IP4.ADDRESS", "device", "show", iface_}).out);
}

} // namespace purr::setup
