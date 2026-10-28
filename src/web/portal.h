// Setup mode's captive portal (docs/intents/network/setup.md#the-setup-page):
// a server on port 80 with the setup page, where anything else redirects to
// /setup while setup runs, and to the dashboard otherwise.
#pragma once

#include "web/site.h"

#include <httplib.h>

namespace purr::web {

void addPortalRoutes(const Site& site, httplib::Server& server);

} // namespace purr::web
