// The panel's RAM is 122x250 portrait; screens are drawn 250x122 landscape.
// packPanel() turns a landscape canvas into panel RAM, and touchToScreen()
// turns the touch controller's panel coordinates into landscape ones; the
// touch strip is along the bottom of the landscape screen, on the same edge
// as the button row of Waveshare's demo. rotate180 flips both.
#pragma once

#include "display/canvas.h"
#include "display/epaper.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace purr::display {


// Landscape canvas -> panel RAM bytes: rows of PANEL_ROW_BYTES, MSB first, 1 = white.
std::vector<uint8_t> packPanel(const Canvas& landscape, bool rotate180);
// Touch controller (tx, ty) -> landscape (x, y).
std::pair<int, int> touchToScreen(int tx, int ty, bool rotate180);

} // namespace purr::display
