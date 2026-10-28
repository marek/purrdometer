// The dashboard's old port (docs/intents/web/dashboard.md): any request to it
// is redirected to the same path on the dashboard's port.
#pragma once

#include <httplib.h>

namespace purr::web {

void addLegacyRedirect(int dashboardPort, httplib::Server& server);

} // namespace purr::web
