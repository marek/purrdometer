# Accounting

> Owns how measurements become rotations: deltas, wrap-around, resets,
> rotations made while the logger was down, and rpm. The measurement format is
> owned by [../ble/sensor.md](../ble/sensor.md).

## Requirement

Every real rotation is counted once. No rotation is invented by a counter
that jumped, wrapped or reset. The turns the sensor itself misses when the
wheel starts are estimated by two settings and kept apart from the measured
ones.

## Rules

| Case | Rule |
|---|---|
| First measurement ever | Adds nothing; it is the baseline |
| Next measurement | Delta = current − previous revolutions, modulo the counter's range: 2¹⁶ for a cadence sensor's crank counter, 2³² for a speed sensor's wheel counter |
| Plausibility | A delta larger than the wall-clock time since the previous measurement allows, at a fixed maximum rotation rate plus a small slack, is a discontinuity, and the current measurement becomes the new baseline. The maximum is a named constant in the logger, set far above any cat (5 rev/s, ~70 km/h on the G5) |
| Sensor restarted (counter went backwards: 509 → 8) | A delta of more than half the counter's range (2¹⁵ crank, 2³¹ wheel) is the counter going back, however long the gap: after a night asleep, 10 → 5 would otherwise pass as 65,531 turns, and did once (2026-10-28). A restarted sensor counts from 0, so its whole count is rotations made since the restart: they are added, when that count is itself plausible for the time since the previous measurement. A loose battery or a battery change costs no rotations |
| Implausible jump forwards (100 → 1100 in a second) | Garbage, not motion: adds nothing, becomes the new baseline |
| Logger restarted, sensor kept counting | The previous measurement and its wall time are kept in `state` ([storage.md](storage.md)), so the delta across the gap is counted, within plausibility |
| Rotations across a gap | Attributed to the minute of the measurement that reveals them; when within the gap they happened is unknown |
| Wrap ambiguity | 2¹⁶ rotations (~250 km on the G5) unseen in one gap are lost on a cadence sensor; no gap is that long in practice |
| Sensor switched | The previous measurement is forgotten; the new sensor's first measurement is a new baseline |
| Repeated measurement, no new rotation | Delta 0; rpm 0; stored only as state |

## Turns the sensor misses

Measured on the Garmin cadence sensor on the outside of a G5 (2026-10-27):

| Start | Turned | Counted |
|---|---|---|
| Rocking a quarter turn, no full turn | 0 | Does not wake |
| Two single turns 30 s apart | 2 | Does not wake |
| 3 in a row, asleep | 3 | Wakes, counts 0, its count back at 0 |
| 12 in a row, asleep | 12 | 10 |
| 5, right after that wake, before it had counted a turn | 5 | 3 |
| 12, awake and already counting | 12 | 12 |
| 12 in a row, asleep, again | 12 | 10 |

After waking, the sensor misses about its first 2 turns, whether they come
in the spin that woke it or the next one; once it has counted a turn it
misses none, however long it stopped. So one setting adds estimated turns:
`[sensor] wake_turns` (default 2, a whole number from 0 to 10), added at
the first turn the sensor counts after waking (a new connection). On the
first connection after the logger starts it is added only when the sensor's
count went back, so a logger restart under an awake sensor adds nothing.

Estimated turns go to the minute they
were added in, in `minutes.estimated` ([storage.md](storage.md)), never into
`revs`; every total, chart and distance uses `revs + estimated`, and the API
shows the estimated part on its own. Setting it to 0 gives the measured
count only.

## Rate

rpm = delta × 60 × 1024 ÷ (current − previous crank event time, modulo 2¹⁶),
using the sensor's own event times rather than arrival times, so radio and
scheduling jitter does not show in the speed. Unknown when the event-time
difference is 0 or the gap is a minute or more.

## Proven by

`purrdometer-logger --selftest`: baseline, one rotation in 1 s = 60 rpm,
16-bit wrap, a restarted sensor's count added, an implausible restart count
refused, implausible jump, rotations kept across an hour offline, no motion;
wake turns at the first counted turn of a new connection and not on a logger
restart, none after a stop, off at 0, totals with estimates.
