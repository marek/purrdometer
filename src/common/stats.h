// Read-only view of the logger's database (docs/intents/data/storage.md):
// rotations per local day and the live state.
#pragma once

#include "common/calendar.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace purr {

struct Live {
    bool connected = false;
    int rpm = 0;
    std::optional<double> lastMotion; // unix time
    std::optional<int> battery;

    // The wheel counts as running for 15 s after the last rotation (display/screens.md).
    bool running(double now) const { return rpm > 0 && lastMotion && now - *lastMotion < 15; }
};

using DayCount = std::pair<cal::Day, long long>;

struct Stats {
    std::map<cal::Day, long long> daily; // only days with rotations
    long long total = 0, today = 0;
    std::optional<cal::Day> firstDay;
    Live live;

    std::vector<DayCount> days(cal::Day from, cal::Day to) const; // every day, zeros included
    // Since the first day in at most ~maxBars bars: per "day", else "week", else "month".
    std::pair<std::string, std::vector<DayCount>> buckets(cal::Day today, int maxBars = 60) const;
};

// A missing database or schema reads as no history.
Stats loadStats(const std::string& dbPath, cal::Day today);

} // namespace purr
