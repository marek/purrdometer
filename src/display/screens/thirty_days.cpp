// 30 DAYS: a bar a day, Mondays labelled.

#include "display/screens/stats_views.h"

#include "common/format.h"

#include <algorithm>
#include <format>
#include <numeric>

namespace purr::display {

void draw30Days(StatsCtx& x)
{
    auto days = x.s.days(x.today - 29, x.today);
    std::vector<long long> revs;
    for (auto& [d, r] : days)
        revs.push_back(r);
    long long total = std::accumulate(revs.begin(), revs.end(), 0LL);
    long active = std::ranges::count_if(revs, [](long long r) { return r > 0; });
    Font small = x.font(9);
    auto& c = x.c;

    c.text(3, 1, km(x.wheel.km(total)) + " km", x.font(11, true));
    x.right(W - 3, 2, std::format("avg {}/day · {} active", km(x.wheel.km(total) / 30), active), small);
    if (!total)
        return x.empty("no rotations in the last 30 days");

    const int bx0 = 3, by0 = 18, bx1 = W - 4, by1 = CONTENT_H - 13;
    double slot = bars(c, bx0, by0, bx1, by1, revs, true);
    c.text(bx0 + 1, by0 - 2, "max " + km(x.wheel.km(*std::ranges::max_element(revs))), small);
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
