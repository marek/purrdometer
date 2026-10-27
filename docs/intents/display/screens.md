# Screens

> Owns the four screens and what each shows. Choosing a screen is owned by
> [touch-bar.md](touch-bar.md); when they redraw by [refresh.md](refresh.md).

## Requirement

At a glance from beside the wheel: how many rotations, how far, and how the
cat's running has gone over the last 30 days and since the start.

## The panel

Waveshare 2.13" Touch e-Paper HAT, V4 panel: 250×122 pixels, black and white,
drawn in landscape. The bottom 17 px row is the touch bar; the screens use
the rest. Text is DejaVu Sans, rendered monochrome and laid out by HarfBuzz
with kerning and hinted advances, so every glyph is a whole number of pixels
wide and small text keeps even gaps; shades are 1-bit dither patterns.

## The screens

| Screen | Shows |
|---|---|
| **NOW** | Total rotations (largest text, shrunk to fit), total km, the date of the first rotation; today's km and rotations; while the wheel turns, `▶ RUNNING` with km/h and rpm, otherwise when the last run was; the sensor's connection and battery |
| **30 DAYS** | One bar per day for the last 30 days, today last and hatched as still in progress; the 30-day total, the daily average, the number of active days, the best day; Mondays labelled `M/D` |
| **CALENDAR** | Weeks as columns, Monday to Sunday as rows, as many weeks as fit (19); each day shaded in four steps by its share of the busiest visible day, a dot for none, today outlined, month names over their first week |
| **ALL TIME** | Bars from the first day to today, by day up to 60 days, by week up to 60 weeks, by month beyond, with a line of cumulative distance over them; total km, active days, best day, the start date |

## Rules

| Subject | Rule |
|---|---|
| Numbers | Rotations with thousands separators. km with one decimal below 100, whole above |
| No history yet | Zeros on NOW; the charts say "no rotations yet" |
| Running | The wheel counts as running for 15 s after the last measurement that added rotations |
| Units | Kilometres and km/h |
| Preview | Every screen renders to PNG from a fake-history database without hardware (`make preview`); the README's pictures are made the same way (`make media`) |
