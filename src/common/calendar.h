// Local calendar days as integers (days since 1970-01-01), so day arithmetic is
// plain integer arithmetic. Civil conversions from Howard Hinnant's
// chrono-compatible algorithms.
#pragma once

#include <cstdio>
#include <ctime>
#include <string>

namespace purr::cal {

using Day = long;

inline Day fromCivil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = unsigned(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + long(doe) - 719468;
}

struct Civil {
    int y;
    unsigned m, d;
};

inline Civil toCivil(Day z)
{
    z += 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = unsigned(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = int(yoe) + int(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp + (mp < 10 ? 3 : -9);
    return {y + (m <= 2), m, d};
}

// 0 = Monday ... 6 = Sunday. 1970-01-01 was a Thursday.
inline int weekday(Day d) { return int(((d % 7) + 7 + 3) % 7); }

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

inline const char* monthAbbr(unsigned m)
{
    static const char* names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    return names[(m - 1) % 12];
}

inline const char* weekdayAbbr(Day d)
{
    static const char* names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    return names[weekday(d)];
}

inline std::string monthDay(Day d) // "Oct 27"
{
    auto c = toCivil(d);
    return std::string(monthAbbr(c.m)) + " " + std::to_string(c.d);
}

inline std::string longDate(Day d) // "Oct 27, 2026"
{
    auto c = toCivil(d);
    return monthDay(d) + ", " + std::to_string(c.y);
}

inline std::string iso(Day d)
{
    auto c = toCivil(d);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02u-%02u", c.y, c.m, c.d);
    return buf;
}

} // namespace purr::cal
