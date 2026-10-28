// The settings page (docs/intents/web/settings.md): GET and POST /settings,
// POST /settings/scan (asks the logger for a scan), POST /settings/reset.
#pragma once

#include "web/site.h"

#include <httplib.h>

namespace purr::web {

void addSettingsRoutes(const Site& site, httplib::Server& server);

} // namespace purr::web
