// The HTTP server: every group of routes on one cpp-httplib server, two
// worker threads (a Pi Zero serves a handful of browsers at most), and a
// logged 500 for anything a route throws.
#pragma once

#include "web/site.h"

#include <httplib.h>

namespace purr::web {

void addRoutes(const Site& site, httplib::Server& server);

int serve(const Site& site); // until the process ends; 1 if it cannot listen

} // namespace purr::web
