#include "display/screens/stats_screen.h"

#include "display/screens/stats_views.h"
#include "display/screens/touch_bar.h"

namespace purr::display {

Canvas render(Fonts& fonts, const Stats& stats, const Wheel& wheel, int view, std::time_t now,
              std::optional<double> hold)
{
    Canvas canvas(W, H);
    StatsCtx ctx{canvas, fonts, stats, wheel, now, cal::localDay(now)};
    switch (view) {
    case 0: drawNow(ctx); break;
    case 1: draw30Days(ctx); break;
    case 2: drawCalendar(ctx); break;
    default: drawAllTime(ctx); break;
    }
    if (hold)
        drawHoldBar(canvas, fonts, *hold);
    else
        drawTabs(canvas, fonts, view);
    return canvas;
}

} // namespace purr::display
