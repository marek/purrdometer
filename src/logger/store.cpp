#include "logger/store.h"

#include "logger/log.h"
#include "logger/statement.h"

#include <sqlite3.h>

#include <stdexcept>

namespace purr::logger {

Store::Store(const std::string& path)
{
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
        throw std::runtime_error("sqlite open " + path + ": " + sqlite3_errmsg(db_));
    sqlite3_busy_timeout(db_, 5000);
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA synchronous=NORMAL");
    exec("CREATE TABLE IF NOT EXISTS minutes (minute INTEGER PRIMARY KEY, revs INTEGER NOT NULL)");
    exec("CREATE TABLE IF NOT EXISTS samples (ts REAL NOT NULL, revs INTEGER NOT NULL, "
         "event_time INTEGER NOT NULL, delta INTEGER NOT NULL)");
    exec("CREATE INDEX IF NOT EXISTS samples_ts ON samples(ts)");
    exec("CREATE TABLE IF NOT EXISTS sensors (address TEXT PRIMARY KEY, name TEXT, kind TEXT, "
         "rssi INTEGER, last_seen REAL)");
    exec("CREATE TABLE IF NOT EXISTS state (key TEXT PRIMARY KEY, value TEXT)");
    exec("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)");
    exec("INSERT OR IGNORE INTO meta VALUES ('schema_version', '2')");
    exec("UPDATE meta SET value = '2' WHERE key = 'schema_version'");
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

void Store::record(double ts, Sample cur, uint32_t delta)
{
    std::scoped_lock lock(mutex_);
    exec("BEGIN");
    if (delta > 0) {
        Statement s(db_, "INSERT INTO minutes VALUES (?1, ?2) "
                         "ON CONFLICT(minute) DO UPDATE SET revs = revs + excluded.revs");
        s.i64(1, int64_t(ts) / 60).i64(2, delta).step();
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
