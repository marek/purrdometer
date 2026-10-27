#include "display/png.h"

#include <png.h>

#include <stdexcept>
#include <vector>

namespace purr::display {

void writePng(const Canvas& canvas, const std::string& path, int scale)
{
    const int w = canvas.width() * scale, h = canvas.height() * scale;
    std::vector<png_byte> grey(size_t(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            grey[size_t(y) * w + x] = canvas.black(x / scale, y / scale) ? 0 : 255;

    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = png_uint_32(w);
    image.height = png_uint_32(h);
    image.format = PNG_FORMAT_GRAY;
    if (!png_image_write_to_file(&image, path.c_str(), 0, grey.data(), w, nullptr))
        throw std::runtime_error("cannot write " + path + ": " + image.message);
}

} // namespace purr::display
