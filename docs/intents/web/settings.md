# Settings from the dashboard

> Owns the gear page: what it may change, where it saves, who may save, and how
> the other programs pick a change up. The config file's keys are described in
> [../../guide/configuration.md](../../guide/configuration.md).

## Requirement

The wheel and the screen can be set from a phone on the home network: the
gear in the dashboard's header opens a page that shows every setting and
saves them. Anyone who can see the dashboard can see the settings; saving is
governed by one rule in the config file. A setting saved here takes effect
without a restart.

## Layers

| File | Owner | Holds |
|---|---|---|
| `/etc/purrdometer/purrdometer.conf` | root, a conffile of the package | The defaults, and everything the web may not change |
| `/var/lib/purrdometer/settings.conf` | the `purrdometer` user, written by `purrdometer-web` | What was saved from the gear page; wins over the file above, key by key |
| `/var/lib/purrdometer/profiles.conf` | the `purrdometer` user, written by `purrdometer-web` | The wheel profiles ([../data/wheels.md](../data/wheels.md#profiles)) |

Reset deletes `settings.conf`, which returns every setting to `/etc`; it
never touches the profiles, which are history.
Writes go to a temporary file and are renamed into place, so a reader never
sees half a file.

## What the page changes

| Setting | Values |
|---|---|
| `[wheel] units` | `cm` or `in`, as a toggle; switching converts a typed diameter and relabels the wheel sizes |
| Wheels | The profiles in `profiles.conf`: below |
| `[display] rotate180` | On or off |
| `[display] active_refresh_s`, `idle_refresh_s`, `full_refresh_every` | Whole numbers in 1–60, 10–3600 and 1–200 |
| `[sensor] address` | A sensor's address, or empty for automatic |
| `[sensor] wake_turns` | A whole number 0–10: turns added when the sensor wakes, for the turns it misses ([../data/accounting.md](../data/accounting.md#turns-the-sensor-misses)) |
| `[sensor] raw_log` | On/off: keep the exact sensor traffic for debugging ([../ble/raw-log.md](../ble/raw-log.md)) |
| `[sensor] raw_log_days` | A whole number 1–14: raw log days to keep; default 2 |

Every value is checked the same way the config file's are
([../data/wheels.md](../data/wheels.md)); a rejected save says why and writes
nothing.

### Wheels

| Part | Shows or does |
|---|---|
| The list | Every profile: its name, its wheel (the preset's name, or "custom") and running diameter, and its distance; the current one marked **in use** |
| Edit | Name, preset and measured diameter of any profile. Saving says how far that profile's distance moves ("839 km → 861 km") and changes no other |
| New wheel | A name, a preset and an optional measured diameter; saved as a new profile and made current. Next to it: "Switch when you move the sensor to the new wheel" |
| Use this wheel | Makes another profile current, for a wheel brought back |
| Delete | Shown only on a profile with no rotations that is not in use |

Every wheel change passes the same checks as the other settings and the same
access rule; a rejected one says why and writes nothing.

### The sensor section

| Part | Shows or does |
|---|---|
| Sensors heard | Every CSC sensor in the `sensors` table, newest first: name, address, kind, signal, when last heard, and which one is connected with its battery |
| Choice | Automatic (the first heard) or one of the list; saved as `[sensor] address` |
| Scan | Asks the logger for a 20-second scan by touching `/var/lib/purrdometer/scan-request`; the list fills in as it runs. Governed by the same access rule as saving |
| Mounting | A picture and a few lines for the chosen sensor's kind: cadence or speed |
| Missed turns | `wake_turns`, with what it is for |
| Debug log | `raw_log`, its retention in days, and links to each retained day; only ordinary `YYYY-MM-DD.log` files can be downloaded |
| Test | "Turn the wheel now": a live count of rotations and rpm since the test began, read from `/api/stats` every second, with no saving involved |

**Never from the web:** `[web]` (`bind`, `port`, `settings_access`,
`settings_pin`). A page that could change who may use it, or where it
listens, could lock itself out or open itself up.

## Who may save

`[web] settings_access` in `/etc/purrdometer/purrdometer.conf`:

| Value | Saving allowed from |
|---|---|
| `lan` (default) | The home network: loopback, 10/8, 172.16/12, 192.168/16, 169.254/16, fc00::/7, fe80::/10, and their IPv4-mapped forms |
| `pin` | Anywhere, with `settings_pin` typed into the page |
| `open` | Anywhere |
| `off` | Nowhere: the page is read-only, and the config file is edited instead |

| Subject | Rule |
|---|---|
| Safe fallbacks | `pin` without a `settings_pin`, or an unreadable value, means `off`, never something wider |
| Forms | Every save and reset carries a random token that must match a `SameSite=Strict`, `HttpOnly` cookie set with the page, so another site cannot submit the form |
| PIN | Compared in constant time; a wrong PIN waits a second before answering |
| Viewing | The page is visible to anyone who can reach the dashboard; when saving is not allowed it says why and the buttons are disabled |

## Saving

| Subject | Rule |
|---|---|
| One writer at a time | The server answers two requests at once. Every change to the settings files (a save, a reset, a wheel added, edited, used or deleted, the setup page's wheel, profile 1 written from `[wheel]`) holds one lock from reading what is there to writing it back, so two overlapping changes both take effect and neither is written over the other. A client is authorised before the lock is taken: a wrong PIN's second does not hold other saves up |
| Reading under the lock | What a form leaves out (`wake_turns`) is taken from the file once the lock is held, not from before the request waited for it, so it keeps what the save before it wrote |
| Whole files | A file is written beside its place under a name of its own, flushed, renamed over it, and its folder flushed: a reader sees the old file or the new, the new one survives a power cut, and two writers never share a temporary file |
| A write that fails | The request fails, the old file stays, nothing is left beside it. If only the folder could not be flushed, the new file is in place and the request fails all the same |

## Picking a change up

| Program | When |
|---|---|
| `purrdometer-web` | On the next request: it reads both files on every request |
| `purrdometer-display` | Before its next redraw (5 s while running, 60 s idle): it checks both files' modification times, and redraws in full on a change |
| `purrdometer-logger` | Within a second for `[sensor] address`, `wake_turns`, `raw_log` and `raw_log_days`: it checks both files' modification times; nothing else it does depends on a setting |

## Proven by

`purrdometer-web --selftest`: address classes, the layering, validation, and a
real server and client: the page and its token, a save without the token
refused, a save written and used, an implausible diameter refused with its
hint, reset (which keeps the profiles), and `off` refusing; profiles seeded
from `[wheel]`, edited, added, switched and deleted, a profile with rotations
not deleted; saves and wheel changes made at once from two clients, none lost
and no two wheels given one number, and a file replaced by many writers read
whole. The display reads the settings through the same C++ code.
