// Number and time formats shared by the screen and the web pages, so both say
// the same thing the same way.
#pragma once

#include <string>

namespace purr {

std::string thousands(long long n);          // 223,867
std::string km(double km);                   // 4.8, 857, 1,204: one decimal below 100
std::string fixed(double v, int decimals);   // 12.4
std::string ago(double seconds);             // "just now", "5 min ago", "3 h ago", "2 days ago"

} // namespace purr
