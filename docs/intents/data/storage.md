# Storage

> Owns the SQLite database: where it is, its tables, who writes and who reads,
> retention and durability. A cornerstone: the database is the only contract
> between the logger and its readers, and this doc wins when another
> disagrees.

## Requirement

Rotation history is permanent and survives restarts, upgrades, remove and
purge. The logger is the only writer; the display and the web dashboard only
read. No process depends on another running.

## The database

| Subject | Rule |
|---|---|
| Path | `/var/lib/purrdometer/purrdometer.db`, owned by the `purrdometer` user |
| Mode | WAL, `synchronous=NORMAL`: a reader never blocks the writer; a power cut loses at most the last moments, never the file |
| Schema owner | The logger creates and migrates it; `meta.schema_version` names the version (5). Version 3 added `minutes.estimated`; version 4 keys `minutes` by minute and wheel profile, and puts every older row under profile 1, in one transaction; version 5 adds `days`, built from `minutes` |
| Readers | The display and the web dashboard open it read-only; a missing file or schema reads as no history |

## Tables

| Table | Holds | Kept |
|---|---|---|
| `minutes(minute, wheel, revs, estimated)`, keyed by `minute` and `wheel` | Rotations per wall-clock minute and wheel profile ([wheels.md](wheels.md#profiles)): measured (`revs`) and estimated turns the sensor missed (`estimated`, [accounting.md](accounting.md#turns-the-sensor-misses)); `minute` = unix time ÷ 60, UTC. A minute in which the profile changed has a row for each | Forever: this is the history |
| `days(day, wheel, revs, estimated)`, keyed by `day` and `wheel` | The same rotations summed per local day (`YYYY-MM-DD`) and wheel profile: what every reader reads ([Reading the history](#reading-the-history)) | Forever, kept equal to `minutes` |
| `samples(ts, revs, event_time, delta)` | Every measurement that added rotations, as received | 30 days, pruned at logger start |
| `state(key PK, value)` | Live status and the counter needed to resume ([accounting.md](accounting.md)) | Overwritten in place |
| `sensors(address PK, name, kind, rssi, last_seen)` | Every CSC sensor heard; `kind` is `cadence`, `speed` or empty until the sensor's features are read | Overwritten in place |
| `meta(key PK, value)` | `schema_version`, `created`, `days_timezone` (the timezone `days` is grouped by) | Forever |

## State keys

| Key | Meaning | Written |
|---|---|---|
| `connected` | `1` while the sensor is connected | On connect and disconnect |
| `sensor` | The sensor's address | When the sensor is chosen |
| `sensor_kind` | `cadence` or `speed` | When its features are read |
| `battery` | Battery percentage | Once per connection |
| `rpm` | Latest rotation rate; `0` when disconnected | Per measurement |
| `last_motion` | Unix time of the last measurement that added rotations | Per moving measurement |
| `last_counter`, `last_revs`, `last_event_time`, `last_wall` | The previous measurement, so a restarted logger counts across the gap; removed together when the sensor changes | Per measurement |

## Rules

- Only rotations are stored, each under its wheel profile. Distance, speed
  and every chart are derived at read time ([wheels.md](wheels.md)).
- The logger stores a rotation under the profile current when it arrives,
  read from `profiles.conf`; it follows a change within a second.
- Days are local days. `minutes` is the record and never changes with the
  timezone; `days` is derived from it, and rebuilt when the timezone changes.
- A measurement and the state it updates (`last_*`, `last_motion`, `rpm`) are
  written in one transaction. A transaction that fails is rolled back whole
  and the connection stays usable for the next one; the logger then keeps its
  previous measurement ([accounting.md](accounting.md)), so a failed write
  loses no rotation and counts none twice.

## Reading the history

The screens redraw every 5 s while the wheel turns, the dashboard refreshes
as often, and the settings page's live test asks every second, so a read of
the history must not grow with it. Measured on whiskerwheel (Pi Zero 2 W) with
five-year databases:

| History | `minutes` rows | Read from `minutes` (by local day) | Read from `days` |
|---|---|---|---|
| Four hours a day for five years | 438,000 | 3.5 s | 25 ms |
| Every minute for five years | 2,628,000 | 23 s | 25 ms |

(25 ms includes 16 ms of starting the `sqlite3` tool.) Through the programs
themselves, reading `days`, the dashboard answers in about 70 ms and
`/api/stats` in 120 ms for either history, against 3.6 s and 23 s reading
`minutes`. So:

| Subject | Rule |
|---|---|
| Readers | Read `days` alone: at most a row per day and wheel, about 1,800 for five years of one wheel |
| Writing | The logger adds a measurement's rotations to `minutes` and to its local day's `days` row in the same transaction, so the two never disagree |
| Timezone | The logger records the timezone `days` was grouped by (`days_timezone`: `TZ`, else the zone `/etc/localtime` names). When it starts under another one, it rebuilds `days` from `minutes` in one transaction (once; 3.5 s for the four-hour history above, 22 s for the every-minute one). A timezone change therefore applies when the services restart, which is when a running program would take it up anyway |
| A reader under another timezone than `days_timezone`, or a database without `days` | Reads `minutes` instead: correct, and slow on a long history |
| Upgrade | Version 5 builds `days` from `minutes` once, when the logger first starts |

