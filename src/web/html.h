// What <%= %> and <%== %> call in a compiled template, and the number formats
// the pages use. <%= %> takes text and integers; anything else (a double, a
// date) goes through a named formatter so every format is chosen on purpose.
#pragma once

#include <string>
#include <string_view>
#include <type_traits>

namespace purr::web {

inline void escapeInto(std::string& out, std::string_view s)
{
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out += c;
        }
    }
}

inline void text(std::string& out, std::string_view s) { escapeInto(out, s); }
inline void text(std::string& out, const char* s) { escapeInto(out, s); }
inline void text(std::string& out, const std::string& s) { escapeInto(out, s); }
template <typename T, typename = std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
void text(std::string& out, T v)
{
    out += std::to_string(v);
}

inline void raw(std::string& out, std::string_view s) { out += s; }

std::string escape(std::string_view s);
std::string thousands(long long n);          // 223,867
std::string km(double km);                   // 4.8, 857, 1,204
std::string fixed(double v, int decimals);   // SVG coordinates, speeds
std::string ago(double seconds);             // "5 min ago"

} // namespace purr::web
