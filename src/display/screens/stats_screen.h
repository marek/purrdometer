// One of the four stats screens with the touch bar under it.
#pragma once

#include "common/stats.h"
#include "common/wheel.h"
#include "display/canvas.h"

#include <ctime>
#include <optional>

namespace purr::display {

// view: the tab (0..3). hold: a long press's progress (0..1), drawn in place of the tabs.
Canvas render(Fonts& fonts, const Stats& stats, const Wheel& wheel, int view, std::time_t now,
              std::optional<double> hold = std::nullopt);

} // namespace purr::display
