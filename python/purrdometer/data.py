"""Read-only queries against the logger's SQLite database."""

from __future__ import annotations

import datetime as dt
import os
import sqlite3
import time
from dataclasses import dataclass, field

@dataclass
class Live:
    connected: bool = False
    rpm: int = 0
    last_motion: float | None = None  # unix time
    battery: int | None = None

    def active(self, now: float, window: float = 15) -> bool:
        return self.last_motion is not None and now - self.last_motion < window


@dataclass
class Stats:
    circumference_m: float
    total_revs: int = 0
    today_revs: int = 0
    daily: dict[dt.date, int] = field(default_factory=dict)  # every day with motion
    first_day: dt.date | None = None
    live: Live = field(default_factory=Live)

    def km(self, revs: int) -> float:
        return revs * self.circumference_m / 1000

    def kmh(self, rpm: int) -> float:
        return rpm * self.circumference_m * 60 / 1000

    def days(self, start: dt.date, end: dt.date) -> list[tuple[dt.date, int]]:
        n = (end - start).days + 1
        return [(d, self.daily.get(d, 0)) for d in (start + dt.timedelta(i) for i in range(n))]

    def buckets(self, today: dt.date, max_bars: int = 60) -> tuple[str, list[tuple[dt.date, int]]]:
        """History since the first day as at most ~max_bars bars: per day, else week, else month."""
        if not self.first_day:
            return "day", []
        span = (today - self.first_day).days + 1
        if span <= max_bars:
            return "day", self.days(self.first_day, today)
        weekly = span <= 7 * max_bars
        out: dict[dt.date, int] = {}
        for d, r in self.days(self.first_day, today):
            key = d - dt.timedelta(days=d.weekday()) if weekly else d.replace(day=1)
            out[key] = out.get(key, 0) + r
        return ("week" if weekly else "month"), sorted(out.items())


def load(db_path: str, circumference_m: float, today: dt.date | None = None) -> Stats:
    stats = Stats(circumference_m)
    if not os.path.exists(db_path):
        return stats
    con = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True, timeout=5)
    try:
        rows = con.execute(
            "SELECT date(minute * 60, 'unixepoch', 'localtime') AS day, SUM(revs) "
            "FROM minutes GROUP BY day ORDER BY day"
        ).fetchall()
        state = dict(con.execute("SELECT key, value FROM state").fetchall())
    except sqlite3.OperationalError:  # logger hasn't created the schema yet
        return stats
    finally:
        con.close()

    stats.daily = {dt.date.fromisoformat(d): int(r) for d, r in rows if r}
    stats.total_revs = sum(stats.daily.values())
    today = today or dt.date.today()
    stats.today_revs = stats.daily.get(today, 0)
    stats.first_day = min(stats.daily) if stats.daily else None

    def num(key, conv=float):
        try:
            return conv(state[key])
        except (KeyError, ValueError):
            return None

    stats.live = Live(
        connected=state.get("connected") == "1",
        rpm=num("rpm", int) or 0,
        last_motion=num("last_motion"),
        battery=num("battery", int),
    )
    return stats


def make_demo_db(path: str, days: int = 120, seed: int = 7) -> None:
    """Fake history for previews/tests. Never point this at the real database."""
    import random

    rnd = random.Random(seed)
    if os.path.exists(path):
        os.remove(path)
    con = sqlite3.connect(path)
    con.executescript(
        "CREATE TABLE minutes (minute INTEGER PRIMARY KEY, revs INTEGER NOT NULL);"
        "CREATE TABLE state (key TEXT PRIMARY KEY, value TEXT);"
    )
    now = time.time()
    for d in range(days, -1, -1):
        if rnd.random() < 0.15:
            continue  # lazy day
        for _ in range(rnd.randint(1, 6)):
            t = now - d * 86400 - rnd.randint(0, 86400)
            for m in range(rnd.randint(2, 25)):
                con.execute(
                    "INSERT OR REPLACE INTO minutes VALUES (?, ?)",
                    (int(t) // 60 + m, rnd.randint(10, 80)),
                )
    for m in range(25):  # running right now, so the live view is consistent
        con.execute("INSERT OR REPLACE INTO minutes VALUES (?, ?)", (int(now) // 60 - m, rnd.randint(40, 60)))
    con.executemany(
        "INSERT INTO state VALUES (?, ?)",
        [("connected", "1"), ("rpm", "54"), ("last_motion", str(now)), ("battery", "85")],
    )
    con.commit()
    con.close()
