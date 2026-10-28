#include "display/canvas.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <cstdlib>
#include <stdexcept>

namespace purr::display {

Canvas::Canvas(int width, int height) : w_(width), h_(height), px_(size_t(width) * height, 0) {}

void Canvas::point(int x, int y, bool black)
{
    if (inside(x, y))
        px_[size_t(y) * w_ + x] = black;
}

void Canvas::fill(int x0, int y0, int x1, int y1, bool black)
{
    for (int y = std::max(0, y0); y <= std::min(h_ - 1, y1); ++y)
        for (int x = std::max(0, x0); x <= std::min(w_ - 1, x1); ++x)
            px_[size_t(y) * w_ + x] = black;
}

void Canvas::outline(int x0, int y0, int x1, int y1, bool black)
{
    line(x0, y0, x1, y0, black);
    line(x0, y1, x1, y1, black);
    line(x0, y0, x0, y1, black);
    line(x1, y0, x1, y1, black);
}

void Canvas::line(int x0, int y0, int x1, int y1, bool black, int width)
{
    // Bresenham. A wider line grows below a shallow line and to the side a steep
    // one leans, as Pillow's wide lines do.
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    const bool steep = -dy > dx;
    const int lean = (y1 - y0) * (x1 - x0) < 0 ? -1 : 1;
    for (int x = x0, y = y0;;) {
        for (int k = 0; k < width; ++k)
            steep ? point(x + k * lean, y, black) : point(x, y + k, black);
        if (x == x1 && y == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}

void Canvas::polyline(const std::vector<std::pair<int, int>>& points, bool black, int width)
{
    for (size_t i = 1; i < points.size(); ++i)
        line(points[i - 1].first, points[i - 1].second, points[i].first, points[i].second, black, width);
}

void Canvas::arc(int cx, int cy, int r, double a0, double a1, int width, bool black)
{
    for (int w = 0; w < width; ++w) {
        double rr = r - w, step = 0.5 / std::max(1.0, rr); // about half a pixel apart
        for (double a = a0 * std::numbers::pi / 180; a <= a1 * std::numbers::pi / 180; a += step)
            point(cx + int(std::lround(rr * std::cos(a))), cy - int(std::lround(rr * std::sin(a))), black);
    }
}

void Canvas::shade(int x0, int y0, int x1, int y1, int level)
{
    if (x1 < x0 || y1 < y0 || level <= 0)
        return;
    if (level >= 4)
        return fill(x0, y0, x1, y1);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            bool on = level == 1 ? (x % 2 == 0 && y % 2 == 0) : level == 2 ? (x + y) % 2 == 0 : !(x % 2 == 1 && y % 2 == 1);
            if (on)
                point(x, y);
        }
}

namespace {

// One shaped glyph: its index and its origin, in 26.6 pixels from the pen start.
struct Placed {
    FT_UInt glyph;
    long x, y;
};

constexpr FT_Int32 LOAD_FLAGS = FT_LOAD_DEFAULT | FT_LOAD_TARGET_MONO;

long pixel(long v) { return (v + 32) >> 6; } // 26.6 to the nearest pixel

// HarfBuzz shaping with kerning. Advances are hinted, so each glyph is a whole
// number of pixels wide: small text on a 1-bit panel keeps even gaps, where
// fractional advances leave gaps that alternate between none and two pixels.
std::vector<Placed> shape(FT_Face face, std::string_view utf8, long& advance)
{
    hb_font_t* font = hb_ft_font_create_referenced(face);
    hb_ft_font_set_load_flags(font, LOAD_FLAGS);
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, utf8.data(), int(utf8.size()), 0, int(utf8.size()));
    hb_buffer_guess_segment_properties(buf);
    hb_shape(font, buf, nullptr, 0);
    unsigned n = 0;
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buf, &n);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buf, &n);
    std::vector<Placed> out;
    long x = 0, y = 0;
    for (unsigned i = 0; i < n; ++i) {
        out.push_back({info[i].codepoint, x + pos[i].x_offset, y + pos[i].y_offset});
        x += pos[i].x_advance;
        y -= pos[i].y_advance;
    }
    advance = x;
    hb_buffer_destroy(buf);
    hb_font_destroy(font);
    return out;
}

} // namespace

// The width Pillow's textbbox gives: from the leftmost ink (or the origin) to the
// rightmost ink or the rounded advance, whichever is further.
int Canvas::textWidth(std::string_view utf8, const Font& font)
{
    FT_Face face = font.fonts->face(font);
    long advance = 0, left = 0, right = 0;
    for (const Placed& g : shape(face, utf8, advance)) {
        if (FT_Load_Glyph(face, g.glyph, LOAD_FLAGS | FT_LOAD_RENDER))
            continue;
        long x = pixel(g.x) + face->glyph->bitmap_left;
        left = std::min(left, x);
        right = std::max(right, x + long(face->glyph->bitmap.width));
    }
    return int(std::max(right, pixel(advance)) - left);
}

int Canvas::text(int x, int y, std::string_view utf8, const Font& font, bool black)
{
    FT_Face face = font.fonts->face(font);
    const int baseline = y + int(pixel(face->size->metrics.ascender));
    long advance = 0;
    for (const Placed& g : shape(face, utf8, advance)) {
        if (FT_Load_Glyph(face, g.glyph, LOAD_FLAGS | FT_LOAD_RENDER))
            continue;
        const FT_GlyphSlot slot = face->glyph;
        const FT_Bitmap& bm = slot->bitmap;
        int left = x + int(pixel(g.x)) + slot->bitmap_left, top = baseline - int(pixel(g.y)) - slot->bitmap_top;
        for (unsigned row = 0; row < bm.rows; ++row)
            for (unsigned col = 0; col < bm.width; ++col)
                if (bm.buffer[row * bm.pitch + col / 8] & (0x80 >> (col % 8)))
                    point(left + int(col), top + int(row), black);
    }
    return int(pixel(advance));
}

} // namespace purr::display
