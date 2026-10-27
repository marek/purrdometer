# The BLE link

> Owns keeping the link to the sensor up: the supervision timeout, WiFi
> coexistence, bonding, reconnecting and discovery. What travels over the link
> is owned by [sensor.md](sensor.md).

## Requirement

Once the sensor is awake and in range, the link stays up through ordinary
radio loss and comes back by itself after the sensor sleeps, the logger
restarts or BlueZ restarts. Losing a few seconds of radio must never drop the
link.

## Link parameters

| Subject | Rule |
|---|---|
| Supervision timeout | 6 s (`600` × 10 ms), set as BlueZ's LE default in `main.conf` by the package ([../workspace/packaging.md](../workspace/packaging.md)) |
| Why 6 s | The sensor asks for a ~200–500 ms connection interval. At the kernel default of 420 ms, two missed connection events end the link; 6 s rides out a dozen or more ([../bugs/supervision-timeout-drops.md](../bugs/supervision-timeout-drops.md)) |
| Connection interval | The sensor's choice, accepted as asked: it sets the sensor's battery life |
| The sensor's requests | The sensor asks for whatever supervision timeout the link started with, so starting at 6 s is enough; no runtime override is needed |
| Cached parameters | BlueZ stores each device's last negotiated parameters and uses them for the next connection; the package clears stale ones when it first sets the default |

## Coexistence

The Pi Zero 2 W's CYW43436 has one 2.4 GHz radio and antenna for both WiFi
and BLE. A slot the WiFi side takes is a BLE connection event lost. WiFi
power-save makes those losses regular (beacon wake-ups, buffered bursts) and
so able to line up with BLE events several times in a row. WiFi power-save is
off ([../workspace/packaging.md](../workspace/packaging.md)). The long
supervision timeout is the fix that matters; power-save off reduces how
often it is tested.

## Security

The sensor is used **unbonded**, the way the phone app talks to it: no
pairing, no encryption. The device is marked `Trusted` so BlueZ keeps its
object between connections.

The Pi connects with a **random address** (`Privacy = device` in BlueZ's
`main.conf`, set by the package), never its own. A sensor remembers each
central it has met: one that was once paired with the Pi's own address sends
it nothing over an unencrypted link, connected and subscribed or not, while a
central it does not know gets the measurements
([../bugs/silent-to-a-central-it-remembers.md](../bugs/silent-to-a-central-it-remembers.md)).
A random address is unknown to every sensor, whatever was paired with this Pi
before.

**The alternative, pairing.** The Bluetooth-standard way to satisfy a sensor
that insists on encryption is to pair with it (`bluetoothctl pair`), after
which BlueZ encrypts every connection with the stored key. It is not used:
a key on the Pi must match the one the sensor holds, so a reinstall, a new
SD card or a sensor paired elsewhere breaks the link until someone pairs
again by hand, while privacy needs nothing. Switching to it means pairing the
sensor once, removing `Privacy = device`, and having the logger treat a
missing key as a reason to pair rather than as a plain connection.
Clearing the pairing on the sensor instead (on Garmin sensors, usually by
taking the battery out) fixes one sensor until it is paired again.

## Connecting

| State | The logger |
|---|---|
| No saved sensor | Scans for any device advertising the CSC service, saves the first one's address ([sensor.md](sensor.md#which-sensor)) |
| Saved sensor, BlueZ knows it | Calls `Connect`; if that fails (the sensor is asleep and not advertising), scans until it advertises again |
| Saved sensor, BlueZ forgot it | Scans for that address |
| Connected | Waits for `ServicesResolved`, then starts notifications ([../bugs/notify-before-services-resolved.md](../bugs/notify-before-services-resolved.md)). A sensor that detects rotation asks for a faster connection interval (about 210–240 ms, against about 500 ms idle) and streams; one that is connected and silent is not turning |
| Disconnected | Marks the database `connected = 0` and starts over |

Errors back off exponentially up to 30 s. The notification session belongs to
the logger's D-Bus connection: BlueZ ends it when that connection closes, so
only a long-lived process can hold it.

## Out of scope

More than one sensor at a time, and a sensor shared with another central
(the phone app) while the logger holds it.
