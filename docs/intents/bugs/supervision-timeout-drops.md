# Case study: the link that dropped every 7–16 seconds

> A bug case study, where history is told on purpose. The present-tense rules
> it produced are in [../ble/link.md](../ble/link.md).

## Symptom

The sensor connected, stayed up for 7 to 16 seconds, and dropped, over and
over. The prototype logged it as unstable and erroring. A phone talking to the
same sensor had no trouble.

## Evidence

| Source | Fact |
|---|---|
| `btmon`, every drop | `Disconnect Complete`, reason `Connection Timeout (0x08)`: the controller heard nothing from the sensor for a whole supervision timeout |
| `btmon`, after connecting | The sensor's L2CAP Connection Parameter Update asked for interval 143–167 (~180–210 ms) and timeout multiplier 42 (420 ms); BlueZ accepted it |
| `/var/lib/bluetooth/…/info` | `[ConnectionParameters] Timeout=42` cached, so every new connection started with 420 ms too |
| Signal strength | −32 to −42 dBm: not range |
| The radio | Pi Zero 2 W, CYW43436: one 2.4 GHz radio and antenna for WiFi and BLE. WiFi on 2.4 GHz channel 6, power-save on |

## Root cause

A 420 ms supervision timeout on a ~206 ms interval allows two missed
connection events. The shared radio regularly gives BLE slots to WiFi, and
WiFi power-save makes those losses periodic, so two in a row happened within
seconds.

## Fix

A 6 s LE supervision timeout as BlueZ's default, cached parameters cleared,
WiFi power-save off, all applied by the package. With a 6 s link the sensor
itself asked for 6 s and a ~500 ms interval. The link then held for every
session observed, against drops every 7–16 s before.

## Playbook

1. Read the disconnect reason before touching code. `0x08` is the link
   layer; no client code causes it.
2. Compare the supervision timeout with the connection interval: fewer than
   about six intervals is fragile on any radio, and fatal on a shared one.
3. Check BlueZ's cached per-device parameters; they outlive config changes.
4. Suspect coexistence on any combo chip before suspecting range.

## Cross-links

- [../ble/link.md](../ble/link.md), [../workspace/packaging.md](../workspace/packaging.md),
  the `debugging-discipline` skill.
