// A prepared SQLite statement that finalizes itself; binds chain.
#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace purr::logger {

class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db)
    {
        if (sqlite3_prepare_v2(db, sql, -1, &s_, nullptr) != SQLITE_OK)
            throw std::runtime_error(std::string("sqlite prepare: ") + sqlite3_errmsg(db));
    }
    ~Statement() { sqlite3_finalize(s_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement& text(int i, const std::string& v) { sqlite3_bind_text(s_, i, v.c_str(), -1, SQLITE_TRANSIENT); return *this; }
    Statement& optText(int i, const std::optional<std::string>& v) { return v ? text(i, *v) : null(i); }
    Statement& i64(int i, int64_t v) { sqlite3_bind_int64(s_, i, v); return *this; }
    Statement& optInt(int i, std::optional<int> v) { return v ? i64(i, *v) : null(i); }
    Statement& dbl(int i, double v) { sqlite3_bind_double(s_, i, v); return *this; }
    Statement& null(int i) { sqlite3_bind_null(s_, i); return *this; }

    int step()
    {
        int rc = sqlite3_step(s_);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE)
            throw std::runtime_error(std::string("sqlite step: ") + sqlite3_errmsg(db_));
        return rc;
    }
    std::string column(int i) { return reinterpret_cast<const char*>(sqlite3_column_text(s_, i)); }

private:
    sqlite3* db_;
    sqlite3_stmt* s_ = nullptr;
};

} // namespace purr::logger
