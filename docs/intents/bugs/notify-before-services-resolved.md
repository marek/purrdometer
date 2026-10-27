# Case study: connected, and never a measurement

> A bug case study, where history is told on purpose. The present-tense rule
> it produced is in [../ble/link.md](../ble/link.md#connecting).

## Symptom

With the link finally stable, the prototype monitor reported "CSC
notifications enabled" and then received nothing, however long it ran.

## Evidence

| Source | Fact |
|---|---|
| The monitor | Slept a fixed 3 s after `Connected`, listed GATT objects once, then marked setup done |
| BlueZ | GATT discovery at a ~500 ms interval took up to 17 s; `ServicesResolved` turned true long after the 3 s |
| `btmon` | No write to the CSC Measurement's CCCD (handle `0x002a`) from the monitor at all |
| `busctl call … StartNotify` | Notifications switched on, then off again as soon as `busctl` exited |

## Root cause

Two faults. The monitor looked for the characteristic before it existed, found
nothing, and never looked again. And BlueZ ties a notification session to the
D-Bus connection that asked for it, so a short-lived tool cannot hold one.

## Fix

The logger waits for `ServicesResolved`, starts notifications from its own
long-lived connection, and keeps that connection for the life of the process.
The CCCD write and its response now appear in `btmon` on every connection.
Whether measurements then arrive depends on the sensor detecting rotation
([../ble/sensor.md](../ble/sensor.md#the-measurement)).

## Playbook

1. "Connected" is not "ready": wait for `ServicesResolved` before any GATT
   work, and retry a missing characteristic instead of giving up.
2. A `StartNotify` from a CLI tool ends with the tool; test notifications from
   a process that stays running (an interactive `bluetoothctl`, the logger).
3. When the link is up and data is missing, look in `btmon` for the CCCD
   write (`Write Request … 0x2902`, `0100`) and its response before blaming
   the sensor.
4. A cadence sensor sends only when it detects a revolution; a silent but
   connected sensor may simply not be turning.

## Cross-links

- [../ble/link.md](../ble/link.md), [../ble/sensor.md](../ble/sensor.md).
