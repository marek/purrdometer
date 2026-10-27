// Made-up history for previews, `make serve` and tests. Never point it at the real database.
#pragma once

#include <string>

namespace purr {

// Replaces `path` with a database in the logger's schema: `days` of runs, a run
// going on right now, and a connected sensor. Deterministic for a given seed.
void makeDemoDb(const std::string& path, int days = 120, unsigned seed = 7);

} // namespace purr
