# purrdometer

## What purrdometer is

An odometer for a cat's exercise wheel. A Bluetooth LE cadence sensor glued
to the wheel counts rotations. A Raspberry Pi Zero 2 W (`whiskerwheel`) logs
them to SQLite and shows rotations, distance and history on a 2.13" touch
e-paper screen beside the wheel and on a web page on the home network. The
wheel is a Ferris Cat Wheel G5; presets cover the Ferris, Grand Ferris, One
Fast Cat and Star Cat wheels, and any wheel works once its diameter is known.

## The experience

| Area | What you get |
|---|---|
| Counting | Every rotation the sensor reports is stored, including ones made while the logger was down, as long as the sensor stayed awake |
| Distance | Rotations × the wheel's running circumference; pick a known wheel or give a diameter in any unit, and all history is recomputed |
| Screens | NOW (totals, today, live speed), 30 DAYS (bars per day), CALENDAR (weeks of days, shaded by distance), ALL TIME (since the first rotation, with a cumulative line) |
| Touch bar | Four tabs along the bottom edge of the screen choose the screen |
| Web | The same history in a browser at `http://whiskerwheel/`, with a JSON API |
| Reliability | The sensor reconnects by itself after a drop or a sleep; the link tolerates seconds of radio loss |
| Install | One `.deb` installs all four services and every system setting they need |

## Principles

1. **Rotations are the record; distance is derived.** Only rotation counts
   are stored. Distance, speed and every chart are computed from them with
   the configured wheel, so fixing a diameter fixes all history.
   ([data/storage.md](data/storage.md), [data/wheels.md](data/wheels.md))
2. **Never count a rotation twice, never invent one.** A counter
   discontinuity adds nothing; a gap with a plausible delta is kept.
   ([data/accounting.md](data/accounting.md))
3. **The database is the only contract between the processes.** The
   logger writes; the display and the web dashboard read. None talks to
   another directly; each can restart alone.
   ([data/storage.md](data/storage.md))
4. **The link survives the radio.** WiFi and BLE share one radio on the Pi
   Zero 2 W, so the link is configured to ride out lost connection events
   rather than assume none are lost. ([ble/link.md](ble/link.md))
5. **The system is configured by the package, never by hand.** Every setting
   outside the project's own files is applied by the `.deb` and undone by its
   purge. ([workspace/packaging.md](workspace/packaging.md))
6. **E-paper is slow; draw only what changed.** ([display/refresh.md](display/refresh.md))
7. **Light enough for a Pi Zero.** Long-running services are C++ where weight
   matters (the logger, the web dashboard); nothing interprets templates at
   runtime. ([web/dashboard.md](web/dashboard.md))
8. **Standard where a standard exists.** The Bluetooth Cycling Speed and
   Cadence profile for the sensor, BlueZ's D-Bus API for the stack, Debian
   packaging for install. ([ble/sensor.md](ble/sensor.md))
9. **Proven, not presumed.** Behaviour is proven by `make test`, the rendered
   previews and on the Pi before it is called done.
   ([workspace/build-and-run.md](workspace/build-and-run.md))
10. **One repo, nothing public without the user.**
   ([workspace/repos.md](workspace/repos.md))

## How the corpus is organised

By area: the workspace ([workspace/](workspace/)), the sensor and link
([ble/](ble/)), data ([data/](data/)), the screen ([display/](display/)), the
web dashboard ([web/](web/)), and
bug case studies ([bugs/](bugs/)), the one place history is told.
[README.md](README.md) is the index and fixes the reading paths;
[glossary.md](glossary.md) fixes the words. Plans are volatile checklists in
[../plans/](../plans/README.md); what a plan decides is written here.
