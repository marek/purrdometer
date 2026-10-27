#include "common/stats.h"

#include <sqlite3.h>

#include <memory>
#include <unistd.h>

namespace purr {

std::vector<DayCount> Stats::days(cal::Day from, cal::Day to) const
{
    std::vector<DayCount> out;
    for (cal::Day d = from; d <= to; ++d) {
        auto it = daily.find(d);
        out.emplace_back(d, it == daily.end() ? 0 : it->second);
    }
    return out;
}

std::pair<std::string, std::vector<DayCount>> Stats::buckets(cal::Day today, int maxBars) const
{
    if (!firstDay)
        return {"day", {}};
    long span = today - *firstDay + 1;
    if (span <= maxBars)
        return {"day", days(*firstDay, today)};
    bool weekly = span <= 7L * maxBars;
    std::map<cal::Day, long long> grouped;
    for (auto [d, r] : days(*firstDay, today)) {
        grouped[weekly ? d - cal::weekday(d) : cal::firstOfMonth(d)] += r;
    }
    return {weekly ? "week" : "month", {grouped.begin(), grouped.end()}};
}

Stats loadStats(const std::string& dbPath, cal::Day today)
{
    Stats s;
    if (access(dbPath.c_str(), R_OK) != 0)
        return s;
    sqlite3* raw = nullptr;
    if (sqlite3_open_v2(dbPath.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        sqlite3_close(raw);
        return s;
    }
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
    sqlite3_busy_timeout(db.get(), 5000);

    auto query = [&](const char* sql, auto&& row) {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK)
            return; // no schema yet
        while (sqlite3_step(st) == SQLITE_ROW)
            row(st);
        sqlite3_finalize(st);
    };
    auto str = [](sqlite3_stmt* st, int i) {
        auto t = sqlite3_column_text(st, i);
        return t ? std::string(reinterpret_cast<const char*>(t)) : std::string();
    };

    query("SELECT date(minute * 60, 'unixepoch', 'localtime'), SUM(revs) FROM minutes GROUP BY 1", [&](sqlite3_stmt* st) {
        long long revs = sqlite3_column_int64(st, 1);
        if (revs > 0)
            s.daily[cal::fromIso(str(st, 0))] = revs;
    });
    query("SELECT key, value FROM state", [&](sqlite3_stmt* st) {
        std::string key = str(st, 0), value = str(st, 1);
        try {
            if (key == "connected")
                s.live.connected = value == "1";
            else if (key == "rpm")
                s.live.rpm = std::stoi(value);
            else if (key == "last_motion")
                s.live.lastMotion = std::stod(value);
            else if (key == "battery")
                s.live.battery = std::stoi(value);
            else if (key == "sensor")
                s.live.sensor = value;
            else if (key == "sensor_kind")
                s.live.sensorKind = value;
        } catch (const std::exception&) {
            // a malformed value reads as unknown
        }
    });

    for (auto& [d, r] : s.daily)
        s.total += r;
    if (auto it = s.daily.find(today); it != s.daily.end())
        s.today = it->second;
    if (!s.daily.empty())
        s.firstDay = s.daily.begin()->first;
    return s;
}

std::vector<SeenSensor> loadSensors(const std::string& dbPath)
{
    std::vector<SeenSensor> out;
    if (access(dbPath.c_str(), R_OK) != 0)
        return out;
    sqlite3* raw = nullptr;
    if (sqlite3_open_v2(dbPath.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        sqlite3_close(raw);
        return out;
    }
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
    sqlite3_busy_timeout(db.get(), 5000);
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db.get(), "SELECT address, name, kind, rssi, last_seen FROM sensors ORDER BY last_seen DESC",
                           -1, &st, nullptr) != SQLITE_OK)
        return out; // an older logger: no table yet
    auto text = [&](int i) {
        auto t = sqlite3_column_text(st, i);
        return t ? std::string(reinterpret_cast<const char*>(t)) : std::string();
    };
    while (sqlite3_step(st) == SQLITE_ROW) {
        SeenSensor s{text(0), text(1), text(2), std::nullopt, sqlite3_column_double(st, 4)};
        if (sqlite3_column_type(st, 3) != SQLITE_NULL)
            s.rssi = sqlite3_column_int(st, 3);
        out.push_back(s);
    }
    sqlite3_finalize(st);
    return out;
}

} // namespace purr
