# A BLE link that stays up

Date: 2026-10-27 10:42 EDT

## Context

The prototype monitor on whiskerwheel connected to the Garmin cadence sensor
and dropped it every few seconds. The goal is a link that stays up through
ordinary radio loss and reconnects by itself, configured reproducibly rather
than by hand edits on the Pi. The rules are in
[ble/link.md](../intents/ble/link.md); how the cause was found is in
[bugs/supervision-timeout-drops.md](../intents/bugs/supervision-timeout-drops.md).

## Tasks

- [x] Root cause from `btmon`: every drop is `Connection Timeout (0x08)`, a 420 ms supervision timeout on a ~206 ms interval
- [x] 6 s LE supervision timeout as BlueZ's default in `main.conf`; the sensor then asks for 6 s itself
- [x] Stale cached `[ConnectionParameters]` cleared
- [x] WiFi power-save off through NetworkManager
- [x] Unbonded use, as the phone app does; the device kept as `Trusted`
- [x] Event-driven monitor: waits for `ServicesResolved`, holds the notification session, scans and reconnects when the sensor sleeps
- [x] Link held across every session observed after the fix, against drops every 7–16 s before
- [x] Every system setting applied by the package's maintainer scripts
