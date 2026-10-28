# Configuration

The easiest way is the gear on the [web dashboard](web.md#settings). The
screen, sensor and web settings can also be set in
`/etc/purrdometer/purrdometer.conf`; what the gear saves goes to
`/var/lib/purrdometer/settings.conf` and wins over that file. Your wheels are
in `/var/lib/purrdometer/profiles.conf`, set from the gear. After editing the
config file, check it and restart the screen and the dashboard:

```sh
purrdometer-display --check-config
sudo systemctl restart purrdometer-display purrdometer-web
```

`--check-config` prints the wheel in use and its distance per rotation, and
fails on any mistake.

## Wheels

Distance is rotations × the circumference of the surface the cat runs on. Each
of your wheels is a **profile**: a name, a model, and a measured diameter if
you have one. Every rotation is stored under the wheel in use when it was
counted, so:

- **Correcting a wheel** (the wrong model, a better measurement) changes the
  distance of that wheel's rotations, history included, and no other.
- **A new wheel** is a new profile. The old wheel keeps its distance, and
  rotations from then on are the new one's. Add it when you move the sensor.
- Switching back to an earlier wheel is **Use this wheel**.

Wheels are set on the settings page ([web](web.md#settings)). On a new
install, the first one comes from `[wheel]` here:

```ini
[wheel]
model = ferris-g5
```

The models:

| `model` | Wheel | Running diameter | Per rotation |
|---|---|---|---|
| `ferris-g5` | Ferris Cat Wheel G5 | 121.9 cm (48 in) | 3.83 m |
| `grand-ferris` | Grand Ferris Cat Wheel | 152.4 cm (60 in) | 4.79 m |
| `one-fast-cat` | One Fast Cat (Gen 7) | 109.2 cm (43 in) | 3.43 m |
| `star-cat-l` | Star Cat Wheel L | about 97.8 cm (38.5 in) | 3.07 m |
| `star-cat-xl` | Star Cat Wheel XL | about 113.0 cm (44.5 in) | 3.55 m |

`purrdometer-display --list-wheels` prints the same list. Star Cat publishes only
the outside size of its wheels (106 cm and 120 cm, 41.7 in and 47.2 in), so their running
diameters are estimates: measure yours and set `diameter` for accurate
distances.

A measured diameter is the running surface: the inside of the track, not the
outside of the wheel. It wins over the model's, and a wheel that isn't listed
needs only that.

`units` is `cm` (the default) or `in`: how wheel sizes are shown, and what a
plain number typed as a diameter means. You can always write the unit instead:
`48.5in` or `1.23m` works whatever `units` says. A diameter is kept in cm
whatever is shown, so switching `units` never changes a distance.

A size no cat wheel has (outside 50–200 cm) is refused. If you type `48.5`
with `units = cm`, the page answers `did you mean 48.5in?` instead of using a
48 cm wheel.

With no wheel set the Ferris G5 is used. Distances are always in km.

To add a wheel model for everyone, add a section to `data/wheels.conf` with
the maker's inner diameter and where it came from. The screen and the
dashboard both read that file ([development](development.md)).

## Display

```ini
[display]
rotate180 = false          # flip picture and touch for a HAT mounted upside down
full_refresh_every = 20    # fast updates between flashing, ghost-clearing ones
active_refresh_s = 5       # seconds between updates while the wheel turns
idle_refresh_s = 60        # seconds between checks while it does not
```

## Web

```ini
[web]
bind = 0.0.0.0             # 127.0.0.1 to allow only the Pi itself
port = 8080
settings_access = lan      # who may save from the gear: lan, pin, open or off
#settings_pin = 2468       # for settings_access = pin
```

See [the web dashboard](web.md).
