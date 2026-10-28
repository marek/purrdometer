// The setup page (docs/intents/network/setup.md#the-setup-page): GET and POST
// /setup, only for clients on the hotspot while setup runs. A submission is
// written for the helper, which joins the network and sets the login.
#pragma once

#include "web/site.h"

#include <httplib.h>

#include <string>

namespace purr::web {

// On the hotspot: the address is in `prefix` (10.42.0.), as IPv4 or IPv4-mapped IPv6.
bool onHotspot(const std::string& addr, const std::string& prefix);

void addSetupRoutes(const Site& site, httplib::Server& server);

} // namespace purr::web
