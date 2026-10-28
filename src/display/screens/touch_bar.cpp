#include "display/screens/touch_bar.h"

#include "display/screens/kit.h"

#include <algorithm>

namespace purr::display {

void drawTabs(Canvas& c, Fonts& fonts, int view)
{
    c.line(0, BAR_Y, W - 1, BAR_Y);
    Font f = fonts.get(9, true);
    const double tabW = double(W) / VIEWS.size();
    for (int i = 0; i < int(VIEWS.size()); ++i) {
        int x0 = iround(i * tabW), x1 = iround((i + 1) * tabW) - 1;
        if (i == view)
            c.fill(x0, BAR_Y, x1, H - 1);
        else if (i)
            c.line(x0, BAR_Y + 3, x0, H - 4);
        int tw = Canvas::textWidth(VIEWS[i], f);
        c.text(x0 + (x1 - x0 - tw) / 2, BAR_Y + 3, VIEWS[i], f, i != view);
    }
}

void drawHoldBar(Canvas& c, Fonts& fonts, double progress)
{
    c.fill(0, BAR_Y, W - 1, H - 1, false);
    c.line(0, BAR_Y, W - 1, BAR_Y);
    c.text(4, BAR_Y + 3, "Hold for WiFi setup", fonts.get(9, true));
    drawStrip(c, 128, BAR_Y + 4, W - 4, progress);
}

std::optional<int> tabAt(int x, int y)
{
    if (y < BAR_Y - TAP_SLACK)
        return std::nullopt;
    return std::clamp(x * int(VIEWS.size()) / W, 0, int(VIEWS.size()) - 1);
}

} // namespace purr::display
