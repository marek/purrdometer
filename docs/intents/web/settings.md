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
| `/var/lib/purrdometer/settings.ini` | the `purrdometer` user, written by `purrdometer-web` | What was saved from the gear page; wins over the file above, key by key |

Reset deletes `settings.ini`, which returns every setting to `/etc`. An empty
`diameter =` in `settings.ini` clears a measured diameter set in `/etc`.
Writes go to a temporary file and are renamed into place, so a reader never
sees half a file.

## What the page changes

| Setting | Values |
|---|---|
| `[wheel] units` | `cm` or `in`, as a toggle; switching converts a typed diameter and relabels the wheel sizes |
| `[wheel] model` | Any preset from `wheels.ini`, listed with its running diameter |
| `[wheel] diameter` | A measured running diameter in the chosen units, or empty for the preset's |
| `[display] rotate180` | On or off |
| `[display] active_refresh_s`, `idle_refresh_s`, `full_refresh_every` | Whole numbers in 1–60, 10–3600 and 1–200 |

Every value is checked the same way the config file's are
([../data/wheels.md](../data/wheels.md)); a rejected save says why and writes
nothing.

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

## Picking a change up

| Program | When |
|---|---|
| `purrdometer-web` | On the next request: it reads both files on every request |
| `purrdometer-display` | Before its next redraw (5 s while running, 60 s idle): it checks both files' modification times, and redraws in full on a change |
| `purrdometer-logger` | Unaffected: it stores rotations, which no setting changes |

## Proven by

`purrdometer-web --selftest`: address classes, the layering, validation, and a
real server and client: the page and its token, a save without the token
refused, a save written and used, an implausible diameter refused with its
hint, reset, and `off` refusing. `python/tests`: the display reads the same
layering.
