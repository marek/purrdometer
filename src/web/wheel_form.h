// A wheel profile as the settings page posts it (docs/intents/web/settings.md#wheels).
#pragma once

#include "common/profiles.h"

#include <httplib.h>

#include <string>
#include <vector>

namespace purr::web {

// name, preset (empty for custom) and diameter (empty for the preset's; a bare
// number in `units`) over `p`; problems go to `problems`.
Profile profileFromForm(const httplib::Request& req, Profile p, const std::string& units,
                        std::vector<std::string>& problems);

} // namespace purr::web
