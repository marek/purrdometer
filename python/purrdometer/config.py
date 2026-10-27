from __future__ import annotations

import configparser
import logging
from dataclasses import dataclass, field

from . import wheels

DEFAULT_PATH = "/etc/purrdometer/purrdometer.conf"

log = logging.getLogger(__name__)


@dataclass
class Config:
    db: str = "/var/lib/purrdometer/purrdometer.db"
    wheel: wheels.Wheel = field(default_factory=lambda: wheels.PRESETS[wheels.DEFAULT])
    units: str = wheels.DEFAULT_UNITS  # how wheel sizes are shown, and what a bare diameter means
    rotate180: bool = False
    full_refresh_every: int = 20   # partial refreshes between full (anti-ghosting) refreshes
    active_refresh_s: float = 5    # how often the live screen updates while the wheel turns
    idle_refresh_s: float = 60     # how often to re-check data when idle
    web_bind: str = "0.0.0.0"
    web_port: int = 8080
    errors: list[str] = field(default_factory=list)


def load(path: str = DEFAULT_PATH) -> Config:
    cfg = Config()
    p = configparser.ConfigParser(inline_comment_prefixes=("#", ";"))
    if not p.read(path):
        return cfg
    units = p.get("wheel", "units", fallback=wheels.DEFAULT_UNITS).strip().lower()
    if units in wheels.DISPLAY_UNITS:
        cfg.units = units
    else:
        cfg.errors.append(f"[wheel] units must be one of {', '.join(wheels.DISPLAY_UNITS)}, not {units!r}")
    try:
        cfg.wheel = wheels.resolve(p.get("wheel", "model", fallback=None), p.get("wheel", "diameter", fallback=None),
                                   cfg.units)
    except ValueError as e:
        # Keep running on the default wheel; --check-config reports this as a failure.
        cfg.errors.append(f"[wheel] {e}")
        log.error("%s: [wheel] %s; using %s", path, e, cfg.wheel.name)
    cfg.rotate180 = p.getboolean("display", "rotate180", fallback=cfg.rotate180)
    cfg.full_refresh_every = p.getint("display", "full_refresh_every", fallback=cfg.full_refresh_every)
    cfg.active_refresh_s = p.getfloat("display", "active_refresh_s", fallback=cfg.active_refresh_s)
    cfg.idle_refresh_s = p.getfloat("display", "idle_refresh_s", fallback=cfg.idle_refresh_s)
    cfg.web_bind = p.get("web", "bind", fallback=cfg.web_bind)
    cfg.web_port = p.getint("web", "port", fallback=cfg.web_port)
    return cfg
