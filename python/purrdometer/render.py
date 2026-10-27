"""Draw the 250x122 1-bit screens.

Layout: content area on top, touch bar (tabs) along the bottom edge.
"""

from __future__ import annotations

import datetime as dt
import time
from functools import lru_cache

from PIL import Image, ImageDraw, ImageFont

from .data import Stats

W, H = 250, 122
BAR_H = 17
BAR_Y = H - BAR_H
CONTENT_H = BAR_Y - 1
BLACK, WHITE = 0, 1

VIEWS = ["NOW", "30 DAYS", "CALENDAR", "ALL TIME"]
FONT_DIR = "/usr/share/fonts/truetype/dejavu"


@lru_cache(maxsize=None)
def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    return ImageFont.truetype(f"{FONT_DIR}/{name}", size)


def tab_at(x: int, y: int) -> int | None:
    """Which tab a tap hit, or None if it wasn't on the touch bar."""
    if y < BAR_Y - 6:  # a little slack above the bar
        return None
    return min(len(VIEWS) - 1, max(0, x * len(VIEWS) // W))


def render(stats: Stats, view: int, now: float | None = None) -> Image.Image:
    now = now or time.time()
    img = Image.new("1", (W, H), WHITE)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"  # crisp 1-bit glyphs, no anti-aliasing
    [draw_now, draw_30d, draw_calendar, draw_all][view](d, stats, now)
    draw_tabs(d, view)
    return img


# -- helpers -----------------------------------------------------------------

def text_w(d, s, f) -> int:
    l, _, r, _ = d.textbbox((0, 0), s, font=f)
    return r - l


def fit(d, s, max_w, size, bold=True, min_size=10):
    while size > min_size and text_w(d, s, font(size, bold)) > max_w:
        size -= 1
    return font(size, bold)


def right(d, x, y, s, f):
    d.text((x - text_w(d, s, f), y), s, font=f, fill=BLACK)


def fmt_int(n: int) -> str:
    return f"{n:,}"


def fmt_km(km: float) -> str:
    if km >= 1000:
        return f"{km:,.0f}"
    if km >= 100:
        return f"{km:.0f}"
    return f"{km:.1f}"


def ago(seconds: float) -> str:
    if seconds < 90:
        return "just now"
    if seconds < 3600:
        return f"{seconds / 60:.0f} min ago"
    if seconds < 86400 * 2:
        return f"{seconds / 3600:.0f} h ago"
    return f"{seconds / 86400:.0f} days ago"


def shade(d, box, level: int) -> None:
    """Fill box with a 1-bit shade: 0 empty, 1 light, 2 50%, 3 dark, 4 solid."""
    x0, y0, x1, y1 = box
    if x1 < x0 or y1 < y0:
        return
    if level >= 4:
        d.rectangle(box, fill=BLACK)
        return
    if level <= 0:
        return
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            on = {
                1: (x % 2 == 0 and y % 2 == 0),
                2: (x + y) % 2 == 0,
                3: not (x % 2 == 1 and y % 2 == 1),
            }[level]
            if on:
                d.point((x, y), fill=BLACK)


def draw_tabs(d, view: int) -> None:
    d.line((0, BAR_Y, W - 1, BAR_Y), fill=BLACK)
    f = font(9, True)
    tab_w = W / len(VIEWS)
    for i, label in enumerate(VIEWS):
        x0, x1 = round(i * tab_w), round((i + 1) * tab_w) - 1
        if i == view:
            d.rectangle((x0, BAR_Y, x1, H - 1), fill=BLACK)
        elif i:
            d.line((x0, BAR_Y + 3, x0, H - 4), fill=BLACK)
        tw = text_w(d, label, f)
        d.text((x0 + (x1 - x0 - tw) // 2, BAR_Y + 3), label, font=f, fill=WHITE if i == view else BLACK)


def empty(d, msg="no rotations yet") -> None:
    f = font(11)
    d.text(((W - text_w(d, msg, f)) // 2, CONTENT_H // 2 - 6), msg, font=f, fill=BLACK)


def bars(d, box, values, highlight_last=False, level=4):
    """Bar chart in box; returns (slot width, x of first slot)."""
    x0, y0, x1, y1 = box
    n = len(values)
    slot = (x1 - x0 + 1) / n
    top = max(values) or 1
    for i, v in enumerate(values):
        bx0 = round(x0 + i * slot)
        bx1 = max(bx0, round(x0 + (i + 1) * slot) - 2)
        h = round((y1 - y0) * v / top)
        if v and h == 0:
            h = 1
        if h:
            lvl = 2 if (highlight_last and i == n - 1) else level
            shade(d, (bx0, y1 - h + 1, bx1, y1), lvl)
            if lvl < 4:
                d.rectangle((bx0, y1 - h + 1, bx1, y1), outline=BLACK)
    d.line((x0, y1 + 1, x1, y1 + 1), fill=BLACK)
    return slot


# -- views -------------------------------------------------------------------

def draw_now(d, s: Stats, now: float) -> None:
    split = 150
    small, label = font(9), font(9, True)

    d.text((4, 1), "ROTATIONS", font=label, fill=BLACK)
    big = fmt_int(s.total_revs)
    d.text((3, 11), big, font=fit(d, big, split - 8, 32), fill=BLACK)
    km = f"{fmt_km(s.km(s.total_revs))} km"
    d.text((4, 50), km, font=fit(d, km, split - 8, 22), fill=BLACK)
    if s.first_day:
        d.text((4, 77), f"since {s.first_day:%b %-d, %Y}", font=small, fill=BLACK)

    d.line((split, 3, split, CONTENT_H - 3), fill=BLACK)
    x = split + 6
    d.text((x, 1), "TODAY", font=label, fill=BLACK)
    tkm = f"{fmt_km(s.km(s.today_revs))} km"
    d.text((x, 12), tkm, font=fit(d, tkm, W - x - 3, 18), fill=BLACK)
    d.text((x, 34), f"{fmt_int(s.today_revs)} rotations", font=small, fill=BLACK)

    live = s.live
    if live.active(now) and live.rpm > 0:
        d.text((x, 52), "▶ RUNNING", font=label, fill=BLACK)
        d.text((x, 63), f"{s.kmh(live.rpm):.1f} km/h", font=font(12, True), fill=BLACK)
        d.text((x, 78), f"{live.rpm} rpm", font=small, fill=BLACK)
    elif live.last_motion:
        d.text((x, 52), "LAST RUN", font=label, fill=BLACK)
        d.text((x, 63), ago(now - live.last_motion), font=small, fill=BLACK)

    status = "sensor ●" if live.connected else "sensor ○"
    if live.battery is not None:
        status += f" {live.battery}%"
    right(d, W - 3, CONTENT_H - 12, status, small)


def draw_30d(d, s: Stats, now: float) -> None:
    today = dt.date.fromtimestamp(now)
    days = s.days(today - dt.timedelta(29), today)
    revs = [r for _, r in days]
    total = sum(revs)
    small, label = font(9), font(9, True)
    d.text((3, 1), f"{fmt_km(s.km(total))} km", font=font(11, True), fill=BLACK)
    active = sum(1 for r in revs if r)
    right(d, W - 3, 2, f"avg {fmt_km(s.km(total) / 30)}/day · {active} active", small)
    if not total:
        return empty(d, "no rotations in the last 30 days")

    box = (3, 18, W - 4, CONTENT_H - 13)
    slot = bars(d, box, revs, highlight_last=True)
    best = max(revs)
    d.text((box[0] + 1, box[1] - 2), f"max {fmt_km(s.km(best))}", font=small, fill=BLACK)
    for i, (day, _) in enumerate(days):
        label = f"{day.month}/{day.day}"
        x = round(box[0] + i * slot)
        if day.weekday() == 0 and x + text_w(d, label, small) <= W - 1:  # label Mondays
            d.line((x, box[3] + 1, x, box[3] + 3), fill=BLACK)
            d.text((x, box[3] + 3), label, font=small, fill=BLACK)


def draw_calendar(d, s: Stats, now: float) -> None:
    today = dt.date.fromtimestamp(now)
    pitch, cell, top, left = 12, 10, 13, 12
    cols = (W - left) // pitch
    start = today - dt.timedelta(days=today.weekday() + 7 * (cols - 1))  # Monday
    visible = [r for d0, r in s.days(start, today)]
    peak = max(visible) if visible else 0

    small = font(9)
    for row, name in ((0, "M"), (2, "W"), (4, "F"), (6, "S")):
        d.text((1, top + row * pitch), name, font=small, fill=BLACK)

    last_month = None
    for c in range(cols):
        for r in range(7):
            day = start + dt.timedelta(days=c * 7 + r)
            if day > today:
                continue
            x, y = left + c * pitch, top + r * pitch
            v = s.daily.get(day, 0)
            if v and peak:
                level = min(4, 1 + int(4 * v / (peak + 1)))
                shade(d, (x, y, x + cell - 1, y + cell - 1), level)
                d.rectangle((x, y, x + cell - 1, y + cell - 1), outline=BLACK)
            else:
                d.point((x + cell // 2, y + cell // 2), fill=BLACK)
            if day == today:
                d.rectangle((x - 1, y - 1, x + cell, y + cell), outline=BLACK)
        month_start = start + dt.timedelta(days=c * 7)
        if month_start.month != last_month:
            if c or month_start.day <= 7:
                d.text((left + c * pitch, 0), f"{month_start:%b}", font=small, fill=BLACK)
            last_month = month_start.month


def draw_all(d, s: Stats, now: float) -> None:
    small = font(9)
    if not s.first_day:
        return empty(d)
    today = dt.date.fromtimestamp(now)
    d.text((3, 1), f"{fmt_km(s.km(s.total_revs))} km", font=font(11, True), fill=BLACK)
    right(d, W - 3, 2, f"{len(s.daily)} active days · best {fmt_km(s.km(max(s.daily.values())))}", small)

    unit, buckets = s.buckets(today)
    values = [r for _, r in buckets]

    box = (3, 18, W - 4, CONTENT_H - 13)
    bars(d, box, values, level=2)
    # cumulative distance line over the bars
    x0, y0, x1, y1 = box
    slot = (x1 - x0 + 1) / len(values)
    total, acc, pts = sum(values) or 1, 0, []
    for i, v in enumerate(values):
        acc += v
        pts.append((round(x0 + (i + 1) * slot) - 1, y1 - round((y1 - y0) * acc / total)))
    d.line([(x0, y1)] + pts, fill=BLACK, width=2)

    d.text((x0, y1 + 3), f"{s.first_day:%b %-d, %Y}", font=small, fill=BLACK)
    right(d, x1, y1 + 3, f"per {unit} · today", small)
