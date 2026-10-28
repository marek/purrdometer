// The touch bar along the bottom: the four tabs, a long press filling, and
// which tab a tap hit.
#pragma once

#include "display/canvas.h"

#include <optional>

namespace purr::display {

void drawTabs(Canvas& c, Fonts& fonts, int view);
void drawHoldBar(Canvas& c, Fonts& fonts, double progress); // "Hold for WiFi setup" and a strip

// Which tab a tap at landscape (x, y) hit, if it was on the bar.
std::optional<int> tabAt(int x, int y);

} // namespace purr::display
