// Reading `nmcli -t` output: fields split on unescaped colons, the scan list,
// the WiFi profiles.
#pragma once

#include "common/setup_state.h"

#include <string>
#include <string_view>
#include <vector>

namespace purr::setup {

std::vector<std::string> splitTerse(std::string_view line);

// `-f SSID,SIGNAL,SECURITY device wifi list`: strongest first, one entry per
// name, hidden networks left out, "--" security as open.
std::vector<Network> parseScan(const std::string& out);

// `-f NAME,TYPE connection show`: the WiFi profiles but the one named `except`.
std::vector<std::string> parseWifiProfiles(const std::string& out, std::string_view except);

// `-g IP4.ADDRESS device show`: the first address, without its prefix length.
std::string parseAddress(const std::string& out);

} // namespace purr::setup
