// A 1-bit drawing surface for the e-paper screens, with HarfBuzz-shaped text.
// Coordinates are inclusive and in landscape pixels; colour true is black.
#pragma once

#include "display/fonts.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace purr::display {

class Canvas {
public:
    Canvas(int width, int height); // all white

    int width() const { return w_; }
    int height() const { return h_; }
    bool black(int x, int y) const { return inside(x, y) && px_[size_t(y) * w_ + x]; }
    bool operator==(const Canvas& other) const = default;

    void point(int x, int y, bool black = true);
    void fill(int x0, int y0, int x1, int y1, bool black = true);
    void outline(int x0, int y0, int x1, int y1, bool black = true);
    void line(int x0, int y0, int x1, int y1, bool black = true, int width = 1);
    void polyline(const std::vector<std::pair<int, int>>& points, bool black = true, int width = 1);
    // An arc of radius r around (cx, cy) from a0 to a1 degrees, 0 = right, counterclockwise.
    void arc(int cx, int cy, int r, double a0, double a1, int width = 1, bool black = true);
    // 0 nothing, 1 light, 2 half, 3 dark, 4 solid: 1-bit dither patterns.
    void shade(int x0, int y0, int x1, int y1, int level);

    // Text with its ascender line at y, laid out by HarfBuzz; returns the advance.
    int text(int x, int y, std::string_view utf8, const Font& font, bool black = true);
    static int textWidth(std::string_view utf8, const Font& font);

private:
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }

    int w_, h_;
    std::vector<unsigned char> px_; // one byte per pixel; 1 is black
};

} // namespace purr::display
