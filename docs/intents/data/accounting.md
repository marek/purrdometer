# Accounting

> Owns how measurements become rotations: deltas, wrap-around, resets,
> rotations made while the logger was down, and rpm. The measurement format is
> owned by [../ble/sensor.md](../ble/sensor.md).

## Requirement

Every real rotation is counted once. No rotation is invented by a counter
that jumped, wrapped or reset.

## Rules

| Case | Rule |
|---|---|
| First measurement ever | Adds nothing; it is the baseline |
| Next measurement | Delta = current − previous crank revolutions, modulo 2¹⁶ |
| Plausibility | A delta larger than the wall-clock time since the previous measurement allows, at a fixed maximum rotation rate plus a small slack, is a reset: it adds nothing and becomes the new baseline. The maximum is a named constant in the logger, set far above any cat (5 rev/s, ~70 km/h on the G5) |
| Counter went backwards | Modulo arithmetic makes it a huge delta, so it is a reset by the rule above |
| Logger restarted, sensor kept counting | The previous measurement and its wall time are kept in `state` ([storage.md](storage.md)), so the delta across the gap is counted, within plausibility |
| Rotations across a gap | Attributed to the minute of the measurement that reveals them; when within the gap they happened is unknown |
| Wrap ambiguity | 2¹⁶ rotations (~250 km on the G5) unseen in one gap are lost; no gap is that long in practice |
| Repeated measurement, no new rotation | Delta 0; rpm 0; stored only as state |

## Rate

rpm = delta × 60 × 1024 ÷ (current − previous crank event time, modulo 2¹⁶),
using the sensor's own event times rather than arrival times, so radio and
scheduling jitter does not show in the speed. Unknown when the event-time
difference is 0 or the gap is a minute or more.

## Proven by

`purrdometer-logger --selftest`: baseline, one rotation in 1 s = 60 rpm,
16-bit wrap, backwards counter, implausible jump, rotations kept across an
hour offline, no motion.
