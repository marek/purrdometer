# The sensor

Mounting the cadence sensor on the wheel, its first connection, the battery,
and changing to another sensor.

## Mounting

- Fix it to the **back side** of the wheel, away from the cat, with the face
  of the sensor parallel to the wheel, the way it sits on a bike's crank arm.
- Put it **close to the axle**, about a crank arm's length out (~15 cm).
  Near the rim, a running cat's speed presses several g on the sensor, which
  may stop it recognising rotations.
- Keep the **battery door reachable**. Glue the sensor's holder to the wheel
  and seat the sensor in it, instead of gluing the sensor down.
- One wheel rotation is one "crank revolution" to the sensor.

## First connection

Nothing to pair. Spin the wheel to wake the sensor; the logger scans for any
cadence sensor, connects to the first it sees and remembers its address in
`/var/lib/purrdometer/sensor`. From then on it uses only that sensor and
reconnects by itself whenever the sensor wakes.

The NOW screen shows `sensor ●` while connected and `sensor ○` while not,
with the battery level beside it.

The sensor sends data only while it detects rotation. A connected sensor on a
still wheel is silent, and that is normal.

## Battery

The battery level is read on each connection and shown on NOW. If a
battery change resets the sensor's counter, the logger sees the jump and
carries on without counting it.

## Changing sensors

```sh
sudo rm /var/lib/purrdometer/sensor
sudo systemctl restart purrdometer-logger
```

Then spin the wheel with the new sensor on it. The history is kept.
