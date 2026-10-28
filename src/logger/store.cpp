#include "logger/store.h"

#include "logger/log.h"
#include "logger/statement.h"
#include "common/timezone.h"

#include <sqlite3.h>

#include <chrono>
#include <stdexcept>

namespace purr::logger {

Store::Store(const std::string& path)
{
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
        throw std::runtime_error("sqlite open " + path + ": " + sqlite3_errmsg(db_));
    sqlite3_busy_timeout(db_, 5000);
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA synchronous=NORMAL");
    exec("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)");
    migrate();
    keepDays();
    exec("CREATE TABLE IF NOT EXISTS samples (ts REAL NOT NULL, revs INTEGER NOT NULL, "
         "event_time INTEGER NOT NULL, delta INTEGER NOT NULL)");
    exec("CREATE INDEX IF NOT EXISTS samples_ts ON samples(ts)");
    exec("CREATE TABLE IF NOT EXISTS sensors (address TEXT PRIMARY KEY, name TEXT, kind TEXT, "
         "rssi INTEGER, last_seen REAL)");
    exec("CREATE TABLE IF NOT EXISTS state (key TEXT PRIMARY KEY, value TEXT)");
    exec("INSERT OR IGNORE INTO meta VALUES ('schema_version', '5')");
    exec("UPDATE meta SET value = '5' WHERE key = 'schema_version'");
    exec("INSERT OR IGNORE INTO meta VALUES ('created', strftime('%s','now'))");
    exec("DELETE FROM samples WHERE ts < strftime('%s','now') - 30*86400");
}

Store::~Store() { sqlite3_close(db_); }

void Store::exec(const std::string& sql)
{
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "?";
        sqlite3_free(err);
        throw std::runtime_error("sqlite: " + msg + " in: " + sql);
    }
}

void Store::migrate()
{
    constexpr const char* MINUTES = "(minute INTEGER NOT NULL, wheel INTEGER NOT NULL DEFAULT 1, "
                                    "revs INTEGER NOT NULL, estimated INTEGER NOT NULL DEFAULT 0, "
                                    "PRIMARY KEY (minute, wheel))";
    exec(std::string("CREATE TABLE IF NOT EXISTS minutes ") + MINUTES);
    if (hasColumn("minutes", "wheel"))
        return;
    // Versions 2 and 3 keyed minutes by the minute alone: every row is profile 1's.
    std::string estimated = hasColumn("minutes", "estimated") ? "estimated" : "0";
    exec("BEGIN");
    exec(std::string("CREATE TABLE minutes_v4 ") + MINUTES);
    exec("INSERT INTO minutes_v4 (minute, wheel, revs, estimated) SELECT minute, 1, revs, " + estimated +
         " FROM minutes");
    exec("DROP TABLE minutes");
    exec("ALTER TABLE minutes_v4 RENAME TO minutes");
    exec("COMMIT");
    log("database: rotations now kept per wheel profile, all history under profile 1");
}

void Store::keepDays()
{
    exec("CREATE TABLE IF NOT EXISTS days (day TEXT NOT NULL, wheel INTEGER NOT NULL, revs INTEGER NOT NULL, "
         "estimated INTEGER NOT NULL DEFAULT 0, PRIMARY KEY (day, wheel))");
    std::string zone = timezoneKey();
    Statement grouped(db_, "SELECT value FROM meta WHERE key = 'days_timezone'");
    if (grouped.step() == SQLITE_ROW && grouped.column(0) == zone)
        return;
    // A new database, version 4, or another timezone: the days from the minutes, once.
    auto started = std::chrono::steady_clock::now();
    exec("BEGIN");
    exec("DELETE FROM days");
    exec("INSERT INTO days SELECT date(minute * 60, 'unixepoch', 'localtime'), wheel, SUM(revs), SUM(estimated) "
         "FROM minutes GROUP BY 1, 2");
    Statement keep(db_, "INSERT OR REPLACE INTO meta VALUES ('days_timezone', ?1)");
    keep.text(1, zone).step();
    exec("COMMIT");
    log("database: days grouped by ", zone, " in ",
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count(), " ms");
}

bool Store::hasColumn(const char* table, const char* column)
{
    Statement s(db_, (std::string("SELECT name FROM pragma_table_info('") + table + "')").c_str());
    while (s.step() == SQLITE_ROW)
        if (s.column(0) == column)
            return true;
    return false;
}

void Store::setState(const std::string& key, const std::string& value)
{
    std::scoped_lock lock(mutex_);
    Statement s(db_, "INSERT INTO state VALUES (?1, ?2) ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    s.text(1, key).text(2, value).step();
}

void Store::clearState(const std::string& key)
{
    std::scoped_lock lock(mutex_);
    Statement s(db_, "DELETE FROM state WHERE key = ?1");
    s.text(1, key).step();
}

std::optional<std::string> Store::state(const std::string& key)
{
    std::scoped_lock lock(mutex_);
    Statement s(db_, "SELECT value FROM state WHERE key = ?1");
    s.text(1, key);
    if (s.step() == SQLITE_ROW)
        return s.column(0);
    return std::nullopt;
}

void Store::sensorHeard(const std::string& address, const std::string& name, std::optional<int> rssi,
                        std::optional<std::string> kind)
{
    std::scoped_lock lock(mutex_);
    Statement s(db_, "INSERT INTO sensors VALUES (?1, ?2, ?3, ?4, ?5) ON CONFLICT(address) DO UPDATE SET "
                     "name = excluded.name, kind = coalesce(excluded.kind, kind), "
                     "rssi = coalesce(excluded.rssi, rssi), last_seen = excluded.last_seen");
    s.text(1, address).text(2, name).optText(3, kind).optInt(4, rssi).dbl(5, nowSeconds()).step();
}

void Store::record(double ts, Sample cur, uint32_t delta, int estimated, int wheel)
{
    std::scoped_lock lock(mutex_);
    exec("BEGIN");
    if (delta > 0 || estimated > 0) {
        Statement s(db_, "INSERT INTO minutes (minute, wheel, revs, estimated) VALUES (?1, ?2, ?3, ?4) "
                         "ON CONFLICT(minute, wheel) DO UPDATE SET "
                         "revs = revs + excluded.revs, estimated = estimated + excluded.estimated");
        s.i64(1, int64_t(ts) / 60).i64(2, wheel).i64(3, delta).i64(4, estimated).step();
        Statement d(db_, "INSERT INTO days VALUES (date(?1 * 60, 'unixepoch', 'localtime'), ?2, ?3, ?4) "
                         "ON CONFLICT(day, wheel) DO UPDATE SET "
                         "revs = revs + excluded.revs, estimated = estimated + excluded.estimated");
        d.i64(1, int64_t(ts) / 60).i64(2, wheel).i64(3, delta).i64(4, estimated).step();
    }
    if (delta > 0) {
        Statement r(db_, "INSERT INTO samples VALUES (?1, ?2, ?3, ?4)");
        r.dbl(1, ts).i64(2, cur.revs).i64(3, cur.eventTime).i64(4, delta).step();
    }
    setState("last_counter", counterName(cur.counter));
    setState("last_revs", std::to_string(cur.revs));
    setState("last_event_time", std::to_string(cur.eventTime));
    setState("last_wall", std::to_string(ts));
    exec("COMMIT");
}

void Store::forgetCounter()
{
    for (const char* key : {"last_counter", "last_revs", "last_event_time", "last_wall"})
        clearState(key);
}

} // namespace purr::logger
