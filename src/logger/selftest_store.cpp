// The logger's SQLite store: totals, wheel switches, days, migrations, and
// writes that fail.

#include "logger/measurement.h"
#include "logger/revolution_counter.h"
#include "logger/selftest.h"
#include "logger/store.h"

#include "common/calendar.h"
#include "common/stats.h"

#include <sqlite3.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <thread>

using namespace purr::logger;

namespace {

// The first column of the first row of a query on another connection to the file; "" for none.
std::string query(const std::string& file, const char* sql)
{
    sqlite3* raw = nullptr;
    sqlite3_open(file.c_str(), &raw);
    std::string out;
    sqlite3_exec(
        raw, sql,
        [](void* o, int n, char** v, char**) {
            for (int i = 0; i < n; ++i)
                *static_cast<std::string*>(o) += std::string(v[i] ? v[i] : "") + (i + 1 < n ? "|" : "");
            return 1; // the first row only
        },
        &out, nullptr);
    sqlite3_close(raw);
    return out;
}

} // namespace

void selftestStore(purr::Check& check)
{
    Store mem(":memory:");
    mem.record(120.5, {Counter::Crank, 5, 0}, 3);
    mem.record(150.0, {Counter::Wheel, 7, 0}, 2);
    check(mem.state("last_wall").value().starts_with("150") && mem.state("last_counter") == "wheel", "state persisted");
    mem.forgetCounter();
    check(!mem.state("last_revs") && !mem.state("last_counter"), "switching sensor forgets the counter");
    mem.sensorHeard("AA:BB:CC:DD:EE:FF", "SPD-BLE1", -60);
    mem.sensorHeard("AA:BB:CC:DD:EE:FF", "SPD-BLE1", std::nullopt, "speed");
    check(true, "sensors recorded");
    std::string file = std::string(std::getenv("PURR_TMP") ? std::getenv("PURR_TMP") : "tmp") + "/logger-selftest.db";
    std::remove(file.c_str());
    {
        Store disk(file);
        disk.record(6000, {Counter::Crank, 10, 0}, 10, 2);
        disk.record(6010, {Counter::Crank, 11, 0}, 1, 0);
    }
    purr::Stats st = purr::loadStats(file, purr::cal::localDay(6000));
    check(st.total == 13 && st.estimated == 2, "totals include estimated turns, shown apart");
    std::remove(file.c_str());

    // A switch of wheel mid-run: every turn stored once, before under the old profile, after under the new.
    {
        Store disk(file);
        RevolutionCounter counter(disk);
        counter.setWakeTurns(0);
        auto crank = [](uint16_t revs, uint16_t t) {
            return std::vector<uint8_t>{0x02, uint8_t(revs), uint8_t(revs >> 8), uint8_t(t), uint8_t(t >> 8)};
        };
        counter.onMeasurement(crank(10, 1000)); // the baseline
        counter.onMeasurement(crank(11, 2000));
        counter.onMeasurement(crank(12, 3000));
        counter.setWheel(2);
        counter.onMeasurement(crank(13, 4000));
        counter.onMeasurement(crank(14, 5000));
    }
    std::time_t now = std::time(nullptr);
    st = purr::loadStats(file, purr::cal::localDay(now), {{1, 3.0}, {2, 4.0}});
    check(st.total == 4 && st.profileRevs[1] == 2 && st.profileRevs[2] == 2,
          "a wheel switch mid-run loses no turn and moves none");
    check(std::fabs(st.totalKm - (2 * 3.0 + 2 * 4.0) / 1000) < 1e-9, "each wheel's rotations on its own circumference");
    std::remove(file.c_str());

    // days: kept equal to minutes across midnight and two wheels, rebuilt for another timezone,
    // and read as the minutes would be.
    {
        auto sql = [&](const char* q) {
            sqlite3* raw = nullptr;
            sqlite3_open(file.c_str(), &raw);
            std::string out;
            sqlite3_exec(
                raw, q,
                [](void* o, int n, char** v, char**) {
                    for (int i = 0; i < n; ++i)
                        *static_cast<std::string*>(o) += std::string(v[i] ? v[i] : "") + (i + 1 < n ? "|" : "\n");
                    return 0;
                },
                &out, nullptr);
            sqlite3_close(raw);
            return out;
        };
        const char* fromMinutes = "SELECT date(minute * 60, 'unixepoch', 'localtime'), wheel, SUM(revs), SUM(estimated) "
                                  "FROM minutes GROUP BY 1, 2 ORDER BY 1, 2";
        const char* fromDays = "SELECT day, wheel, revs, estimated FROM days ORDER BY 1, 2";
        std::time_t base = now - now % 86400;
        {
            Store disk(file);
            disk.record(double(base - 30), {Counter::Crank, 1, 0}, 3, 2, 1);    // before midnight UTC
            disk.record(double(base + 30), {Counter::Crank, 2, 0}, 4, 0, 1);    // after it
            disk.record(double(base + 90), {Counter::Crank, 3, 0}, 5, 0, 2);    // another wheel
            disk.record(double(base + 95), {Counter::Crank, 4, 0}, 1, 0, 2);    // the same minute again
        }
        std::string m = sql(fromMinutes), d = sql(fromDays);
        check(!m.empty() && m == d, "days are kept equal to minutes, across midnight and two wheels");
        sqlite3* raw = nullptr;
        sqlite3_open(file.c_str(), &raw);
        sqlite3_exec(raw, "UPDATE meta SET value = 'Elsewhere/Nowhere' WHERE key = 'days_timezone'; DELETE FROM days",
                     nullptr, nullptr, nullptr);
        sqlite3_close(raw);
        purr::Stats slow = purr::loadStats(file, purr::cal::localDay(now), {{1, 3.0}, {2, 4.0}}); // days not ours: minutes
        { Store reopened(file); }
        check(sql(fromDays) == m, "another timezone rebuilds the days from the minutes");
        purr::Stats fast = purr::loadStats(file, purr::cal::localDay(now), {{1, 3.0}, {2, 4.0}});
        check(fast.total == slow.total && fast.daily == slow.daily && fast.profileRevs == slow.profileRevs &&
                  fast.estimated == slow.estimated && std::fabs(fast.totalKm - slow.totalKm) < 1e-12 && fast.total == 15,
              "reading the days gives what reading the minutes gives");
    }
    std::remove(file.c_str());

    // A version 3 database: minutes keyed by the minute alone.
    {
        sqlite3* raw = nullptr;
        sqlite3_open(file.c_str(), &raw);
        sqlite3_exec(raw,
                     "CREATE TABLE minutes (minute INTEGER PRIMARY KEY, revs INTEGER NOT NULL, "
                     "estimated INTEGER NOT NULL DEFAULT 0);"
                     "INSERT INTO minutes VALUES (100, 7, 2), (101, 5, 0);",
                     nullptr, nullptr, nullptr);
        sqlite3_close(raw);
        Store migrated(file);
        migrated.record(101 * 60 + 5, {Counter::Crank, 1, 0}, 1, 0, 2);
    }
    st = purr::loadStats(file, purr::cal::localDay(6000), {{1, 3.0}, {2, 4.0}});
    check(st.total == 15 && st.estimated == 2 && st.profileRevs[1] == 14 && st.profileRevs[2] == 1,
          "a version 3 database: all history under profile 1, and a minute shared by two wheels");
    std::remove(file.c_str());

    // A write that fails after its transaction began: nothing of it is kept,
    // the connection takes the next write, and no turn is lost or counted twice.
    {
        using namespace std::chrono_literals;
        Store disk(file);
        disk.record(6000, {Counter::Crank, 10, 0}, 0);
        query(file, "CREATE TRIGGER inject BEFORE INSERT ON state WHEN NEW.key = 'last_wall' "
                    "BEGIN SELECT RAISE(ABORT, 'injected'); END");
        bool threw = false;
        try {
            disk.record(6001, {Counter::Crank, 12, 0}, 2, 1, 1, 90);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw, "a failing write throws");
        check(query(file, "SELECT count(*) FROM minutes") == "0" && query(file, "SELECT count(*) FROM days") == "0" &&
                  query(file, "SELECT count(*) FROM samples") == "0" && disk.state("last_revs") == "10" &&
                  !disk.state("rpm") && !disk.state("last_motion"),
              "a failed write leaves the minutes, days, samples and state as they were");
        query(file, "DROP TRIGGER inject");
        disk.record(6002, {Counter::Crank, 12, 0}, 2, 1, 1, 90);
        check(query(file, "SELECT sum(revs), sum(estimated) FROM minutes") == "2|1" && disk.state("last_revs") == "12" &&
                  disk.state("rpm") == "90" && disk.state("last_motion").value().starts_with("6002"),
              "the next write goes through: the failed one did not poison the connection");
    }
    std::remove(file.c_str());

    {
        using namespace std::chrono_literals;
        Store disk(file);
        RevolutionCounter counter(disk);
        counter.setWakeTurns(2);
        counter.connected(false);
        auto crank = [](uint16_t revs, uint16_t t) {
            return std::vector<uint8_t>{0x02, uint8_t(revs), uint8_t(revs >> 8), uint8_t(t), uint8_t(t >> 8)};
        };
        counter.onMeasurement(crank(10, 1000)); // the baseline
        query(file, "CREATE TRIGGER inject BEFORE INSERT ON state WHEN NEW.key = 'last_wall' "
                    "BEGIN SELECT RAISE(ABORT, 'injected'); END");
        std::this_thread::sleep_for(200ms);
        counter.onMeasurement(crank(11, 2000));
        std::this_thread::sleep_for(200ms);
        counter.onMeasurement(crank(12, 3000));
        check(query(file, "SELECT count(*) FROM minutes") == "0" && disk.state("last_revs") == "10",
              "measurements that cannot be stored change nothing in the database");
        query(file, "DROP TRIGGER inject");
        std::this_thread::sleep_for(200ms);
        counter.onMeasurement(crank(13, 4000));
        check(query(file, "SELECT sum(revs), sum(estimated) FROM minutes") == "3|2" && disk.state("last_revs") == "13",
              "the turns of the failed measurements are counted once, by the next, with the wake turns still owed");
        std::this_thread::sleep_for(200ms);
        counter.onMeasurement(crank(14, 5000));
        check(query(file, "SELECT sum(revs), sum(estimated) FROM minutes") == "4|2" &&
                  query(file, "SELECT sum(delta) FROM samples") == "4",
              "the wake turns are added once");
    }
    std::remove(file.c_str());

    // A sensor switch: the old count goes whole or not at all, and a count that could not go is not
    // resumed under the new sensor.
    {
        using namespace std::chrono_literals;
        Store disk(file);
        auto crank = [](uint16_t revs, uint16_t t) {
            return std::vector<uint8_t>{0x02, uint8_t(revs), uint8_t(revs >> 8), uint8_t(t), uint8_t(t >> 8)};
        };
        {
            RevolutionCounter counter(disk);
            counter.setWakeTurns(0);
            counter.onMeasurement(crank(10, 1000));
            std::this_thread::sleep_for(100ms);
            counter.onMeasurement(crank(11, 2000));
            query(file, "CREATE TRIGGER inject BEFORE DELETE ON state WHEN OLD.key = 'last_wall' "
                        "BEGIN SELECT RAISE(ABORT, 'injected'); END");
            bool threw = false;
            try {
                counter.forget();
            } catch (const std::runtime_error&) {
                threw = true;
            }
            check(threw && query(file, "SELECT count(*) FROM state WHERE key LIKE 'last_%' AND key <> 'last_motion'") == "4",
                  "a count that cannot be forgotten throws and is kept whole, all four keys");
            std::this_thread::sleep_for(100ms);
            counter.onMeasurement(crank(12, 3000));
            check(query(file, "SELECT sum(revs) FROM minutes") == "2" && disk.state("last_revs") == "12",
                  "and the old sensor goes on being counted, with nothing lost in between");
            query(file, "DROP TRIGGER inject");
            counter.forget();
            check(!disk.state("last_revs") && !disk.state("last_counter") && !disk.state("last_event_time") &&
                      !disk.state("last_wall"),
                  "forgetting takes all four keys together");
            std::this_thread::sleep_for(100ms);
            counter.onMeasurement(crank(13, 4000)); // the old sensor's last notification, after the switch
            check(query(file, "SELECT sum(revs) FROM minutes") == "2" && !disk.state("last_revs"),
                  "notifications that arrive before the new sensor connects are not counted");
            counter.connected(false);
            counter.onMeasurement(crank(500, 100)); // the new sensor's first count: a baseline
            check(query(file, "SELECT sum(revs) FROM minutes") == "2" && disk.state("last_revs") == "500",
                  "the new sensor's first measurement is a baseline, not a jump from the old one");
        }
        RevolutionCounter restarted(disk);
        std::this_thread::sleep_for(100ms);
        restarted.onMeasurement(crank(501, 1100));
        check(query(file, "SELECT sum(revs) FROM minutes") == "3", "a restart resumes the new sensor's count");
    }
    std::remove(file.c_str());
    {
        using namespace std::chrono_literals;
        Store disk(file);
        RevolutionCounter counter(disk);
        counter.setWakeTurns(0);
        auto crank = [](uint16_t revs, uint16_t t) {
            return std::vector<uint8_t>{0x02, uint8_t(revs), uint8_t(revs >> 8), uint8_t(t), uint8_t(t >> 8)};
        };
        counter.onMeasurement(crank(10, 1000));
        query(file, "CREATE TRIGGER inject BEFORE DELETE ON state WHEN OLD.key = 'last_wall' "
                    "BEGIN SELECT RAISE(ABORT, 'injected'); END");
        try {
            counter.forget();
        } catch (const std::runtime_error&) {
        }
        RevolutionCounter restarted(disk); // what a restart would find: the old sensor's count, still whole
        std::this_thread::sleep_for(100ms);
        restarted.onMeasurement(crank(11, 2000));
        check(query(file, "SELECT sum(revs) FROM minutes") == "1",
              "a count that could not be forgotten is still the old sensor's, which is why the switch waits");
    }
    std::remove(file.c_str());
}
