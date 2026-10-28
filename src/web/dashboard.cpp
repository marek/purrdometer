#include "web/dashboard.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace purr {

namespace {

std::vector<Bar> bars(const std::vector<DayKm>& rows, cal::Day today, const std::string& unit)
{
    double peak = 0, total = 0;
    for (auto& [d, k] : rows) {
        peak = std::max(peak, k);
        total += k;
    }
    if (peak <= 0)
        peak = 1;
    std::vector<Bar> out;
    double acc = 0;
    for (auto& [d, k] : rows) {
        acc += k;
        Bar b;
        if (unit == "day") {
            b.label = cal::weekdayAbbr(d) + " " + cal::monthDay(d);
            b.tick = cal::weekday(d) == 0 ? cal::monthDay(d) : "";
            b.today = d == today;
        } else if (unit == "week") {
            b.label = "week of " + cal::longDate(d);
            b.tick = cal::dayOf(d) <= 7 ? cal::monthAbbr(d) : "";
        } else {
            b.label = std::format("{} {}", cal::monthAbbr(d), cal::yearOf(d));
            b.tick = cal::monthAbbr(d);
        }
        b.km = k;
        b.height = k / peak;
        b.cumulative = total > 0 ? acc / total : 0;
        out.push_back(b);
    }
    return out;
}

} // namespace

Dashboard buildDashboard(const Stats& s, const Wheel& wheel, std::time_t now)
{
    Dashboard v;
    v.wheel = wheel;
    v.now = now;
    cal::Day today = cal::localDay(now);

    v.totalRevs = s.total;
    v.todayRevs = s.today;
    v.totalKm = s.totalKm;
    v.todayKm = s.todayKm;
    v.firstDay = s.firstDay;
    v.activeDays = s.daily.size();
    for (auto& [d, k] : s.dailyKm)
        v.bestKm = std::max(v.bestKm, k);

    v.running = s.live.running(double(now));
    v.connected = s.live.connected;
    v.rpm = s.live.rpm;
    v.kmh = wheel.kmh(s.live.rpm);
    if (s.live.lastMotion)
        v.lastMotionAgo = double(now) - *s.live.lastMotion;
    v.battery = s.live.battery;

    auto last30 = s.kmDays(today - 29, today);
    v.last30 = bars(last30, today, "day");
    for (auto& [d, k] : last30) {
        v.last30Km += k;
        v.last30Active += k > 0;
    }
    v.last30AvgKm = v.last30Km / 30;

    auto [unit, history] = s.kmBuckets(today);
    v.historyUnit = unit;
    v.history = bars(history, today, unit);

    cal::Day start = today - cal::weekday(today) - 7 * (Dashboard::CALENDAR_WEEKS - 1);
    // Shades in turns of the current wheel: a day's rotations when there was one wheel.
    auto turns = [&](double k) { return std::llround(k * 1000 / wheel.circumferenceM()); };
    auto shown = s.kmDays(start, today);
    long long peak = 0;
    for (auto& [d, k] : shown)
        peak = std::max(peak, turns(k));
    for (auto& [d, k] : shown) {
        long long r = turns(k);
        Cell c;
        c.day = d;
        c.km = k;
        c.level = (r && peak) ? std::min(4, 1 + int(4 * r / (peak + 1))) : 0;
        c.week = int((d - start) / 7);
        c.weekday = cal::weekday(d);
        c.today = d == today;
        v.calendar.push_back(c);
    }
    for (int w = 0; w < Dashboard::CALENDAR_WEEKS; ++w) {
        cal::Day monday = start + 7 * w;
        if (cal::dayOf(monday) <= 7)
            v.calendarMonths.emplace_back(w, cal::monthAbbr(monday));
    }
    return v;
}

} // namespace purr
