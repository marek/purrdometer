#include "display/preview.h"

#include "common/calendar.h"
#include "common/stats.h"
#include "display/png.h"
#include "display/screens/layout.h"
#include "display/screens/stats_screen.h"

#include <cctype>
#include <filesystem>
#include <format>
#include <print>

namespace purr::display {

namespace {

std::string slug(const char* view) // "30 DAYS" -> "30days"
{
    std::string s;
    for (const char* p = view; *p; ++p)
        if (*p != ' ')
            s += char(std::tolower(static_cast<unsigned char>(*p)));
    return s;
}

} // namespace

int writePreviews(const Config& cfg, const std::string& db, const std::string& dir, std::time_t now)
{
    Fonts fonts;
    Stats stats = loadStats(db, cal::localDay(now), circumferences(cfg));
    std::filesystem::create_directories(dir);
    for (int i = 0; i < int(VIEWS.size()); ++i) {
        std::string path = std::format("{}/{}-{}.png", dir, i, slug(VIEWS[i]));
        writePng(render(fonts, stats, cfg.wheel, i, now), path);
        std::println("{}", path);
    }
    return 0;
}

} // namespace purr::display
