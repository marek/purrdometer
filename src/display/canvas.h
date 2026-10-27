// A 1-bit drawing surface for the e-paper screens, with FreeType text.
// Coordinates are inclusive and in landscape pixels; colour true is black.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct FT_LibraryRec_;
struct FT_FaceRec_;

namespace purr::display {

constexpr const char* FONT_DIR = "/usr/share/fonts/truetype/dejavu";

class Fonts;

// One face at one pixel size.
struct Font {
    Fonts* fonts = nullptr;
    bool bold = false;
    int size = 0;
};

// DejaVu Sans and Sans Bold, rendered monochrome: crisp on e-paper.
class Fonts {
public:
    explicit Fonts(std::string dir = FONT_DIR);
    ~Fonts();
    Fonts(const Fonts&) = delete;
    Fonts& operator=(const Fonts&) = delete;

    Font get(int size, bool bold = false) { return {this, bold, size}; }
    FT_FaceRec_* face(const Font& f); // sized for f

private:
    FT_LibraryRec_* library_ = nullptr;
    FT_FaceRec_* regular_ = nullptr;
    FT_FaceRec_* bold_ = nullptr;
};

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

// UTF-8 to code points; invalid bytes become U+FFFD.
std::vector<char32_t> decodeUtf8(std::string_view s);

} // namespace purr::display
