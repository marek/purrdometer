// NOW: the total, today, and the wheel while it turns.

#include "display/screens/stats_views.h"

#include "common/format.h"

#include <algorithm>
#include <format>
#include <numeric>

namespace purr::display {

void drawNow(StatsCtx& x)
{
    constexpr int SPLIT = 150;
    Font small = x.font(9), label = x.font(9, true);
    auto& c = x.c;

    c.text(4, 1, "ROTATIONS", label);
    std::string big = thousands(x.s.total);
    c.text(3, 11, big, x.fit(big, SPLIT - 8, 32));
    std::string total = km(x.s.totalKm) + " km";
    c.text(4, 50, total, x.fit(total, SPLIT - 8, 22));
    if (x.s.firstDay)
        c.text(4, 77, "since " + cal::longDate(*x.s.firstDay), small);

    c.line(SPLIT, 3, SPLIT, CONTENT_H - 3);
    const int col = SPLIT + 6;
    c.text(col, 1, "TODAY", label);
    std::string today = km(x.s.todayKm) + " km";
    c.text(col, 12, today, x.fit(today, W - col - 3, 18));
    c.text(col, 34, thousands(x.s.today) + " rotations", small);

    const Live& live = x.s.live;
    if (live.running(double(x.now))) {
        c.text(col, 52, "▶ RUNNING", label);
        c.text(col, 63, std::format("{:.1f} km/h", x.wheel.kmh(live.rpm)), x.font(12, true));
        c.text(col, 78, std::format("{} rpm", live.rpm), small);
    } else if (live.lastMotion) {
        c.text(col, 52, "LAST RUN", label);
        c.text(col, 63, ago(double(x.now) - *live.lastMotion), small);
    }

    std::string status = live.connected ? "sensor ●" : "sensor ○";
    if (live.battery)
        status += std::format(" {}%", *live.battery);
    x.right(W - 3, CONTENT_H - 12, status, small);
}

} // namespace purr::display
