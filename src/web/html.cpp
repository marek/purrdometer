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

std::string thousands(long long n)
{
    std::string digits = std::to_string(n < 0 ? -n : n), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0)
            out += ',';
        out += digits[i];
    }
    return n < 0 ? "-" + out : out;
}

std::string fixed(double v, int decimals) { return std::format("{:.{}f}", v, decimals); }

// Same rule as the e-paper screens: one decimal below 100, whole above.
std::string km(double km)
{
    if (km >= 1000)
        return thousands(std::llround(km));
    return fixed(km, km >= 100 ? 0 : 1);
}

std::string ago(double seconds)
{
    if (seconds < 90)
        return "just now";
    if (seconds < 3600)
        return fixed(seconds / 60, 0) + " min ago";
    if (seconds < 2 * 86400)
        return fixed(seconds / 3600, 0) + " h ago";
    return fixed(seconds / 86400, 0) + " days ago";
}

} // namespace purr::web
