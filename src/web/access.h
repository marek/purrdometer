// Who may save settings from the web UI (docs/intents/web/settings.md).
#pragma once

#include "common/config.h"

#include <string>

namespace purr::web {

// Loopback, RFC 1918, link-local and IPv6 unique-local addresses, including
// IPv4-mapped IPv6 forms of them.
bool isLanAddress(const std::string& addr);

// A random token for the settings form, checked against a cookie (double submit).
std::string newToken();

// Equal-length, constant-time comparison for the PIN and the form token.
bool sameSecret(const std::string& a, const std::string& b);

} // namespace purr::web
