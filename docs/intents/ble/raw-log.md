# The raw sensor log

> Owns the debugging log of what the sensor sent: what is in it, where it is,
> how long it is kept, and how it is turned on.

## Requirement

When counting goes wrong, what the sensor actually sent can be read back:
every measurement as received, what the logger made of it, and every
connection event, for the last day or two. It costs nothing unless it is
turned on.

## Rules

| Subject | Rule |
|---|---|
| Settings | `[sensor] raw_log = on/off` (default off) and `raw_log_days` (default 2, 1–14), in the config file or the settings page; the logger follows a change within a second |
| Where | `/var/lib/purrdometer/sensor-log/<YYYY-MM-DD>.log`, a file a local day, owned by the `purrdometer` user |
| Kept | The last `raw_log_days` days; older files are removed when a day's file is opened and when the setting changes. Turning the log off keeps the files already written |
| A measurement | `<time> measurement <hex bytes> <counter> revs=<n> event=<1/1024 s> +<turns> [restart] [garbage] [estimated=<n>] [rpm=<n>]`: the bytes exactly as sent, and what the accounting made of them ([../data/accounting.md](../data/accounting.md)) |
| An event | `<time> <event> [detail]`: heard (with signal), connecting, connected, disconnected, services resolved, notifications on, battery, sensor kind, counter restored, wheel profile, settings changed |
| Time | Local time with milliseconds and the UTC offset, so lines sort and compare across a timezone change |
| Size | About one measurement a second while the wheel turns, and for the few minutes a still sensor stays connected; at around 100 bytes a line, four hours of running a day is about 2 MB, two days about 5 MB |
| Reading | Plain text, for `grep` over SSH; the settings page links each day's file for download, under the same access rule as the page |

## Proven by

`purrdometer-logger --selftest`: a measurement and an event written as above,
nothing written while off, a day past the limit removed.
`purrdometer-web --selftest`: the settings saved, a day's file downloaded,
nothing else in the folder reachable. On whiskerwheel: turned on from the page,
and a spin of the wheel in the log.
