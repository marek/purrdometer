// The dashboard page and the JSON API over a database of known rotations.

#include "common/calendar.h"
#include "common/config.h"
#include "common/stats.h"
#include "web/dashboard_routes.h"
#include "web/selftest.h"

#include <ctime>
#include <nlohmann/json.hpp>
#include <string>

using namespace purr;

void selftestDashboard(Fixture& f)
{
    purr::Check& check = f.check;
    web::Site& srv = f.srv;
    const std::string& db = f.db;
    Stats st = loadStats(db, cal::localDay(std::time(nullptr)), circumferences(srv.config()));
    check(st.total == 150 && st.today == 100 && st.live.connected && st.live.rpm == 54, "database read");
    std::string html = web::renderDashboard(srv);
    check(html.find("150") != std::string::npos && html.find("Running") != std::string::npos &&
              html.find("battery 85%") != std::string::npos && html.find("<polyline") != std::string::npos &&
              html.find("href=\"/settings\"") != std::string::npos,
          "dashboard renders totals, live state, charts and the gear");
    auto api = nlohmann::json::parse(web::renderApi(srv));
    check(api["total"]["rotations"] == 150 && api["daily"].size() == 2, "JSON API");
}
