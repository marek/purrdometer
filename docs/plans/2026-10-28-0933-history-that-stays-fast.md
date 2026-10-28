# History that stays fast

Date: 2026-10-28 09:33 EDT

## Context

Every screen redraw (every 5 s while the wheel turns), every dashboard
refresh (every 5 s per open browser while it turns), every `/api/stats`
(every second during the settings page's live test) and the settings page
read the whole `minutes` table and turn each row into a local day. That cost
grows with the history, on a Pi Zero 2 W with 416 MB. The aim: after five
years of running, a redraw still costs about what it costs today. Measured on
the Pi before and after, with five-year databases on the SD card (a first
attempt in `/tmp`, which is RAM on this Pi, ran it out of memory). Settled in
[data/storage.md](../intents/data/storage.md).

## Measured before

| History | `minutes` rows | Full read by local day | By UTC day | Last two days only |
|---|---|---|---|---|
| Four hours a day, five years | 438,000 | 3.5 s | 1.8 s | 21 ms |
| Every minute, five years | 2,628,000 | 23 s | 12.9 s | 38 ms |

No full scan per redraw is affordable; a per-day table read is 25 ms either
way. The logger keeps it (`days`), see the storage intent.

## Measured after

Through the programs themselves on whiskerwheel; "minutes" is the same build
made to read the minutes (the `days` table marked as another timezone).

| Five-year history | Read from | `/api/stats` | Dashboard | Settings page | Four screens rendered |
|---|---|---|---|---|---|
| Four hours a day | `days` | 120 ms | 72 ms | 67 ms | 297 ms |
| | `minutes` | 3.6 s | 3.6 s | 3.6 s | 3.8 s |
| Every minute | `days` | 119 ms | 73 ms | 67 ms | 241 ms |
| | `minutes` | 23.3 s | 23.3 s | 23.0 s | 23.3 s |

## Tasks

- [x] Measured on whiskerwheel: five years at four hours a day (~440k rows) and every minute (~2.6M rows), the full read as the screens do it
- [x] The design, from the measurement, written into the storage intent: what is read each time, what is kept, what invalidates it (a new rotation, a wheel edit, a timezone change)
- [x] Built for every reader: the display, the dashboard, the API, the settings page
- [x] Self-tests: the kept part and a fresh read agree, across midnight, a wheel switch and a timezone change
- [x] Measured again on whiskerwheel with the same databases; numbers in this plan
- [x] The databases removed from the Pi afterwards
