"""purrdometer-display: e-paper UI.

Redraw policy (e-paper is slow and ghosts):
  * redraw only when the picture would change
  * partial refresh normally; a full refresh every N partials and at least hourly
  * while the wheel turns, update the live view every few seconds
  * deep-sleep the panel after a minute without updates (the image stays)
"""

from __future__ import annotations

import argparse
import logging
import os
import queue
import signal
import sys
import time

from . import config as config_mod
from . import data, render, wheels

log = logging.getLogger("purrdometer")


class App:
    def __init__(self, cfg: config_mod.Config):
        from . import hw  # hardware imports only when actually driving the panel

        self.cfg = cfg
        self.taps: queue.Queue[tuple[int, int]] = queue.Queue()
        self.epd = hw.EPaper(rotate180=cfg.rotate180)
        self.touch = hw.Touch(lambda x, y: self.taps.put((x, y)), rotate180=cfg.rotate180)
        self.view = 0
        self.last_image = None
        self.partials = 0
        self.last_full = 0.0
        self.last_update = 0.0

    def show(self, img, force_full=False) -> None:
        if self.last_image is not None and img.tobytes() == self.last_image.tobytes() and not force_full:
            return
        now = time.monotonic()
        if force_full or self.epd.asleep or self.partials >= self.cfg.full_refresh_every or now - self.last_full > 3600:
            self.epd.full(img)
            self.partials, self.last_full = 0, now
        else:
            self.epd.partial(img)
            self.partials += 1
        self.last_image = img
        self.last_update = now

    def frame(self):
        stats = data.load(self.cfg.db, self.cfg.wheel.circumference_m)
        return render.render(stats, self.view), stats

    def run(self) -> None:
        img, stats = self.frame()
        self.show(img, force_full=True)
        next_data = 0.0
        while True:
            now = time.monotonic()
            active = stats.live.active(time.time())
            timeout = max(0.0, next_data - now)
            try:
                x, y = self.taps.get(timeout=timeout)
                tab = render.tab_at(x, y)
                if tab is not None and tab != self.view:
                    self.view = tab
                    next_data = 0  # redraw now
                continue
            except queue.Empty:
                pass
            img, stats = self.frame()
            self.show(img)
            active = stats.live.active(time.time())
            next_data = time.monotonic() + (self.cfg.active_refresh_s if active else self.cfg.idle_refresh_s)
            if not active and time.monotonic() - self.last_update > 60:
                self.epd.sleep()

    def close(self) -> None:
        self.epd.close()


def preview(cfg: config_mod.Config, out_dir: str, scale: int = 3) -> None:
    from PIL import Image

    os.makedirs(out_dir, exist_ok=True)
    stats = data.load(cfg.db, cfg.wheel.circumference_m)
    for i, name in enumerate(render.VIEWS):
        img = render.render(stats, i).convert("L").resize((render.W * scale, render.H * scale), Image.NEAREST)
        path = os.path.join(out_dir, f"{i}-{name.lower().replace(' ', '')}.png")
        img.save(path)
        print(path)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="purrdometer-display")
    ap.add_argument("--config", default=config_mod.DEFAULT_PATH)
    ap.add_argument("--db", help="override database path")
    ap.add_argument("--preview", metavar="DIR", help="render every view to PNGs and exit (no hardware)")
    ap.add_argument("--demo-db", metavar="PATH", help="create a fake-history database at PATH and use it")
    ap.add_argument("--wheels", action="store_true", help="list the known wheel models and exit")
    ap.add_argument("--check-config", action="store_true", help="print the wheel the config resolves to; non-zero on errors")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args(argv)
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO, format="%(levelname)s %(message)s")

    cfg = config_mod.load(args.config)
    if args.wheels:
        for key, w in wheels.PRESETS.items():
            print(f"{key:12} {wheels.describe(w, cfg.units)}")
        return 0
    if args.check_config:
        print(f"{args.config}: {wheels.describe(cfg.wheel, cfg.units)}")
        for e in cfg.errors:
            print(f"error: {e}", file=sys.stderr)
        return 1 if cfg.errors else 0
    if args.demo_db:
        data.make_demo_db(args.demo_db)
        cfg.db = args.demo_db
    if args.db:
        cfg.db = args.db
    if args.preview:
        preview(cfg, args.preview)
        return 0

    app = App(cfg)
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    try:
        app.run()
    finally:
        app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
