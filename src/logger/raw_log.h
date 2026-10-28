// The optional, short-lived record of exact sensor traffic for debugging
// (docs/intents/ble/raw-log.md).
#pragma once

#include "logger/accounting.h"
#include "logger/measurement.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace purr::logger {

class RawLog {
public:
    using TimePoint = std::chrono::system_clock::time_point;
    using Clock = std::function<TimePoint()>;

    explicit RawLog(std::string directory, bool enabled = false, int keepDays = 2,
                    Clock clock = std::chrono::system_clock::now);

    // Applies live settings and removes log days outside the new limit. Turning
    // logging off leaves retained files in place.
    void configure(bool enabled, int keepDays);

    void event(const std::string& name, const std::string& detail = "");
    void measurement(const std::vector<uint8_t>& bytes, const std::optional<Sample>& sample,
                     const Accounting& accounting, int estimated);

private:
    void writeLocked(const std::string& message, TimePoint now);
    void pruneLocked(TimePoint now);
    void report(const std::string& message);

    std::string directory_;
    Clock clock_;
    std::mutex mutex_;
    bool enabled_ = false;
    int keepDays_ = 2;
    bool reportedFailure_ = false;
};

} // namespace purr::logger
