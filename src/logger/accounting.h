// Revolution accounting (docs/intents/data/accounting.md): how many turns a
// sample adds to the last one, its rpm, and when a counter jumped too far to
// be motion.
#pragma once

#include "logger/measurement.h"

#include <optional>

namespace purr::logger {

// Anything faster than this is treated as a counter reset, not real motion.
// 5 rev/s on a 48" wheel is ~70 km/h.
constexpr double MAX_REVS_PER_SECOND = 5.0;

struct Accounting {
    uint32_t delta = 0;  // revolutions to add
    double rpm = -1;     // < 0: unknown
    bool reset = false;  // counter discontinuity detected
};

// last: the previous sample (if any) and the wall-clock time it was received.
Accounting account(std::optional<Sample> last, double lastWall, Sample cur, double nowWall);

} // namespace purr::logger
