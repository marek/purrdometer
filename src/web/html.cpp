#include "web/html.h"

#include <cmath>
#include <format>

namespace purr::web {

std::string escape(std::string_view s)
{
    std::string out;
    escapeInto(out, s);
    return out;
}

} // namespace purr::web
