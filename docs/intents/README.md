# purrdometer intent corpus

Style: precise, normative, table-heavy, light on prose. The `code-style`
skill's rules (no history narration, only the non-obvious, concise) apply to
every doc here exactly as to a code comment. Docs state requirements and
rationale, never a walkthrough of the code. The `intent-docs` skill is how to
write one.

**Sources.** This corpus is written from what was decided: the user's words
in the working sessions, the plans, the measured behaviour of the sensor and
the Pi, and the user guide (`docs/guide/`). It is not read off the code. A
number appears only where it was decided or measured (the 6 s supervision
timeout, the published inner diameters of the wheel presets, the 250×122 panel); tuning
values live as named constants in the source, and the doc says so rather than
listing them.

**Cornerstones.** Two docs win when another doc disagrees with them:

- [data/storage.md](data/storage.md) owns the database: the one contract
  between the logger and the display.
- [workspace/repos.md](workspace/repos.md) owns what may be committed and
  what may be pushed.

## Folders

| Folder | Holds |
|---|---|
| `intents/*.md` | What the project is and the words it uses |
| [workspace/](workspace/) | The repo, building, running, the package |
| [ble/](ble/) | The sensor and the link to it |
| [data/](data/) | Storage, turning sensor readings into rotations, wheels and distance |
| [display/](display/) | The e-paper screens, the touch bar, the refresh policy |
| [web/](web/) | The web dashboard and the templates its pages are written in |
| [bugs/](bugs/) | Bug case studies: the one place history is told |

## Docs

| Doc | Owns |
|---|---|
| [overview.md](overview.md) | What purrdometer is, the principles |
| [glossary.md](glossary.md) | Term disambiguation |
| [workspace/repos.md](workspace/repos.md) | The single repo, identity, branches, pushing, what is committed |
| [workspace/build-and-run.md](workspace/build-and-run.md) | Build dependencies, make targets, the testing venues, running on the Pi |
| [workspace/packaging.md](workspace/packaging.md) | The `.deb`: contents, version, maintainer scripts, the system settings it owns, install/remove/purge guarantees |
| [ble/link.md](ble/link.md) | Keeping the BLE link up: supervision timeout, WiFi coexistence, unbonded use, reconnecting, discovery |
| [ble/sensor.md](ble/sensor.md) | The CSC sensor as the logger uses it: services, the measurement, battery, which sensor is ours, mounting |
| [data/storage.md](data/storage.md) | The SQLite database: path, tables, who writes and who reads, retention, durability |
| [data/accounting.md](data/accounting.md) | Sensor readings → rotations: deltas, wrap-around, resets, rotations while the logger was down, rpm |
| [data/wheels.md](data/wheels.md) | Wheel presets, custom diameters, units, distance and speed, recomputed history |
| [display/screens.md](display/screens.md) | The four screens and what each shows |
| [display/touch-bar.md](display/touch-bar.md) | The tab strip: placement, hit areas, orientation, the touch controller |
| [display/refresh.md](display/refresh.md) | When the panel redraws: partial vs full, ghosting, live updates, sleep |
| [web/dashboard.md](web/dashboard.md) | `purrdometer-web`: the stack, routes, what the page shows, access, the service |
| [web/pages.md](web/pages.md) | `.page` files compiled to C++ by `pagec`: tags, escaping, formatters |
| [bugs/supervision-timeout-drops.md](bugs/supervision-timeout-drops.md) | The link dropping every 7–16 s: a 420 ms supervision timeout on a ~200 ms interval |
| [bugs/notify-before-services-resolved.md](bugs/notify-before-services-resolved.md) | A connected sensor that never sent data: notifications requested before GATT discovery finished |

## Reading paths

- **New here**: this README → overview → glossary → workspace/repos →
  workspace/build-and-run → data/storage.
- **Changing the BLE side**: ble/link → ble/sensor → data/accounting →
  bugs/supervision-timeout-drops.
- **Changing what is stored**: data/storage → data/accounting → display/screens.
- **Changing a screen or the touch bar**: display/screens →
  display/touch-bar → display/refresh → data/wheels.
- **Changing the web dashboard**: web/dashboard → web/pages →
  data/storage → data/wheels.
- **Changing the package or system settings**: workspace/packaging →
  ble/link.

## Build plans

Plans are volatile and live in [../plans/](../plans/README.md). A plan keeps
tasks and checkboxes; what it decides is written here.
