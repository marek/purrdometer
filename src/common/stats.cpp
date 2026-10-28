#include "common/stats.h"

#include <sqlite3.h>

#include <memory>
#include <unistd.h>

namespace purr {

namespace {

template <typename T>
std::vector<std::pair<cal::Day, T>> everyDay(const std::map<cal::Day, T>& m, cal::Day from, cal::Day to)
{
    std::vector<std::pair<cal::Day, T>> out;
    for (cal::Day d = from; d <= to; ++d) {
        auto it = m.find(d);
        out.emplace_back(d, it == m.end() ? T{} : it->second);
    }
    return out;
}

} // namespace

std::vector<DayCount> Stats::days(cal::Day from, cal::Day to) const { return everyDay(daily, from, to); }

std::vector<DayKm> Stats::kmDays(cal::Day from, cal::Day to) const { return everyDay(dailyKm, from, to); }

std::pair<std::string, std::vector<DayKm>> Stats::kmBuckets(cal::Day today, int maxBars) const
{
    if (!firstDay)
        return {"day", {}};
    long span = today - *firstDay + 1;
    if (span <= maxBars)
        return {"day", kmDays(*firstDay, today)};
    bool weekly = span <= 7L * maxBars;
    std::map<cal::Day, double> grouped;
    for (auto [d, km] : kmDays(*firstDay, today))
        grouped[weekly ? d - cal::weekday(d) : cal::firstOfMonth(d)] += km;
    return {weekly ? "week" : "month", {grouped.begin(), grouped.end()}};
}

Stats loadStats(const std::string& dbPath, cal::Day today, const std::map<int, double>& circumferences)
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
            return false; // no schema yet, or an older one
        while (sqlite3_step(st) == SQLITE_ROW)
            row(st);
        sqlite3_finalize(st);
        return true;
    };
    auto str = [](sqlite3_stmt* st, int i) {
        auto t = sqlite3_column_text(st, i);
        return t ? std::string(reinterpret_cast<const char*>(t)) : std::string();
    };

    auto circumference = [&](int wheel) {
        auto it = circumferences.find(wheel);
        return it != circumferences.end() ? it->second : circumferences.empty() ? 0.0 : circumferences.begin()->second;
    };
    auto day = [&](sqlite3_stmt* st) {
        long long revs = sqlite3_column_int64(st, 2);
        int wheel = sqlite3_column_int(st, 1);
        if (revs <= 0)
            return;
        cal::Day d = cal::fromIso(str(st, 0));
        s.daily[d] += revs;
        s.dailyKm[d] += double(revs) * circumference(wheel) / 1000;
        s.profileRevs[wheel] += revs;
        s.estimated += sqlite3_column_int64(st, 3);
    };
    // Schema 4 keys rotations by wheel profile; older databases are all profile 1.
    if (!query("SELECT date(minute * 60, 'unixepoch', 'localtime'), wheel, SUM(revs + estimated), SUM(estimated) "
               "FROM minutes GROUP BY 1, 2",
               day) &&
        !query("SELECT date(minute * 60, 'unixepoch', 'localtime'), 1, SUM(revs + estimated), SUM(estimated) "
               "FROM minutes GROUP BY 1",
               day))
        query("SELECT date(minute * 60, 'unixepoch', 'localtime'), 1, SUM(revs), 0 FROM minutes GROUP BY 1", day);
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
    for (auto& [d, km] : s.dailyKm)
        s.totalKm += km;
    if (auto it = s.daily.find(today); it != s.daily.end())
        s.today = it->second;
    if (auto it = s.dailyKm.find(today); it != s.dailyKm.end())
        s.todayKm = it->second;
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
