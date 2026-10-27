// Screens as PNG, for `make preview` and the README's pictures.
#pragma once

#include "display/canvas.h"

#include <string>

namespace purr::display {

// Greyscale PNG, each pixel a scale x scale block. Throws std::runtime_error.
void writePng(const Canvas& canvas, const std::string& path, int scale = 3);

} // namespace purr::display
