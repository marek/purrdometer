// The WiFi through NetworkManager's nmcli: the profiles there are, a scan, the
// setup hotspot, joining a network.
#pragma once

#include "common/setup_form.h"
#include "common/setup_state.h"
#include "setup/command.h"

#include <expected>
#include <string>
#include <vector>

namespace purr::setup {

class NetworkManager {
public:
    static constexpr const char* HOTSPOT_PROFILE = "purrdometer-setup";
    static constexpr const char* JOINED_PREFIX = "purrdometer-wifi-";
    static constexpr int JOIN_WAIT_S = 45;

    // `run` runs nmcli; tests give their own.
    explicit NetworkManager(std::string iface = "wlan0", Runner run = runCommand)
        : iface_(std::move(iface)), run_(std::move(run))
    {
    }

    // Whether a WiFi profile other than the hotspot's exists; an error when nmcli could not say,
    // which is not the same as "none".
    std::expected<bool, std::string> hasWifiProfile();
    std::vector<Network> scan();                // the radio cannot scan while it is a hotspot
    std::string hotspotName() const;            // purrdometer- and the MAC's last four hex digits
    // Succeeds only when the hotspot is up and will not come back by itself at boot; otherwise it is
    // removed again and the error says why.
    std::expected<void, std::string> startHotspot(const std::string& ssid, const std::string& password);
    void stopHotspot();
    void reconnect();                           // back to whichever WiFi profile NetworkManager prefers

    // Joins and keeps the profile; on failure removes it and says why. The address it got.
    std::expected<std::string, std::string> join(const Apply& a);

private:
    CommandResult nmcli(std::vector<std::string> args, std::chrono::milliseconds timeout = std::chrono::seconds(60));

    std::string iface_;
    Runner run_;
};

} // namespace purr::setup
