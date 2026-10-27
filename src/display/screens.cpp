#include "display/screens.h"

#include "common/calendar.h"
#include "common/format.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>

namespace purr::display {

namespace {


struct Ctx {
    Canvas& c;
    Fonts& fonts;
    const Stats& s;
    const Wheel& wheel;
    std::time_t now;
    cal::Day today;

    Font font(int size, bool bold = false) { return fonts.get(size, bold); }
    // The largest size from `size` down to 10 at which `text` fits `maxW`.
    Font fit(const std::string& text, int maxW, int size, bool bold = true)
    {
        while (size > 10 && Canvas::textWidth(text, font(size, bold)) > maxW)
            --size;
        return font(size, bold);
    }
    void right(int x, int y, const std::string& text, const Font& f) { c.text(x - Canvas::textWidth(text, f), y, text, f); }
    void empty(const std::string& msg = "no rotations yet")
    {
        Font f = font(11);
        c.text((W - Canvas::textWidth(msg, f)) / 2, CONTENT_H / 2 - 6, msg, f);
    }
};

// Halves to even, as the layout was designed with (Python's round()).
int iround(double v) { return int(std::nearbyint(v)); }

// Bars in the box; the last one hatched when highlightLast. Returns the slot width.
double bars(Canvas& c, int x0, int y0, int x1, int y1, const std::vector<long long>& values, bool highlightLast = false,
            int level = 4)
{
    const double slot = double(x1 - x0 + 1) / double(values.size());
    const long long top = std::max(1LL, *std::ranges::max_element(values));
    for (size_t i = 0; i < values.size(); ++i) {
        int bx0 = iround(x0 + double(i) * slot), bx1 = std::max(bx0, iround(x0 + double(i + 1) * slot) - 2);
        int h = iround(double(y1 - y0) * double(values[i]) / double(top));
        if (values[i] && h == 0)
            h = 1;
        if (!h)
            continue;
        int lvl = highlightLast && i + 1 == values.size() ? 2 : level;
        c.shade(bx0, y1 - h + 1, bx1, y1, lvl);
        if (lvl < 4)
            c.outline(bx0, y1 - h + 1, bx1, y1);
    }
    c.line(x0, y1 + 1, x1, y1 + 1);
    return slot;
}

void drawTabs(Canvas& c, Fonts& fonts, int view)
{
    c.line(0, BAR_Y, W - 1, BAR_Y);
    Font f = fonts.get(9, true);
    const double tabW = double(W) / VIEWS.size();
    for (int i = 0; i < int(VIEWS.size()); ++i) {
        int x0 = iround(i * tabW), x1 = iround((i + 1) * tabW) - 1;
        if (i == view)
            c.fill(x0, BAR_Y, x1, H - 1);
        else if (i)
            c.line(x0, BAR_Y + 3, x0, H - 4);
        int tw = Canvas::textWidth(VIEWS[i], f);
        c.text(x0 + (x1 - x0 - tw) / 2, BAR_Y + 3, VIEWS[i], f, i != view);
    }
}

void drawNow(Ctx& x)
{
    constexpr int SPLIT = 150;
    Font small = x.font(9), label = x.font(9, true);
    auto& c = x.c;

    c.text(4, 1, "ROTATIONS", label);
    std::string big = thousands(x.s.total);
    c.text(3, 11, big, x.fit(big, SPLIT - 8, 32));
    std::string total = km(x.wheel.km(x.s.total)) + " km";
    c.text(4, 50, total, x.fit(total, SPLIT - 8, 22));
    if (x.s.firstDay)
        c.text(4, 77, "since " + cal::longDate(*x.s.firstDay), small);

    c.line(SPLIT, 3, SPLIT, CONTENT_H - 3);
    const int col = SPLIT + 6;
    c.text(col, 1, "TODAY", label);
    std::string today = km(x.wheel.km(x.s.today)) + " km";
    c.text(col, 12, today, x.fit(today, W - col - 3, 18));
    c.text(col, 34, thousands(x.s.today) + " rotations", small);

    const Live& live = x.s.live;
    if (live.running(double(x.now))) {
        c.text(col, 52, "▶ RUNNING", label);
        c.text(col, 63, std::format("{:.1f} km/h", x.wheel.kmh(live.rpm)), x.font(12, true));
        c.text(col, 78, std::format("{} rpm", live.rpm), small);
    } else if (live.lastMotion) {
        c.text(col, 52, "LAST RUN", label);
        c.text(col, 63, ago(double(x.now) - *live.lastMotion), small);
    }

    std::string status = live.connected ? "sensor ●" : "sensor ○";
    if (live.battery)
        status += std::format(" {}%", *live.battery);
    x.right(W - 3, CONTENT_H - 12, status, small);
}

void draw30Days(Ctx& x)
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

void drawCalendar(Ctx& x)
{
    constexpr int PITCH = 12, CELL = 10, TOP = 13, LEFT = 12;
    constexpr int COLS = (W - LEFT) / PITCH;
    const cal::Day start = x.today - cal::weekday(x.today) - 7 * (COLS - 1); // a Monday
    long long peak = 0;
    for (auto& [d, r] : x.s.days(start, x.today))
        peak = std::max(peak, r);
    Font small = x.font(9);
    auto& c = x.c;

    for (auto [row, name] : {std::pair{0, "M"}, {2, "W"}, {4, "F"}, {6, "S"}})
        c.text(1, TOP + row * PITCH, name, small);

    unsigned lastMonth = 0;
    for (int col = 0; col < COLS; ++col) {
        for (int row = 0; row < 7; ++row) {
            cal::Day d = start + col * 7 + row;
            if (d > x.today)
                continue;
            int cx = LEFT + col * PITCH, cy = TOP + row * PITCH;
            auto it = x.s.daily.find(d);
            long long v = it == x.s.daily.end() ? 0 : it->second;
            if (v && peak) {
                int level = std::min(4, 1 + int(4 * v / (peak + 1)));
                c.shade(cx, cy, cx + CELL - 1, cy + CELL - 1, level);
                c.outline(cx, cy, cx + CELL - 1, cy + CELL - 1);
            } else {
                c.point(cx + CELL / 2, cy + CELL / 2);
            }
            if (d == x.today)
                c.outline(cx - 1, cy - 1, cx + CELL, cy + CELL);
        }
        cal::Day monday = start + col * 7;
        if (cal::monthOf(monday) != lastMonth) {
            if (col || cal::dayOf(monday) <= 7)
                c.text(LEFT + col * PITCH, 0, cal::monthAbbr(monday), small);
            lastMonth = cal::monthOf(monday);
        }
    }
}

void drawAllTime(Ctx& x)
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

} // namespace

Canvas render(Fonts& fonts, const Stats& stats, const Wheel& wheel, int view, std::time_t now)
{
    Canvas canvas(W, H);
    Ctx ctx{canvas, fonts, stats, wheel, now, cal::localDay(now)};
    switch (view) {
    case 0: drawNow(ctx); break;
    case 1: draw30Days(ctx); break;
    case 2: drawCalendar(ctx); break;
    default: drawAllTime(ctx); break;
    }
    drawTabs(canvas, fonts, view);
    return canvas;
}

std::optional<int> tabAt(int x, int y)
{
    if (y < BAR_Y - TAP_SLACK)
        return std::nullopt;
    return std::clamp(x * int(VIEWS.size()) / W, 0, int(VIEWS.size()) - 1);
}

} // namespace purr::display
