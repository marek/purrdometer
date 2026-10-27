// Local calendar days as integers (days since 1970-01-01), so day arithmetic is
// plain integer arithmetic; conversions go through std::chrono's calendar.
#pragma once

#include <chrono>
#include <cstdio>
#include <ctime>
#include <format>
#include <string>

namespace purr::cal {

using Day = long;

inline std::chrono::year_month_day ymd(Day d)
{
    return std::chrono::year_month_day{std::chrono::sys_days{std::chrono::days{d}}};
}

inline Day fromCivil(int y, unsigned m, unsigned d)
{
    using namespace std::chrono;
    return sys_days{year{y} / month{m} / day{d}}.time_since_epoch().count();
}

// 0 = Monday ... 6 = Sunday.
inline int weekday(Day d)
{
    return int(std::chrono::weekday{std::chrono::sys_days{std::chrono::days{d}}}.iso_encoding()) - 1;
}

inline Day fromIso(const std::string& s) // "YYYY-MM-DD"
{
    int y = 0;
    unsigned m = 0, d = 0;
    std::sscanf(s.c_str(), "%d-%u-%u", &y, &m, &d);
    return fromCivil(y, m, d);
}

inline Day localDay(std::time_t t)
{
    std::tm tm{};
    localtime_r(&t, &tm);
    return fromCivil(tm.tm_year + 1900, unsigned(tm.tm_mon + 1), unsigned(tm.tm_mday));
}

inline int yearOf(Day d) { return int(ymd(d).year()); }
inline unsigned monthOf(Day d) { return unsigned(ymd(d).month()); }
inline unsigned dayOf(Day d) { return unsigned(ymd(d).day()); }
inline Day firstOfMonth(Day d) { return fromCivil(yearOf(d), monthOf(d), 1); }

inline std::string monthAbbr(Day d) { return std::format("{:%b}", ymd(d).month()); }
inline std::string weekdayAbbr(Day d) { return std::format("{:%a}", std::chrono::sys_days{std::chrono::days{d}}); }
inline std::string monthDay(Day d) { return std::format("{:%b} {}", ymd(d).month(), dayOf(d)); } // "Oct 27"
inline std::string longDate(Day d) { return std::format("{}, {}", monthDay(d), yearOf(d)); }   // "Oct 27, 2026"
inline std::string iso(Day d) { return std::format("{:%F}", ymd(d)); }                         // "2026-10-27"

} // namespace purr::cal
