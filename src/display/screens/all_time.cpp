// ALL TIME: buckets since the first day and the running total over them.

#include "display/screens/stats_views.h"

#include "common/format.h"

#include <algorithm>
#include <format>
#include <numeric>

namespace purr::display {

void drawAllTime(StatsCtx& x)
{
    if (!x.s.firstDay)
        return x.empty();
    Font small = x.font(9);
    auto& c = x.c;
    long long best = 0;
    for (auto& [d, r] : x.s.daily)
        best = std::max(best, r);
    c.text(3, 1, km(x.wheel.km(x.s.total)) + " km", x.font(11, true));
    x.right(W - 3, 2, std::format("{} active days · best {}", x.s.daily.size(), km(x.wheel.km(best))), small);

    auto [unit, buckets] = x.s.buckets(x.today);
    std::vector<long long> values;
    for (auto& [d, r] : buckets)
        values.push_back(r);

    const int bx0 = 3, by0 = 18, bx1 = W - 4, by1 = CONTENT_H - 13;
    bars(c, bx0, by0, bx1, by1, values, false, 2);
    // Cumulative distance over the bars.
    const double slot = double(bx1 - bx0 + 1) / double(values.size());
    const long long total = std::max(1LL, std::accumulate(values.begin(), values.end(), 0LL));
    std::vector<std::pair<int, int>> points{{bx0, by1}};
    long long acc = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        acc += values[i];
        points.emplace_back(iround(bx0 + double(i + 1) * slot) - 1, by1 - iround(double(by1 - by0) * double(acc) / double(total)));
    }
    c.polyline(points, true, 2);

    c.text(bx0, by1 + 3, cal::longDate(*x.s.firstDay), small);
    x.right(bx1, by1 + 3, std::format("per {} · today", unit), small);
}

} // namespace purr::display
