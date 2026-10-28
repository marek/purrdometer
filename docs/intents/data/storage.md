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
| Schema owner | The logger creates and migrates it; `meta.schema_version` names the version (3); version 3 adds `minutes.estimated`, by `ALTER TABLE` on an older database |
| Readers | The display and the web dashboard open it read-only; a missing file or schema reads as no history |

## Tables

| Table | Holds | Kept |
|---|---|---|
| `minutes(minute PK, revs, estimated)` | Rotations per wall-clock minute, measured (`revs`) and estimated turns the sensor missed (`estimated`, [accounting.md](accounting.md#turns-the-sensor-misses)); `minute` = unix time ÷ 60, UTC | Forever: this is the history |
| `samples(ts, revs, event_time, delta)` | Every measurement that added rotations, as received | 30 days, pruned at logger start |
| `state(key PK, value)` | Live status and the counter needed to resume ([accounting.md](accounting.md)) | Overwritten in place |
| `sensors(address PK, name, kind, rssi, last_seen)` | Every CSC sensor heard; `kind` is `cadence`, `speed` or empty until the sensor's features are read | Overwritten in place |
| `meta(key PK, value)` | `schema_version`, `created` | Forever |

## State keys

| Key | Meaning | Written |
|---|---|---|
| `connected` | `1` while the sensor is connected | On connect and disconnect |
| `sensor` | The sensor's address | When the sensor is chosen |
| `sensor_kind` | `cadence` or `speed` | When its features are read |
| `battery` | Battery percentage | Once per connection |
| `rpm` | Latest rotation rate; `0` when disconnected | Per measurement |
| `last_motion` | Unix time of the last measurement that added rotations | Per moving measurement |
| `last_revs`, `last_event_time`, `last_wall` | The previous measurement, so a restarted logger counts across the gap | Per measurement |

## Rules

- Only rotations are stored. Distance, speed and every chart are derived at
  read time ([wheels.md](wheels.md)).
- Days are local days, computed when reading; a timezone change regroups
  history without rewriting it.
- A measurement and the state it updates are written in one transaction.
