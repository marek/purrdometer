// The four tabs' drawing, each in its own file; stats_screen.cpp picks one.
#pragma once

#include "display/screens/kit.h"

namespace purr::display {

void drawNow(StatsCtx& x);       // now.cpp: the totals, today, the wheel turning
void draw30Days(StatsCtx& x);    // thirty_days.cpp: a bar a day
void drawCalendar(StatsCtx& x);  // calendar.cpp: weeks of shaded days
void drawAllTime(StatsCtx& x);   // all_time.cpp: buckets and the running total

} // namespace purr::display
