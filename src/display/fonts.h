// DejaVu Sans and Sans Bold through FreeType, at any pixel size.
#pragma once

#include "common/paths.h"

#include <string>
#include <string_view>
#include <vector>

struct FT_LibraryRec_;
struct FT_FaceRec_;

namespace purr::display {


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

// UTF-8 to code points; invalid bytes become U+FFFD.
std::vector<char32_t> decodeUtf8(std::string_view s);


} // namespace purr::display
