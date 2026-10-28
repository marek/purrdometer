# Plans

Every plan for purrdometer is a file in this folder named
`YYYY-MM-DD-HHMM-<slug>.md` (local time the plan first appeared), with a
`Date:` line under its title and a checkbox list that is ticked and committed
as each task finishes. The `plan-checklists` skill has the rules;
`tools/check-plans.sh` checks them and `tools/check-plans.sh --fix` refreshes
the counts below.

A plan is volatile: tasks, boxes, decisions waiting for the user. What a plan
settles is written into [../intents/](../intents/README.md), and the plan
links to it.

"What is left" is the Open column. Boxes marked for the user need a person;
the rest can be worked.

| Plan | Covers | Done | Open |
|---|---|---|---|
| [2026-10-27-1042-ble-link-stability.md](2026-10-27-1042-ble-link-stability.md) | The sensor link that dropped every few seconds: the 6 s supervision timeout, WiFi power-save, unbonded use, an event-driven client, all owned by the package | 8 | 0 |
| [2026-10-27-1105-screens-storage-and-package.md](2026-10-27-1105-screens-storage-and-package.md) | Rotations in SQLite, the four screens and the touch bar, wheel presets with the Ferris G5, the `.deb`; checks on the panel and the wheel, and decisions on history and the OS upgrade | 13 | 2 |
| [2026-10-27-1216-docs-and-working-structure.md](2026-10-27-1216-docs-and-working-structure.md) | The README, guide, intents, plans, skills, roles and working agreement adopted from wayland-canvas | 9 | 0 |
| [2026-10-27-1236-wheel-presets-and-web-dashboard.md](2026-10-27-1236-wheel-presets-and-web-dashboard.md) | Presets for the Ferris, Grand Ferris, One Fast Cat and Star Cat wheels from one shared file; a C++ web dashboard with pages compiled to C++ | 9 | 1 |
| [2026-10-27-1420-cpp26-trixie-and-web-settings.md](2026-10-27-1420-cpp26-trixie-and-web-settings.md) | C++26 on Raspberry Pi OS trixie, builds in a trixie container, and the dashboard's gear page for the wheel and screen settings | 9 | 1 |
| [2026-10-27-1615-display-in-cpp.md](2026-10-27-1615-display-in-cpp.md) | The e-paper display ported to C++, proven pixel by pixel against the Python version, and Python removed | 9 | 0 |
| [2026-10-27-1650-releases-and-image.md](2026-10-27-1650-releases-and-image.md) | Releases from a `v*` tag on GitHub's arm64 runners: the `.deb` and a shrunk, compressed Raspberry Pi OS Lite image with it installed | 6 | 0 |
| [2026-10-27-1705-setup-hotspot.md](2026-10-27-1705-setup-hotspot.md) | A setup hotspot with a per-device password shown on the e-paper, and a setup page for WiFi, the login password and the wheel | 6 | 6 |
| [2026-10-27-1800-sensor-silent-to-the-pi.md](2026-10-27-1800-sensor-silent-to-the-pi.md) | A connected, subscribed sensor silent to the Pi alone: it remembered a pairing with the Pi's address; the Pi now connects with a random address | 8 | 0 |
| [2026-10-27-1920-choosing-the-sensor.md](2026-10-27-1920-choosing-the-sensor.md) | The settings page lists the sensors heard, cadence or speed, chooses one, shows how to mount it and tests it live; speed sensors supported | 7 | 1 |
| [2026-10-27-2000-one-job-per-class.md](2026-10-27-2000-one-job-per-class.md) | The source divided into a folder per program and a class per job, behaviour unchanged | 9 | 0 |
| [2026-10-28-0846-wheel-profiles.md](2026-10-28-0846-wheel-profiles.md) | Wheels as profiles, rotations stored under the current one, so a new wheel leaves history alone and an edit corrects it; `.conf` names for the catalogue and the page's settings | 10 | 0 |
| [2026-10-28-0859-install-paths-and-warnings.md](2026-10-28-0859-install-paths-and-warnings.md) | Every path from one header built from the Makefile's install directories; a build with no warnings, kept that way by -Werror | 6 | 0 |
| [2026-10-28-0933-history-that-stays-fast.md](2026-10-28-0933-history-that-stays-fast.md) | Reads of the history that cost the same after five years as today, measured on the Pi | 6 | 0 |
| [2026-10-28-1039-dashboard-on-port-80.md](2026-10-28-1039-dashboard-on-port-80.md) | The dashboard on port 80 in one server with the setup page; 8080 redirects | 6 | 0 |
| [2026-10-28-1205-raw-sensor-log.md](2026-10-28-1205-raw-sensor-log.md) | Every measurement and connection event as the sensor sent it, a file a day for a day or two, off by default, downloadable from the settings page | 5 | 1 |
| [2026-10-28-1635-codebase-hardening-review.md](2026-10-28-1635-codebase-hardening-review.md) | Hardening where failures cross a boundary: a failed SQLite write, the root helper's SSH key, the sensor link across threads, setup commands and state, concurrent web saves, the BlueZ package settings and `pagec`; tests split and sanitizer builds | 13 | 1 |
