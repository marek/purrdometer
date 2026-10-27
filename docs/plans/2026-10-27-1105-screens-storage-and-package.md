# Screens, storage and the package

Date: 2026-10-27 11:05 EDT

## Context

The wheel needs a record and a face: every rotation kept in SQLite, and the
touch e-paper screen showing rotations, distance, the last 30 days and
everything since the start, chosen from a touch bar. The wheel is a Ferris
Cat Wheel G5 and other diameters must be easy to set. Everything installs
from one `.deb`. The decisions are in [data/](../intents/data/) and
[display/](../intents/display/); the package in
[workspace/packaging.md](../intents/workspace/packaging.md).

## Tasks

- [x] Logger writes per-minute rotations, raw samples and live state to SQLite ([data/storage.md](../intents/data/storage.md))
- [x] Accounting: 16-bit wrap, resets, rotations kept across a logger restart, rpm from the sensor's event times; `--selftest` ([data/accounting.md](../intents/data/accounting.md))
- [x] Panel and GT1151 touch driver for the 2.13" V4 HAT, interrupt-driven
- [x] NOW, 30 DAYS, CALENDAR and ALL TIME screens; previews checked by eye ([display/screens.md](../intents/display/screens.md))
- [x] Touch bar along the bottom edge ([display/touch-bar.md](../intents/display/touch-bar.md))
- [x] Refresh policy: partial, periodic full, live while running, sleep when idle ([display/refresh.md](../intents/display/refresh.md))
- [x] Wheel presets with the Ferris G5 at its 48" inner diameter, custom diameters in any unit, `--wheels` and `--check-config` ([data/wheels.md](../intents/data/wheels.md))
- [x] `.deb` with both services, the config, the BlueZ and NetworkManager settings; install, upgrade, remove, purge and reinstall checked on whiskerwheel
- [x] Rebuild and install the package with the wheel presets on whiskerwheel
- [ ] The screen on the panel looks like the previews (the user)
- [ ] Each tab of the touch bar opens its screen, the right way up (the user)
- [ ] Sensor mounted near the axle with the battery door reachable, and a spun wheel shows rotations in the log and on NOW (the user)
- [x] The G5 preset at 121.9 cm (48"), not the measured 123.2 cm (48.5"): decided
- [ ] How historic usage before the sensor is estimated and where it is stored, kept apart from measured rotations (the user to decide)
- [ ] `apt full-upgrade` on whiskerwheel: newer bluez, bluez-firmware and kernel (the user to decide)
