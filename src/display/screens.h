// The four e-paper screens and the touch bar (docs/intents/display/screens.md,
// touch-bar.md): 250x122 landscape, the bar along the bottom edge.
#pragma once

#include "common/stats.h"
#include "common/wheel.h"
#include "display/canvas.h"

#include <array>
#include <ctime>
#include <optional>

namespace purr::display {

constexpr int W = 250, H = 122;
constexpr int BAR_H = 17, BAR_Y = H - BAR_H, CONTENT_H = BAR_Y - 1;
constexpr int TAP_SLACK = 6; // a tap this far above the bar still counts
constexpr std::array<const char*, 4> VIEWS = {"NOW", "30 DAYS", "CALENDAR", "ALL TIME"};

Canvas render(Fonts& fonts, const Stats& stats, const Wheel& wheel, int view, std::time_t now);

// Which tab a tap at landscape (x, y) hit, if it was on the bar.
std::optional<int> tabAt(int x, int y);

} // namespace purr::display
