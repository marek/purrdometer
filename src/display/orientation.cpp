#include "display/orientation.h"

namespace purr::display {

std::vector<uint8_t> packPanel(const Canvas& c, bool rotate180)
{
    std::vector<uint8_t> ram(size_t(PANEL_ROW_BYTES) * PANEL_H, 0xFF);
    for (int py = 0; py < PANEL_H; ++py)
        for (int px = 0; px < PANEL_W; ++px) {
            // Landscape is the panel turned a quarter clockwise (anticlockwise when flipped).
            int lx = rotate180 ? PANEL_H - 1 - py : py;
            int ly = rotate180 ? px : PANEL_W - 1 - px;
            if (c.black(lx, ly))
                ram[size_t(py) * PANEL_ROW_BYTES + px / 8] &= uint8_t(~(0x80 >> (px % 8)));
        }
    return ram;
}

std::pair<int, int> touchToScreen(int tx, int ty, bool rotate180)
{
    // Landscape is PANEL_H wide and PANEL_W tall.
    int x = PANEL_H - 1 - ty, y = tx;
    if (rotate180)
        return {PANEL_H - 1 - x, PANEL_W - 1 - y};
    return {x, y};
}

} // namespace purr::display
