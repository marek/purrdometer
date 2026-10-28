// Every stats screen rendered to PNG files, with no hardware: make preview
// and make media.
#pragma once

#include "common/config.h"

#include <ctime>
#include <string>

namespace purr::display {

int writePreviews(const Config& cfg, const std::string& db, const std::string& dir, std::time_t now);

} // namespace purr::display
