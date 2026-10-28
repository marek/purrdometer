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
    std::string sensor;     // the address in use
    std::string sensorKind; // "cadence" or "speed"

    // The wheel counts as running for 15 s after the last rotation (display/screens.md).
    bool running(double now) const { return rpm > 0 && lastMotion && now - *lastMotion < 15; }
};

using DayCount = std::pair<cal::Day, long long>;
using DayKm = std::pair<cal::Day, double>;

struct Stats {
    std::map<cal::Day, long long> daily; // rotations, only days with rotations
    std::map<cal::Day, double> dailyKm;  // the same days' distance, each profile's rotations on its own wheel
    long long total = 0, today = 0;      // measured plus estimated
    long long estimated = 0;             // of total: turns the sensor missed (docs/intents/data/accounting.md)
    double totalKm = 0, todayKm = 0;
    std::map<int, long long> profileRevs; // rotations stored under each wheel profile
    std::optional<cal::Day> firstDay;
    Live live;

    std::vector<DayCount> days(cal::Day from, cal::Day to) const; // every day, zeros included
    std::vector<DayKm> kmDays(cal::Day from, cal::Day to) const;
    // Since the first day in at most ~maxBars bars: per "day", else "week", else "month".
    std::pair<std::string, std::vector<DayKm>> kmBuckets(cal::Day today, int maxBars = 60) const;
};

// A missing database or schema reads as no history. circumferences: each wheel
// profile's, in metres (docs/intents/data/wheels.md); rotations under a
// profile not in it use the first one's.
Stats loadStats(const std::string& dbPath, cal::Day today, const std::map<int, double>& circumferences = {});

// A CSC sensor the logger has heard (docs/intents/data/storage.md).
struct SeenSensor {
    std::string address, name, kind; // kind: "cadence", "speed" or empty until read
    std::optional<int> rssi;
    double lastSeen = 0;            // unix time
};

// Newest first; empty without a database.
std::vector<SeenSensor> loadSensors(const std::string& dbPath);

} // namespace purr
