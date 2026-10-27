# purrdometer guide

An odometer for a cat wheel: a Bluetooth cadence sensor on the wheel, a
Raspberry Pi Zero 2 W logging every rotation, and a 2.13" touch e-paper
screen and a web page showing rotations, distance and history. This guide is for people who
set it up and use it; working on its source is the last page.

| Page | What it covers |
|---|---|
| [Install](install.md) | The hardware, installing the package, upgrading, removing |
| [The sensor](sensor.md) | Mounting it on the wheel, the first connection, the battery, changing sensors |
| [The screens](screens.md) | NOW, 30 DAYS, CALENDAR, ALL TIME, the touch bar, how often it redraws |
| [The web dashboard](web.md) | The page at `http://whiskerwheel:8080/`, the JSON, its settings |
| [Configuration](configuration.md) | `purrdometer.conf`: the wheel (Ferris, Grand Ferris, One Fast Cat, Star Cat, or measured), the screen, the web dashboard |
| [Troubleshooting](troubleshooting.md) | Logs, a sensor that drops or stays silent, the Bluetooth trace, the database |
| [Working on purrdometer](development.md) | Repo layout, building, testing, previews, packaging |
