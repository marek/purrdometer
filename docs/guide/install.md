# Install

The hardware, installing the package, and upgrading or removing it.

## Hardware

| Part | Notes |
|---|---|
| Raspberry Pi Zero 2 W | Raspberry Pi OS bookworm (64-bit) or later, on WiFi |
| Waveshare 2.13" Touch e-Paper HAT | The V4 panel with the GT1151 touch controller. Enable SPI and I²C (`sudo raspi-config` → Interface Options) |
| Bluetooth LE cadence sensor | Any Cycling Speed and Cadence sensor with crank data; built against a Garmin cadence sensor |
| Cat wheel | Presets for the Ferris Cat Wheel G5, Grand Ferris, One Fast Cat and Star Cat L and XL; any wheel works once its diameter is set ([configuration](configuration.md)) |

## Install the package

Build the package on the Pi and install it:

```sh
git clone <this repo> ~/workspace/purrdometer && cd ~/workspace/purrdometer
sudo apt install build-essential pkg-config libsdbus-c++-dev libsqlite3-dev dpkg-dev \
    libcpp-httplib-dev nlohmann-json3-dev python3-pil python3-spidev python3-smbus2 python3-gpiozero python3-lgpio fonts-dejavu-core
packaging/build-deb.sh     # about 7 minutes the first time on a Pi Zero
sudo apt install ./dist/purrdometer_*_arm64.deb
```

That is the whole setup. The package:

- installs and starts three services: `purrdometer-logger` (Bluetooth to the
  database), `purrdometer-display` (the screen) and `purrdometer-web` (the
  [web dashboard](web.md) on port 8080);
- sets Bluetooth's LE supervision timeout to 6 s and turns WiFi power-save
  off, which keeps the sensor connected ([troubleshooting](troubleshooting.md#a-sensor-that-keeps-dropping));
- creates a `purrdometer` user and `/var/lib/purrdometer` for the history.

The screen shows NOW with zeros until the wheel turns. Spin the wheel to wake
the sensor; the logger finds it by itself ([the sensor](sensor.md)).

## Upgrade

Build and install the new package the same way. Settings in
`/etc/purrdometer/purrdometer.conf` and the history are kept.

When Raspberry Pi OS upgrades `bluez`, apt may ask about a changed
`/etc/bluetooth/main.conf`. Keep the installed version (the default answer):
it holds purrdometer's timeout.

## Remove

```sh
sudo apt remove purrdometer   # stops the services; settings and history kept
sudo apt purge purrdometer    # also takes the Bluetooth setting out again
```

The history in `/var/lib/purrdometer` is kept even by a purge. Delete it by
hand if you really want it gone.
