// The settings page's wheels (docs/intents/web/settings.md#wheels): POST
// /settings/wheels with do = edit, new, use or delete, written to
// profiles.conf. ensureProfiles writes the seeded profile 1 to the file, so
// history keeps its wheel when [wheel] in the settings stops being read.
#pragma once

#include "web/site.h"

#include <httplib.h>

namespace purr::web {

// Under a SaveLock.
void ensureProfiles(const Site& site);

void addWheelRoutes(const Site& site, httplib::Server& server);

} // namespace purr::web
