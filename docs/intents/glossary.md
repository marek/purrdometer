# Glossary

Terms as this project uses them. Where a word has a general meaning and a
narrower one here, the narrower one is given.

| Term | Meaning here |
|---|---|
| **purrdometer** | This project: the logger, the display and the package ([overview.md](overview.md)) |
| **whiskerwheel** | The Raspberry Pi Zero 2 W the project runs on, reachable as `marek@whiskerwheel` |
| **Rotation** | One full turn of the wheel; the sensor reports it as one crank revolution ([data/accounting.md](data/accounting.md)) |
| **Sensor** | The Garmin BLE cadence sensor (`CAD-BLE…`) glued to the wheel ([ble/sensor.md](ble/sensor.md)) |
| **CSC** | Bluetooth's Cycling Speed and Cadence profile: service `0x1816`, measurement characteristic `0x2A5B` ([ble/sensor.md](ble/sensor.md)) |
| **Measurement** | One CSC notification: cumulative crank revolutions (16-bit) and the time of the last crank event (1/1024 s, 16-bit) |
| **Delta** | Rotations between two measurements, after wrap-around and plausibility ([data/accounting.md](data/accounting.md)) |
| **Reset** | A counter discontinuity (sensor rebooted, battery swapped, garbage); adds nothing and resynchronises |
| **Connection interval** | Time between BLE connection events; the sensor asks for ~200–500 ms ([ble/link.md](ble/link.md)) |
| **Supervision timeout** | How long a BLE link may go without a received packet before it is declared lost; 6 s here ([ble/link.md](ble/link.md)) |
| **Coexistence** | The Pi's one radio and antenna time-shared between WiFi and BLE ([ble/link.md](ble/link.md)) |
| **Logger** | `purrdometer-logger`, the C++ BlueZ client that writes the database |
| **Display** | `purrdometer-display`, the C++ program that draws the screens and reads the touch bar |
| **Web dashboard** | `purrdometer-web`, the C++ server for the same history in a browser ([web/dashboard.md](web/dashboard.md)) |
| **Gear page, settings.ini** | The dashboard's settings page and the file it saves to, which wins over `/etc/purrdometer/purrdometer.conf` ([web/settings.md](web/settings.md)) |
| **Page, pagec** | A `.page` file, HTML with C++ in `<% %>` tags, and the compiler that turns it into a C++ function ([web/pages.md](web/pages.md)) |
| **Database** | `/var/lib/purrdometer/purrdometer.db` ([data/storage.md](data/storage.md)) |
| **Minute bucket** | A row of `minutes`: rotations counted in one wall-clock minute; the permanent history |
| **Wheel, preset** | A wheel model with a known running diameter, picked by key such as `ferris-g5`; listed in `data/wheels.ini` ([data/wheels.md](data/wheels.md)) |
| **Running diameter** | The diameter of the surface the cat runs on: the inside of the track, not the outer size |
| **Screen** | One of NOW, 30 DAYS, CALENDAR, ALL TIME ([display/screens.md](display/screens.md)) |
| **Touch bar** | The strip of four tabs along the bottom edge of the screen ([display/touch-bar.md](display/touch-bar.md)) |
| **Partial / full refresh** | A fast e-paper update vs a flashing one that clears ghosting ([display/refresh.md](display/refresh.md)) |
| **Preview** | A screen rendered to PNG from a fake-history database, no hardware needed |
| **Intent, plan, skill, role** | The contract; a volatile checklist; how to work; who is working ([README.md](README.md), `AGENTS.md`) |
| **Strand** | One of intent, tests, implementation |
