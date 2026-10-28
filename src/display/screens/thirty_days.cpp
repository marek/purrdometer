// 30 DAYS: a bar a day, Mondays labelled.

#include "display/screens/stats_views.h"

#include "common/format.h"

#include <algorithm>
#include <format>
#include <numeric>

namespace purr::display {

void draw30Days(StatsCtx& x)
{
    auto days = x.s.kmDays(x.today - 29, x.today);
    std::vector<double> dist;
    for (auto& [d, k] : days)
        dist.push_back(k);
    double total = std::accumulate(dist.begin(), dist.end(), 0.0);
    long active = std::ranges::count_if(dist, [](double k) { return k > 0; });
    Font small = x.font(9);
    auto& c = x.c;

    c.text(3, 1, km(total) + " km", x.font(11, true));
    x.right(W - 3, 2, std::format("avg {}/day · {} active", km(total / 30), active), small);
    if (total <= 0)
        return x.empty("no rotations in the last 30 days");

    const int bx0 = 3, by0 = 18, bx1 = W - 4, by1 = CONTENT_H - 13;
    double slot = bars(c, bx0, by0, bx1, by1, dist, true);
    c.text(bx0 + 1, by0 - 2, "max " + km(*std::ranges::max_element(dist)), small);
    for (size_t i = 0; i < days.size(); ++i) {
        cal::Day d = days[i].first;
        std::string label = std::format("{}/{}", cal::monthOf(d), cal::dayOf(d));
        int lx = iround(bx0 + double(i) * slot);
        if (cal::weekday(d) == 0 && lx + Canvas::textWidth(label, small) <= W - 1) { // label Mondays
            c.line(lx, by1 + 1, lx, by1 + 3);
            c.text(lx, by1 + 3, label, small);
        }
    }
}

} // namespace purr::display
