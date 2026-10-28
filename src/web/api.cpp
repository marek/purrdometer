// GET /api/stats: the totals, today, the live state, the sensor and every
// day, as JSON.

#include "web/dashboard_routes.h"

#include "common/calendar.h"
#include "common/stats.h"

#include <nlohmann/json.hpp>

#include <ctime>

namespace purr::web {

std::string renderApi(const Site& site)
{
    using nlohmann::json;
    std::time_t now = std::time(nullptr);
    Config cfg = site.config();
    Stats st = loadStats(site.paths.db, cal::localDay(now));
    const Wheel& w = cfg.wheel;
    auto orNull = [](const auto& opt) { return opt ? json(*opt) : json(nullptr); };
    json daily = json::array();
    for (auto& [d, r] : st.daily)
        daily.push_back({{"date", cal::iso(d)}, {"rotations", r}, {"km", w.km(r)}});
    json j = {
        {"wheel", {{"name", w.name}, {"diameter_m", w.diameterM}, {"circumference_m", w.circumferenceM()},
                   {"estimated", w.estimated}}},
        {"total", {{"rotations", st.total}, {"km", w.km(st.total)}, {"estimated_rotations", st.estimated}}},
        {"today", {{"rotations", st.today}, {"km", w.km(st.today)}}},
        {"first_day", st.firstDay ? json(cal::iso(*st.firstDay)) : json(nullptr)},
        {"live", {{"running", st.live.running(double(now))}, {"rpm", st.live.rpm}, {"kmh", w.kmh(st.live.rpm)},
                  {"connected", st.live.connected}, {"last_motion", orNull(st.live.lastMotion)},
                  {"battery", orNull(st.live.battery)}}},
        {"sensor", {{"address", st.live.sensor}, {"kind", st.live.sensorKind}}},
        {"daily", daily},
    };
    return j.dump();
}

} // namespace purr::web
