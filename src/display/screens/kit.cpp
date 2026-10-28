#include "display/screens/kit.h"

#include <algorithm>
#include <cmath>

namespace purr::display {

Font fitFont(Fonts& fonts, const std::string& text, int maxW, int size, bool bold, int min)
{
    while (size > min && Canvas::textWidth(text, fonts.get(size, bold)) > maxW)
        --size;
    return fonts.get(size, bold);
}

int iround(double v) { return int(std::nearbyint(v)); }

double bars(Canvas& c, int x0, int y0, int x1, int y1, const std::vector<double>& values, bool highlightLast, int level)
{
    const double slot = double(x1 - x0 + 1) / double(values.size());
    const double peak = *std::ranges::max_element(values), top = peak > 0 ? peak : 1;
    for (size_t i = 0; i < values.size(); ++i) {
        int bx0 = iround(x0 + double(i) * slot), bx1 = std::max(bx0, iround(x0 + double(i + 1) * slot) - 2);
        int h = iround(double(y1 - y0) * values[i] / top);
        if (values[i] > 0 && h == 0)
            h = 1;
        if (!h)
            continue;
        int lvl = highlightLast && i + 1 == values.size() ? 2 : level;
        c.shade(bx0, y1 - h + 1, bx1, y1, lvl);
        if (lvl < 4)
            c.outline(bx0, y1 - h + 1, bx1, y1);
    }
    c.line(x0, y1 + 1, x1, y1 + 1);
    return slot;
}

void drawStrip(Canvas& c, int x0, int y0, int x1, double progress)
{
    c.outline(x0, y0, x1, y0 + 8);
    int filled = x0 + 1 + int(std::lround((x1 - x0 - 2) * std::clamp(progress, 0.0, 1.0)));
    if (filled > x0 + 1)
        c.fill(x0 + 1, y0 + 1, filled, y0 + 7);
}

void StatsCtx::empty(const std::string& msg)
{
    Font f = font(11);
    c.text((W - Canvas::textWidth(msg, f)) / 2, CONTENT_H / 2 - 6, msg, f);
}

} // namespace purr::display
