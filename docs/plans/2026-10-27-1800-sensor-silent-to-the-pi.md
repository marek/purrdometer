# The sensor that streamed to everything but the Pi

Date: 2026-10-27 18:00 EDT

## Context

With the link stable, the Pi still received no rotations: connected and
subscribed, the sensor sent it nothing, while a laptop got measurements from
the same turning. The sensor remembered a pairing with the Pi's own address
from May. The Pi now connects with a random address, which no sensor knows,
and the package sets it. How it was found is in
[bugs/silent-to-a-central-it-remembers.md](../intents/bugs/silent-to-a-central-it-remembers.md).

## Tasks

- [x] Ruled out: the subscription (read back from the sensor), BlueZ and the logger (`gatttool`), the connection interval (forced to 48.75 ms), PHY, data length and encryption
- [x] Side by side with the same turning: 38 measurements on a laptop, none on the Pi
- [x] A random address on the Pi: 23 measurements and 16 rotations in 40 s
- [x] `Privacy = device` in BlueZ's `main.conf` from the package, under its own marker, removed by purge; tested on a scratch `main.conf`
- [x] Intents, a bug case study and the troubleshooting guide; pairing documented as the alternative
- [x] The package with the setting installed on whiskerwheel; privacy survives a Bluetooth restart and a reboot
- [ ] Clear the test rotations from the history, or keep them (the user to decide)
- [ ] A sensor section in the settings page: the sensors heard, cadence or speed, choosing one, mounting help per type, a live test (the user to decide)
