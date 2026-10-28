// CALENDAR: a column a week, days shaded by distance.

#include "display/screens/stats_views.h"

#include "common/format.h"

#include <algorithm>
#include <format>
#include <numeric>

namespace purr::display {

void drawCalendar(StatsCtx& x)
{
    constexpr int PITCH = 12, CELL = 10, TOP = 13, LEFT = 12;
    constexpr int COLS = (W - LEFT) / PITCH;
    const cal::Day start = x.today - cal::weekday(x.today) - 7 * (COLS - 1); // a Monday
    long long peak = 0;
    for (auto& [d, k] : x.s.kmDays(start, x.today))
        peak = std::max(peak, x.turns(k));
    Font small = x.font(9);
    auto& c = x.c;

    for (auto [row, name] : {std::pair{0, "M"}, {2, "W"}, {4, "F"}, {6, "S"}})
        c.text(1, TOP + row * PITCH, name, small);

    unsigned lastMonth = 0;
    for (int col = 0; col < COLS; ++col) {
        for (int row = 0; row < 7; ++row) {
            cal::Day d = start + col * 7 + row;
            if (d > x.today)
                continue;
            int cx = LEFT + col * PITCH, cy = TOP + row * PITCH;
            auto it = x.s.dailyKm.find(d);
            long long v = it == x.s.dailyKm.end() ? 0 : x.turns(it->second);
            if (v && peak) {
                int level = std::min(4, 1 + int(4 * v / (peak + 1)));
                c.shade(cx, cy, cx + CELL - 1, cy + CELL - 1, level);
                c.outline(cx, cy, cx + CELL - 1, cy + CELL - 1);
            } else {
                c.point(cx + CELL / 2, cy + CELL / 2);
            }
            if (d == x.today)
                c.outline(cx - 1, cy - 1, cx + CELL, cy + CELL);
        }
        cal::Day monday = start + col * 7;
        if (cal::monthOf(monday) != lastMonth) {
            if (col || cal::dayOf(monday) <= 7)
                c.text(LEFT + col * PITCH, 0, cal::monthAbbr(monday), small);
            lastMonth = cal::monthOf(monday);
        }
    }
}

} // namespace purr::display
