# Wheels and distance

> Owns the wheel catalogue, wheel profiles, custom diameters, units, and how
> distance and speed are derived from rotations.

## Requirement

Setting up a wheel is one choice: pick a known model, or give the measured
diameter in whatever unit is at hand. A wheel is a **profile**, and every
rotation is stored under the profile in use when it was counted
([storage.md](storage.md)). Correcting a profile recomputes the distance of
its own rotations; a new wheel is a new profile, and the history of the old
one keeps its distance. No rotation is lost or moved by either.

## Profiles

`/var/lib/purrdometer/profiles.conf`, written by the settings page
([../web/settings.md](../web/settings.md#wheels)):

```ini
[profiles]
current = 2

[profile.1]
name = Living room G5
preset = ferris-g5

[profile.2]
name = Big wheel
preset = grand-ferris
diameter = 152.4cm
```

| Subject | Rule |
|---|---|
| A profile | A number, a name, and a `preset` from the catalogue, a `diameter`, or both; a `diameter` (measured) wins over the preset's, and a profile with no preset is a custom wheel |
| Current | `[profiles] current` names the profile new rotations are stored under |
| Numbers | Never reused: a profile's number is what history refers to |
| Correcting | Changing a profile's name, preset or diameter changes the distance of every rotation stored under it, and no other |
| A new wheel | A new profile made current; the rotations already stored stay under the old one |
| Switching | Making another profile current; the sensor's count carries on across the switch ([accounting.md](accounting.md)), so a run cut by a switch has its turns before the switch under the old profile and after it under the new one, each once |
| Deleting | Only a profile no rotation is stored under, and never the current one |
| Seeding | Without the file, there is one profile, number 1, from `[wheel] model` and `diameter` in `/etc/purrdometer/purrdometer.conf`, or from an older `settings.conf`; `purrdometer-web` writes the file from it when it starts. Once the file exists, `[wheel] model` and `diameter` are not read |
| Readers | The display and the web read the profiles for distance; the logger reads only `current` |

## Distance and speed

## Rules

| Subject | Rule |
|---|---|
| Distance | For each profile, its rotations × π × its running diameter; the sum over profiles for any total, day or bar |
| Speed | rpm × π × the current profile's running diameter × 60 ÷ 1000 km/h |
| Running diameter | The diameter of the surface the cat runs on: the inside of the track. A wheel's outer size is not used |
| Where a wheel is set | The settings page's wheels ([../web/settings.md](../web/settings.md#wheels)); `[wheel] model` and `diameter` in the config file only seed the first profile |
| Default | `ferris-g5` when nothing is set |
| Units setting | `[wheel] units = cm` (default) or `in`: the unit a bare `diameter` number is in, and the unit every wheel size is shown in first (`--list-wheels`, `--check-config`, the web page). Distances are always km |
| Units in a value | `cm`, `mm`, `m`, `in`, `inch`, `inches`, `"`; an explicit unit always wins over the setting. With no unit and no setting in play (the catalogue), a bare number is an error. A profile's diameter is written with its unit |
| Plausibility | Diameters outside 0.5–2 m are an error: cat wheels are roughly 0.9–1.6 m. When a bare number would be plausible in the other unit, the error says so (`'48.5' is 48 cm …; did you mean 48.5in?`) |
| Errors | An unknown model or an unreadable diameter is logged, and the display runs on the default wheel; `purrdometer-display --check-config` prints the resolved wheel and exits non-zero on any error |
| Listing | `purrdometer-display --list-wheels` lists every preset with its diameter and circumference |
| The catalogue | The presets live in `data/wheels.conf`, installed as `/usr/share/purrdometer/wheels.conf`: package data, replaced on upgrade, never edited by a user; a wheel not in it is a profile with a measured diameter. The display and the web read it through the same C++ code |
| Estimated | A preset whose maker publishes no inner diameter carries `estimated = true`; `--list-wheels`, `--check-config` and the web page say so and advise measuring |

## The catalogue

A preset is added by the manufacturer's published inner (running) diameter,
with the page it came from. Where only the outer size is published, the inner
is an estimate and the preset says so.

| Key | Wheel | Running diameter | Outer | Track | Per rotation | Source |
|---|---|---|---|---|---|---|
| `ferris-g5` | Ferris Cat Wheel G5 | 48 in | 51 in | 12.5 in | 3.830 m | [ferriscatwheel.com](https://www.ferriscatwheel.com/pages/ferris-cat-exercise-wheel-gen5) |
| `grand-ferris` | Grand Ferris Cat Wheel | 60 in | 63 in | 15 in | 4.788 m | [ferriscatwheel.com](https://www.ferriscatwheel.com/pages/grand-ferris-cat-exercise-wheel) |
| `one-fast-cat` | One Fast Cat (Gen 7) | 43 in | 48 in | | 3.431 m | [onefastcat.com](https://onefastcat.com/products/cat-wheel) |
| `star-cat-l` | Star Cat Wheel L | ~38.5 in, estimated | 41.7 in | 15.7 in | 3.072 m | [starcatwheel.com](https://starcatwheel.com) |
| `star-cat-xl` | Star Cat Wheel XL | ~44.5 in, estimated | 47.2 in | 15.7 in | 3.551 m | [starcatwheel.com](https://starcatwheel.com) |

A measured diameter is more accurate than a preset when a mat or lining
changes the running surface.
