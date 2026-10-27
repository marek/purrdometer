# The web dashboard

> Owns `purrdometer-web`: what it serves, what the page shows, and how light
> it must stay. How its pages are written is owned by [pages.md](pages.md).

## Requirement

The same history as the e-paper screens, in a browser on the home network,
from a process light enough for a Pi Zero 2 W: C++, no interpreter, a few MB
of memory, a page in tens of milliseconds. Read-only over the database
([../data/storage.md](../data/storage.md)); it never writes anything.

## Stack

| Part | Choice | Why |
|---|---|---|
| Language | C++26, like the logger | Native and small: a few MB on a 416 MB Pi |
| HTTP | cpp-httplib (Debian `libcpp-httplib`) | A small compiled library from apt; a fixed pool of 2 worker threads |
| Pages | `.page` files compiled to C++ at build time ([pages.md](pages.md)) | No runtime template engine; a page can call any C++ function |
| JSON | nlohmann-json (header-only, from apt) | |
| Wheels | `data/wheels.ini`, the same file the display reads ([../data/wheels.md](../data/wheels.md)) | One owner for every preset |

## Routes

| Route | Serves |
|---|---|
| `/` | The dashboard |
| `/api/stats` | JSON: the wheel, totals, today, live state, every day with rotations |
| `/healthz` | `ok` |

## The page

| Part | Shows |
|---|---|
| Cards | Total rotations and the first day; total km, active days, best day; today's km and rotations; while running, km/h and rpm, otherwise when the last run was; sensor connection and battery |
| Last 30 days | A bar per day, today in a lighter shade, Mondays labelled; total, daily average, active days |
| Calendar | The last 53 weeks, Monday first, each day shaded in four steps by its share of the busiest day shown |
| All time | Bars per day, week or month (the same bucketing as the e-paper ALL TIME screen) with a line of cumulative distance |
| Footer | The wheel and its distance per rotation, the JSON link |
| Config errors | Shown at the top when the wheel config is invalid; the page then uses the default wheel |

Every bar and cell has a tooltip with its date and distance. The page reloads
itself every 5 s while the wheel turns and every 30 s otherwise; it follows
the browser's light or dark preference.

## Rules

| Subject | Rule |
|---|---|
| Config | `[web] bind` (default `0.0.0.0`) and `port` (default 8080) in `purrdometer.conf`; `[wheel]` as for the display |
| Access | No login: it is for the home network. `bind = 127.0.0.1` keeps it local |
| Service | `purrdometer-web.service` as `purrdometer`, with a read-only system, no devices, `MemoryMax=64M`, `CPUQuota=50%` |
| Errors | A failing request logs the path and the exception and answers 500; the process keeps serving |
| Proven by | `purrdometer-web --selftest`: lengths and units, the presets, calendar arithmetic, escaping, number formats, a real database through the whole page and the JSON |
