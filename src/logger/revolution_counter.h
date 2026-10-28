// Measurements into revolutions: keeps the last sample (restored from the
// database after a restart), accounts each new one, and records what it adds.
#pragma once

#include "logger/measurement.h"
#include "logger/store.h"

#include <optional>
#include <vector>

namespace purr::logger {

class RevolutionCounter {
public:
    explicit RevolutionCounter(Store& store);

    void setKind(Counter c) { counter_ = c; }
    void onMeasurement(const std::vector<uint8_t>& bytes); // from the D-Bus thread
    void forget();                                          // another sensor: no count across the switch

private:
    Store& store_;
    Counter counter_ = Counter::Crank;
    std::optional<Sample> last_;
    double lastWall_ = 0;
};

} // namespace purr::logger
