// GET / (the dashboard page), GET /api/stats (the same numbers as JSON) and
// GET /healthz.
#pragma once

#include "web/site.h"

#include <httplib.h>

#include <string>

namespace purr::web {

std::string renderDashboard(const Site& site);
std::string renderApi(const Site& site);

void addDashboardRoutes(const Site& site, httplib::Server& server);

} // namespace purr::web
