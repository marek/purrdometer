#include "display/fonts.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdexcept>

namespace purr::display {

std::vector<char32_t> decodeUtf8(std::string_view s)
{
    std::vector<char32_t> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        // Continuation bytes that follow the lead byte.
        int n = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        char32_t cp = n <= 0 ? c : c & (0x3F >> n);
        bool ok = n >= 0;
        for (int k = 1; ok && k <= n; ++k) {
            ok = i + k < s.size() && (static_cast<unsigned char>(s[i + k]) & 0xC0) == 0x80;
            if (ok)
                cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        }
        out.push_back(ok ? cp : U'\uFFFD');
        i += ok ? size_t(n) + 1 : 1;
    }
    return out;
}

Fonts::Fonts(std::string dir)
{
    if (FT_Init_FreeType(&library_))
        throw std::runtime_error("FreeType did not start");
    if (FT_New_Face(library_, (dir + "/DejaVuSans.ttf").c_str(), 0, &regular_) ||
        FT_New_Face(library_, (dir + "/DejaVuSans-Bold.ttf").c_str(), 0, &bold_))
        throw std::runtime_error("DejaVu Sans not found in " + dir);
}

Fonts::~Fonts()
{
    if (regular_)
        FT_Done_Face(regular_);
    if (bold_)
        FT_Done_Face(bold_);
    if (library_)
        FT_Done_FreeType(library_);
}

FT_Face Fonts::face(const Font& f)
{
    FT_Face face = f.bold ? bold_ : regular_;
    FT_Set_Pixel_Sizes(face, 0, FT_UInt(f.size));
    return face;
}

} // namespace purr::display
