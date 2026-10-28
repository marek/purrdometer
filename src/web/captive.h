// Setup mode's captive redirect (docs/intents/network/setup.md#the-setup-page):
// while setup runs, a hotspot client asking for anything but /setup (a phone's
// connectivity check, any site: on the hotspot every name is the Pi) is sent to
// the setup page. Everyone else is served as always.
#pragma once

#include "web/site.h"

#include <httplib.h>

namespace purr::web {

void addCaptiveRedirect(const Site& site, httplib::Server& server);

} // namespace purr::web
