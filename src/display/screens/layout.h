// The e-paper's landscape geometry: 250x122, the touch bar along the bottom
// edge, four tabs (docs/intents/display/screens.md, touch-bar.md).
#pragma once

#include <array>

namespace purr::display {

constexpr int W = 250, H = 122;
constexpr int BAR_H = 17, BAR_Y = H - BAR_H, CONTENT_H = BAR_Y - 1;
constexpr int TAP_SLACK = 6; // a tap this far above the bar still counts
constexpr std::array<const char*, 4> VIEWS = {"NOW", "30 DAYS", "CALENDAR", "ALL TIME"};

} // namespace purr::display
