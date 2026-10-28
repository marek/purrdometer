#include "web/html.h"

#include <cctype>
#include <cmath>
#include <format>

namespace purr::web {

std::string escape(std::string_view s)
{
    std::string out;
    escapeInto(out, s);
    return out;
}

std::string urlEncode(std::string_view s)
{
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out += char(c);
        else
            out += std::format("%{:02X}", c);
    }
    return out;
}

} // namespace purr::web
