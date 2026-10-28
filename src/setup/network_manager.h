// The WiFi through NetworkManager's nmcli: the profiles there are, a scan, the
// setup hotspot, joining a network.
#pragma once

#include "common/setup_form.h"
#include "common/setup_state.h"

#include <expected>
#include <string>
#include <vector>

namespace purr::setup {

class NetworkManager {
public:
    static constexpr const char* HOTSPOT_PROFILE = "purrdometer-setup";
    static constexpr const char* JOINED_PREFIX = "purrdometer-wifi-";
    static constexpr int JOIN_WAIT_S = 45;

    explicit NetworkManager(std::string iface = "wlan0") : iface_(std::move(iface)) {}

    bool hasWifiProfile();
    std::vector<Network> scan();                // the radio cannot scan while it is a hotspot
    std::string hotspotName() const;            // purrdometer- and the MAC's last four hex digits
    bool startHotspot(const std::string& ssid, const std::string& password);
    void stopHotspot();
    void reconnect();                           // back to whichever WiFi profile NetworkManager prefers

    // Joins and keeps the profile; on failure removes it and says why.
    std::expected<std::string, std::string> join(const Apply& a); // the address it got

private:
    std::string iface_;
};

} // namespace purr::setup
