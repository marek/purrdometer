// Measurements into revolutions: keeps the last sample (restored from the
// database after a restart), accounts each new one, and records what it adds.
#pragma once

#include "logger/measurement.h"
#include "logger/missed_turns.h"
#include "logger/raw_log.h"
#include "logger/store.h"

#include <mutex>
#include <optional>
#include <vector>

namespace purr::logger {

class RevolutionCounter {
public:
    explicit RevolutionCounter(Store& store); // tests and callers that do not need the optional raw log
    RevolutionCounter(Store& store, RawLog& rawLog);

    void setKind(Counter c);
    void setWakeTurns(int wakeTurns);
    void setWheel(int profile);                             // rotations from now on are stored under it
    void connected(bool firstSinceStart);                   // the sensor woke (or the logger started)
    void onMeasurement(const std::vector<uint8_t>& bytes); // from the D-Bus thread
    // Another sensor: no count across the switch. Throws, changing nothing, when the stored counter
    // cannot be removed: a restart would otherwise resume the old sensor's count under the new one.
    // Until the next connection() the measurements that arrive are not counted.
    void forget();

private:
    Store& store_;
    RawLog& rawLog_;
    std::mutex mutex_; // measurements arrive on the D-Bus thread, settings on the main one
    MissedTurns missed_;
    int wheel_ = 1;
    bool awaitingSensor_ = false; // forgotten, and the new sensor has not connected
    Counter counter_ = Counter::Crank;
    std::optional<Sample> last_;
    double lastWall_ = 0;
};

} // namespace purr::logger
