#include "common/demo.h"

#include <sqlite3.h>

#include <cstdio>
#include <ctime>
#include <format>
#include <random>
#include <stdexcept>

namespace purr {

void makeDemoDb(const std::string& path, int days, unsigned seed)
{
    std::remove(path.c_str());
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("cannot create " + path);
    }
    std::mt19937 rnd(seed);
    auto between = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rnd); };
    std::string sql = "BEGIN;"
                      "CREATE TABLE minutes (minute INTEGER PRIMARY KEY, revs INTEGER NOT NULL);"
                      "CREATE TABLE state (key TEXT PRIMARY KEY, value TEXT);";
    auto minute = [&](long m, int revs) { sql += std::format("INSERT OR REPLACE INTO minutes VALUES ({}, {});", m, revs); };

    const long now = long(std::time(nullptr));
    for (int d = days; d >= 0; --d) {
        if (between(0, 99) < 15)
            continue; // a lazy day
        for (int run = between(1, 6); run > 0; --run) {
            long start = (now - d * 86400L - between(0, 86400)) / 60;
            for (int m = between(2, 25); m > 0; --m)
                minute(start + m, between(10, 80));
        }
    }
    for (int m = 0; m < 25; ++m) // running right now, so the live view is consistent
        minute(now / 60 - m, between(40, 60));
    sql += std::format("INSERT INTO state VALUES ('connected', '1'), ('rpm', '54'), ('last_motion', '{}'), "
                       "('battery', '85');COMMIT;",
                       now);

    char* err = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    std::string message = err ? err : "";
    sqlite3_free(err);
    sqlite3_close(db);
    if (rc != SQLITE_OK)
        throw std::runtime_error("demo database: " + message);
}

} // namespace purr
