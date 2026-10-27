"""Known wheels and diameter parsing.

Distance is rotations x the running-surface circumference. The running
surface is the inside of the track the cat stands on, so a preset's diameter
is the wheel's *inner* diameter; makers often advertise the outer.

The presets live in wheels.ini, shared with purrdometer-web (C++).
"""

from __future__ import annotations

import configparser
import math
import os
import re
from dataclasses import dataclass
from pathlib import Path

INCH = 0.0254
DEFAULT = "ferris-g5"
INSTALLED_PRESETS = "/usr/share/purrdometer/wheels.ini"
REPO_PRESETS = Path(__file__).resolve().parents[2] / "data" / "wheels.ini"


@dataclass(frozen=True)
class Wheel:
    name: str
    diameter_m: float            # running (inner) diameter
    outer_m: float | None = None
    track_m: float | None = None
    estimated: bool = False      # no published inner diameter; measure for accuracy
    source: str = ""

    @property
    def circumference_m(self) -> float:
        return math.pi * self.diameter_m


_UNITS = {"in": INCH, '"': INCH, "inch": INCH, "inches": INCH, "cm": 0.01, "mm": 0.001, "m": 1.0}
DISPLAY_UNITS = ("cm", "in")    # [wheel] units: how sizes are shown, and what a bare number means
DEFAULT_UNITS = "cm"
PLAUSIBLE_DIAMETER_M = (0.5, 2.0)  # cat wheels are roughly 0.9-1.6 m across


def parse_length(text: str, default_unit: str | None = None) -> float:
    """'48.5in', '48.5"', '123 cm', '1230mm', '1.23m' -> metres. A bare number takes default_unit."""
    m = re.fullmatch(r"\s*([0-9]+(?:\.[0-9]+)?)\s*(in|inch|inches|\"|cm|mm|m)?\s*", text.lower())
    if not m or not (m.group(2) or default_unit):
        raise ValueError(f"not a length with a unit (cm, in, mm, m): {text!r}")
    return float(m.group(1)) * _UNITS[m.group(2) or default_unit]


def parse_diameter(text: str, default_unit: str = DEFAULT_UNITS) -> float:
    """A wheel diameter, refusing sizes no cat wheel has (and hinting at the other unit)."""
    metres = parse_length(text, default_unit)
    lo, hi = PLAUSIBLE_DIAMETER_M
    if not lo <= metres <= hi:
        hint = ""
        bare = re.fullmatch(r"\s*([0-9.]+)\s*", text)
        if bare:
            other = "in" if default_unit == "cm" else "cm"
            if lo <= float(bare.group(1)) * _UNITS[other] <= hi:
                hint = f"; did you mean {bare.group(1)}{other}?"
        raise ValueError(f"{text!r} is {metres * 100:.0f} cm, not a cat wheel's diameter{hint}")
    return metres


def show(metres: float, units: str = DEFAULT_UNITS) -> str:
    """A wheel size in the preferred unit first: '121.9 cm (48.0 in)'."""
    cm, inch = f"{metres * 100:.1f} cm", f"{metres / INCH:.1f} in"
    return f"{inch} ({cm})" if units == "in" else f"{cm} ({inch})"


def load_presets(path: str | os.PathLike | None = None) -> dict[str, Wheel]:
    path = path or os.environ.get("PURR_WHEELS") or (
        INSTALLED_PRESETS if os.path.exists(INSTALLED_PRESETS) else REPO_PRESETS
    )
    p = configparser.ConfigParser(inline_comment_prefixes=("#", ";"))
    if not p.read(path):
        raise FileNotFoundError(f"wheel presets not found: {path}")
    out = {}
    for key in p.sections():
        s = p[key]
        opt = lambda k: parse_length(s[k]) if k in s else None  # noqa: E731
        out[key] = Wheel(s["name"], parse_diameter(s["inner"]), opt("outer"), opt("track"),
                         s.getboolean("estimated", False), s.get("source", ""))
    return out


PRESETS = load_presets()


def resolve(model: str | None, diameter: str | None, units: str = DEFAULT_UNITS) -> Wheel:
    """A measured diameter (a bare number in `units`) wins over the model's; the model defaults to DEFAULT."""
    key = (model or "").strip().lower()
    if key and key not in PRESETS:
        raise ValueError(f"unknown wheel model {model!r}; known: {', '.join(PRESETS)}")
    if diameter:
        base = PRESETS.get(key)
        label = f"{base.name}, measured" if base else "custom wheel"
        return Wheel(label, parse_diameter(diameter, units), base.outer_m if base else None,
                     base.track_m if base else None)
    return PRESETS[key or DEFAULT]


def describe(w: Wheel, units: str = DEFAULT_UNITS) -> str:
    text = f"{w.name}: running diameter {show(w.diameter_m, units)}"
    if w.outer_m:
        text += f", outer {show(w.outer_m, units).split(' (')[0]}"
    text += f", {w.circumference_m:.3f} m per rotation"
    if w.estimated:
        text += " (inner diameter estimated: measure it and set diameter for accuracy)"
    return text
