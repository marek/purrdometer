#include "web/dashboard.h"

#include <algorithm>

namespace purr {

namespace {

std::vector<Bar> bars(const std::vector<DayCount>& rows, const Wheel& wheel, cal::Day today, const std::string& unit)
{
    long long peak = 1, total = 0;
    for (auto& [d, r] : rows) {
        peak = std::max(peak, r);
        total += r;
    }
    std::vector<Bar> out;
    long long acc = 0;
    for (auto& [d, r] : rows) {
        acc += r;
        Bar b;
        auto c = cal::toCivil(d);
        if (unit == "day") {
            b.label = std::string(cal::weekdayAbbr(d)) + " " + cal::monthDay(d);
            b.tick = cal::weekday(d) == 0 ? cal::monthDay(d) : "";
            b.today = d == today;
        } else if (unit == "week") {
            b.label = "week of " + cal::longDate(d);
            b.tick = c.d <= 7 ? cal::monthAbbr(c.m) : "";
        } else {
            b.label = std::string(cal::monthAbbr(c.m)) + " " + std::to_string(c.y);
            b.tick = cal::monthAbbr(c.m);
        }
        b.km = wheel.km(r);
        b.height = double(r) / peak;
        b.cumulative = total ? double(acc) / total : 0;
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
    v.totalKm = wheel.km(s.total);
    v.todayKm = wheel.km(s.today);
    v.firstDay = s.firstDay;
    v.activeDays = s.daily.size();
    for (auto& [d, r] : s.daily)
        v.bestKm = std::max(v.bestKm, wheel.km(r));

    v.running = s.live.running(double(now));
    v.connected = s.live.connected;
    v.rpm = s.live.rpm;
    v.kmh = wheel.kmh(s.live.rpm);
    if (s.live.lastMotion)
        v.lastMotionAgo = double(now) - *s.live.lastMotion;
    v.battery = s.live.battery;

    auto last30 = s.days(today - 29, today);
    v.last30 = bars(last30, wheel, today, "day");
    long long revs30 = 0;
    for (auto& [d, r] : last30) {
        revs30 += r;
        v.last30Active += r > 0;
    }
    v.last30Km = wheel.km(revs30);
    v.last30AvgKm = v.last30Km / 30;

    auto [unit, history] = s.buckets(today);
    v.historyUnit = unit;
    v.history = bars(history, wheel, today, unit);

    cal::Day start = today - cal::weekday(today) - 7 * (Dashboard::CALENDAR_WEEKS - 1);
    auto shown = s.days(start, today);
    long long peak = 0;
    for (auto& [d, r] : shown)
        peak = std::max(peak, r);
    for (auto& [d, r] : shown) {
        Cell c;
        c.day = d;
        c.km = wheel.km(r);
        c.level = (r && peak) ? std::min(4, 1 + int(4 * r / (peak + 1))) : 0;
        c.week = int((d - start) / 7);
        c.weekday = cal::weekday(d);
        c.today = d == today;
        v.calendar.push_back(c);
    }
    for (int w = 0; w < Dashboard::CALENDAR_WEEKS; ++w) {
        auto monday = cal::toCivil(start + 7 * w);
        if (monday.d <= 7)
            v.calendarMonths.emplace_back(w, cal::monthAbbr(monday.m));
    }
    return v;
}

} // namespace purr
