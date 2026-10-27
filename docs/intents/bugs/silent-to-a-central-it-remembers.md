# Case study: connected, subscribed, and silent to the Pi alone

> A bug case study, where history is told on purpose. The present-tense rule
> it produced is in [../ble/link.md](../ble/link.md#security).

## Symptom

With the link stable and notifications switched on, the logger stored no
rotations however the sensor was turned. The same sensor, turned the same
way, streamed to a laptop.

## Evidence

| Test | Fact |
|---|---|
| CCCD read back from the sensor | `01 00`: notifications on, as the Pi asked |
| `gatttool` on the Pi, bypassing BlueZ's GATT client and the logger | Subscribed, accepted, then nothing |
| `btmon` on the Pi while turning | Not one ACL packet from the sensor for 25–41 s; the sensor only asked for its idle ~500 ms interval |
| Interval forced to 48.75 ms from the Pi | Still nothing: not the interval |
| `bluetoothctl` on a laptop, same turning, back to back with the Pi | 38 measurements on the laptop, 0 on the Pi |
| The laptop's working session | The sensor asked for a faster 210–240 ms interval and streamed; its first measurement came two seconds *before* the laptop subscribed, so it keeps per-central state across connections |
| GATT, PHY, data length, encryption | The same on both: unencrypted, LE 1M, the same discovery and subscriptions |
| The Pi's history | It had paired with the sensor in May; its own copy of the key was removed this morning, the sensor's was not |
| The Pi connecting with a random address (`btmgmt privacy on`) | 23 measurements and 16 rotations in 40 s |

## Root cause

The sensor still holds a pairing for the Pi's public address. To a central it
remembers as paired, connecting without encryption, it sends nothing, though
it accepts the connection and the subscription. A central it does not know
gets an ordinary unpaired session.

## Fix

`Privacy = device` in BlueZ's `main.conf`, set by the package: the Pi connects
with a random address, which no sensor has paired with.

## Playbook

1. A connected, subscribed, silent sensor: try a second central with the same
   motion before suspecting the motion or the code.
2. Look for the sensor's own signal that it is streaming (here, its request for
   a faster interval) rather than waiting for data.
3. Sensors keep per-central state. Removing a pairing on the central forgets
   only half of it; a random central address is a clean slate.
4. A radio that reuses cached parameters hides changes: remove the device from
   BlueZ when a test needs a fresh connection.

## Cross-links

- [../ble/link.md](../ble/link.md), [../ble/sensor.md](../ble/sensor.md),
  [notify-before-services-resolved.md](notify-before-services-resolved.md).
