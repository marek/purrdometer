<div align="center">

# purrdometer

An odometer for a cat wheel: every rotation counted, every kilometre kept.

[Why](#why) · [Screens](#screens) · [Web](#web) · [Install](#install) · [Guide](docs/guide/README.md) · [Configuration](docs/guide/configuration.md) · [Contributing](#contributing)

![The NOW screen: total rotations and kilometres, today's distance, live speed while running](docs/media/0-now.png)

</div>

## Why

A cat on a wheel runs further than you'd guess, mostly at night, and nobody
is watching. purrdometer puts a Bluetooth cadence sensor on the wheel and a
small e-paper screen beside it, so you can see in the morning how many
rotations she did, how far that was, and how this week compares with the
last month.

## Screens

Four screens, picked from the touch bar along the bottom edge.

<table>
<tr>
<td width="50%"><img src="docs/media/1-30days.png" alt="30 DAYS: a bar per day for the last 30 days, with the total, the daily average and the best day"></td>
<td width="50%"><img src="docs/media/2-calendar.png" alt="CALENDAR: 19 weeks of days, each shaded by how far the cat ran"></td>
</tr>
<tr>
<td colspan="2" align="center"><img width="50%" src="docs/media/3-alltime.png" alt="ALL TIME: bars per week since the first day, with a line of total distance rising across them"></td>
</tr>
</table>

<sub>Rendered from made-up history by `make media`.</sub>

## Web

The same history in a browser at `http://whiskerwheel:8080/`, served by a
small C++ server with pages compiled to C++: a few MB of memory
and a page in tens of milliseconds, even on a Pi Zero.

![The web dashboard: totals and live speed, the last 30 days, a year of days, all time with total distance](docs/media/web.png)

## Features

- Every rotation logged to SQLite, including ones made while the logger was down: [the sensor](docs/guide/sensor.md)
- Distance from the wheel's running diameter, with presets for a variety of popular brands or a custom diameter in cm or inches, and history recomputed when it changes: [configuration](docs/guide/configuration.md)
- NOW, 30 DAYS, CALENDAR and ALL TIME screens on a 2.13" touch e-paper HAT: [screens](docs/guide/screens.md)
- A web dashboard, its settings behind a gear, and a JSON API on the home network: [web](docs/guide/web.md)
- A Bluetooth link that rides out the Pi Zero 2 W's shared WiFi/Bluetooth radio: [troubleshooting](docs/guide/troubleshooting.md#a-sensor-that-keeps-dropping)
- One `.deb` that installs everything and configures the system: [install](docs/guide/install.md)

## Install

On a Raspberry Pi Zero 2 W with the Waveshare 2.13" Touch e-Paper HAT:

Flash the Raspberry Pi image from the
[latest release](https://github.com/marek/purrdometer/releases/latest) with
Raspberry Pi Imager (set the user and WiFi in its OS customisation), or install
the `.deb` on a Pi running Raspberry Pi OS trixie:

```sh
sudo apt install ./purrdometer_*_arm64.deb
```

Then spin the wheel to wake the sensor. The details are in
[install](docs/guide/install.md).

| Hardware | |
|---|---|
| Computer | Raspberry Pi Zero 2 W, Raspberry Pi OS trixie or later |
| Screen | Waveshare 2.13" Touch e-Paper HAT (V4) |
| Sensor | Any Bluetooth LE cycling cadence sensor (built with a Garmin one) |
| Wheel | One of the popular brands with a preset, or any wheel with a known diameter |

## Documentation

The [guide](docs/guide/README.md) covers installing, the sensor, the screens,
the web dashboard, configuration, troubleshooting and development.

## Contributing

purrdometer is built by agentic development with intents, tests and
implementation. Start with [AGENTS.md](AGENTS.md) and
[development](docs/guide/development.md).

## License

[Apache-2.0](LICENSE.md).
