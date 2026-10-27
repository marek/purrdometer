# Wheels and distance

> Owns wheel presets, custom diameters, units, and how distance and speed are
> derived from rotations.

## Requirement

Setting up a wheel is one line: pick a known model, or give the measured
diameter in whatever unit is at hand. Changing it recomputes all history,
because only rotations are stored ([storage.md](storage.md)).

## Rules

| Subject | Rule |
|---|---|
| Distance | rotations × π × running diameter |
| Speed | rpm × π × running diameter × 60 ÷ 1000 km/h |
| Running diameter | The diameter of the surface the cat runs on: the inside of the track. A wheel's outer size is not used |
| Config | From the dashboard's gear ([../web/settings.md](../web/settings.md)) or the config file: `[wheel] model = <key>`; `[wheel] diameter = <length>` overrides the model's diameter; `[wheel] units` ([../../guide/configuration.md](../../guide/configuration.md)) |
| Default | `ferris-g5` when nothing is set |
| Units setting | `[wheel] units = cm` (default) or `in`: the unit a bare `diameter` number is in, and the unit every wheel size is shown in first (`--list-wheels`, `--check-config`, the web page). Distances are always km |
| Units in a value | `cm`, `mm`, `m`, `in`, `inch`, `inches`, `"`; an explicit unit always wins over the setting. With no unit and no setting in play (the presets file), a bare number is an error |
| Plausibility | Diameters outside 0.5–2 m are an error: cat wheels are roughly 0.9–1.6 m. When a bare number would be plausible in the other unit, the error says so (`'48.5' is 48 cm …; did you mean 48.5in?`) |
| Errors | An unknown model or an unreadable diameter is logged, and the display runs on the default wheel; `purrdometer-display --check-config` prints the resolved wheel and exits non-zero on any error |
| Listing | `purrdometer-display --list-wheels` lists every preset with its diameter and circumference |
| One owner | The presets live in `data/wheels.ini` (installed as `/usr/share/purrdometer/wheels.ini`); the display and the web dashboard read it through the same C++ code |
| Estimated | A preset whose maker publishes no inner diameter carries `estimated = true`; `--list-wheels`, `--check-config` and the web page say so and advise measuring |

## Presets

A preset is added by the manufacturer's published inner (running) diameter,
with the page it came from. Where only the outer size is published, the inner
is an estimate and the preset says so.

| Key | Wheel | Running diameter | Outer | Track | Per rotation | Source |
|---|---|---|---|---|---|---|
| `ferris-g5` | Ferris Cat Wheel G5 | 48 in | 51 in | 12.5 in | 3.830 m | [ferriscatwheelinternational.com](https://ferriscatwheelinternational.com/wheel.php) |
| `grand-ferris` | Grand Ferris Cat Wheel | 60 in | 63 in | 15 in | 4.788 m | [ferriscatwheel.com](https://www.ferriscatwheel.com/pages/grand-ferris-cat-exercise-wheel) |
| `one-fast-cat` | One Fast Cat (Gen 7) | 43 in | 48 in | | 3.431 m | [onefastcat.com](https://onefastcat.com/products/cat-wheel) |
| `star-cat-l` | Star Cat Wheel L | ~38.5 in, estimated | 41.7 in | 15.7 in | 3.072 m | [starcatwheel.com](https://starcatwheel.com) |
| `star-cat-xl` | Star Cat Wheel XL | ~44.5 in, estimated | 47.2 in | 15.7 in | 3.551 m | [starcatwheel.com](https://starcatwheel.com) |

A measured diameter is more accurate than a preset when a mat or lining
changes the running surface.
