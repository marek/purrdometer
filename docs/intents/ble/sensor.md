# The sensor

> Owns the cadence sensor as the logger uses it: the services read, the
> measurement, battery, which sensor is ours, and mounting. Keeping the link up
> is owned by [link.md](link.md).

## Requirement

Any Bluetooth LE sensor implementing the Cycling Speed and Cadence (CSC)
profile with crank revolution data works. The one in use is a Garmin cadence
sensor (`CAD-BLE…`, firmware 2.40.0), which reports crank revolutions only.
One crank revolution is one wheel rotation.

## What is read

| GATT item | UUID | Use |
|---|---|---|
| CSC service | `0x1816` | Discovery filter and sensor match |
| CSC Measurement | `0x2A5B`, notify | Every notification is a measurement ([../data/accounting.md](../data/accounting.md)) |
| CSC Feature | `0x2A5C` | Not read: the measurement's own flags say what it carries |
| Battery Level | `0x2A19`, read | Read once per connection and stored for the display |

## The measurement

Byte 0 is flags. Bit 0 set: 6 bytes of wheel data follow (32-bit cumulative
wheel revolutions, 16-bit last wheel event time), skipped. Bit 1 set: 4 bytes
of crank data follow, little-endian: 16-bit cumulative crank revolutions and
16-bit last crank event time in 1/1024 s. A measurement without crank data is
ignored.

The sensor sends a measurement only when it detects a revolution. A
stationary wheel sends nothing, and a connected, silent sensor is normal.

## Which sensor

The first CSC sensor found is saved to `/var/lib/purrdometer/sensor` and is
the only one used from then on. Deleting that file makes the logger take the
next CSC sensor it sees. The address is machine state and is not committed.

## Mounting

| Subject | Rule |
|---|---|
| Plane | Face parallel to the wheel, as on a crank arm |
| Radius | Close to the axle, about crank length (~15 cm). Near the rim, running speed puts several g of centripetal acceleration on the sensor, which may confuse its rotation detection |
| Battery | The battery door must stay reachable; glue a holder to the wheel and seat the sensor in it, not the sensor's back to the wheel |
| Side | The back side of the wheel, away from the cat |
