// Everything the dashboard template shows, computed from Stats and the wheel.
#pragma once

#include "common/calendar.h"
#include "common/wheel.h"
#include "web/stats.h"

#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace purr {

struct Bar {
    std::string label;      // tooltip
    std::string tick;       // axis label, may be empty
    double km = 0;
    double height = 0;      // 0..1 of the tallest bar
    double cumulative = 0;  // 0..1 of the total so far
    bool today = false;
};

struct Cell {
    cal::Day day = 0;
    double km = 0;
    int level = 0;          // 0 none, 1..4 by share of the busiest day shown
    int week = 0, weekday = 0;
    bool today = false;
};

struct Dashboard {
    static constexpr int CALENDAR_WEEKS = 53;

    Wheel wheel;
    std::string units = DEFAULT_UNITS; // how wheel sizes are shown
    std::vector<std::string> configErrors;
    std::time_t now = 0;

    long long totalRevs = 0, todayRevs = 0;
    double totalKm = 0, todayKm = 0, bestKm = 0;
    std::optional<cal::Day> firstDay;
    size_t activeDays = 0;

    bool running = false, connected = false;
    int rpm = 0;
    double kmh = 0;
    std::optional<double> lastMotionAgo; // seconds
    std::optional<int> battery;

    std::vector<Bar> last30;
    double last30Km = 0, last30AvgKm = 0;
    int last30Active = 0;

    std::string historyUnit;
    std::vector<Bar> history;

    std::vector<Cell> calendar;
    std::vector<std::pair<int, std::string>> calendarMonths; // week index, "Oct"
};

Dashboard buildDashboard(const Stats& stats, const Wheel& wheel, std::time_t now);

} // namespace purr
