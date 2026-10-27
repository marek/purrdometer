# The sensor

> Owns the cadence sensor as the logger uses it: the services read, the
> measurement, battery, which sensor is ours, and mounting. Keeping the link up
> is owned by [link.md](link.md).

## Requirement

Any Bluetooth LE sensor implementing the Cycling Speed and Cadence (CSC)
profile works, of either kind, and either way one revolution it reports is one
turn of the cat wheel:

| Kind | Reports | CSC Feature bit | Counter |
|---|---|---|---|
| Cadence | Crank revolutions | 1 | 16-bit |
| Speed | Wheel revolutions | 0 | 32-bit |

A sensor reporting both is used as a speed sensor. The one in use is a Garmin
cadence sensor (`CAD-BLE…`, firmware 2.40.0).

## What is read

| GATT item | UUID | Use |
|---|---|---|
| CSC service | `0x1816` | Discovery filter and sensor match |
| CSC Measurement | `0x2A5B`, notify | Every notification is a measurement ([../data/accounting.md](../data/accounting.md)) |
| CSC Feature | `0x2A5C`, read | Read once per connection: the sensor's kind, stored with what was heard ([../data/storage.md](../data/storage.md)) |
| Battery Level | `0x2A19`, read | Read once per connection and stored for the display |

## The measurement

Byte 0 is flags. Bit 0 set: 6 bytes of wheel data follow, little-endian:
32-bit cumulative wheel revolutions and 16-bit last wheel event time in
1/1024 s. Bit 1 set: 4 bytes of crank data follow: 16-bit cumulative crank
revolutions and 16-bit last crank event time in 1/1024 s. The logger counts
the wheel data of a speed sensor and the crank data of a cadence sensor; a
measurement without the counter it counts is ignored.

The sensor sends a measurement only when it detects a revolution. A
stationary wheel sends nothing, and a connected, silent sensor is normal.

## Which sensor

| Setting | The logger uses |
|---|---|
| `[sensor] address` set (from the settings page or the config file) | That sensor and no other |
| Not set | The first CSC sensor heard, remembered in `/var/lib/purrdometer/sensor`, as before |

A change of choice applies without a restart: the logger disconnects and
connects to the new sensor, and the counter starts again from that sensor's
next measurement, so nothing is counted across the switch.

Every CSC sensor the logger hears is recorded with its name, signal strength,
kind (once known) and when it was last heard, for the settings page to list.
The logger scans when it has no sensor, and for 20 seconds when the settings
page asks it to (by touching `/var/lib/purrdometer/scan-request`), connected
or not.

## Mounting

| Subject | Rule |
|---|---|
| Cadence sensor plane | Face parallel to the wheel, as on a crank arm: it counts turns of its own orientation, like a clock hand going round |
| Speed sensor | On the axle or hub, as on a bicycle hub, or on the wheel's face like a cadence sensor; it counts turns the same way |
| Radius | Close to the axle, about crank length (~15 cm). Near the rim, running speed puts several g of centripetal acceleration on the sensor, which may confuse its rotation detection |
| Battery | The battery door must stay reachable; glue a holder to the wheel and seat the sensor in it, not the sensor's back to the wheel |
| Side | The back side of the wheel, away from the cat |
