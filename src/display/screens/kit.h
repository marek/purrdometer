// What the screens draw with: the stats being shown, text fitted to a width,
// bar charts and progress strips.
#pragma once

#include "common/calendar.h"
#include "common/stats.h"
#include "common/wheel.h"
#include "display/canvas.h"
#include "display/screens/layout.h"

#include <ctime>
#include <string>
#include <vector>

namespace purr::display {

// The largest size from `size` down to `min` at which `text` fits `maxW`.
Font fitFont(Fonts& fonts, const std::string& text, int maxW, int size, bool bold = true, int min = 10);

// Halves to even, as the layout was designed with (Python's round()).
int iround(double v);

// Bars in the box; the last one hatched when highlightLast. Returns the slot width.
double bars(Canvas& c, int x0, int y0, int x1, int y1, const std::vector<long long>& values, bool highlightLast = false,
            int level = 4);

// A progress strip: an outline that fills from the left.
void drawStrip(Canvas& c, int x0, int y0, int x1, double progress);

// One stats screen being drawn.
struct StatsCtx {
    Canvas& c;
    Fonts& fonts;
    const Stats& s;
    const Wheel& wheel;
    std::time_t now;
    cal::Day today;

    Font font(int size, bool bold = false) { return fonts.get(size, bold); }
    // The largest size from `size` down to 10 at which `text` fits `maxW`.
    Font fit(const std::string& text, int maxW, int size, bool bold = true) { return fitFont(fonts, text, maxW, size, bold); }
    void right(int x, int y, const std::string& text, const Font& f) { c.text(x - Canvas::textWidth(text, f), y, text, f); }
    void empty(const std::string& msg = "no rotations yet"); // centred where the content goes
};

} // namespace purr::display
