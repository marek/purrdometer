# Wheel presets and a web dashboard

Date: 2026-10-27 12:36 EDT

## Context

Owners of other wheels should be able to pick theirs: presets for the Ferris
G5, the Grand Ferris, One Fast Cat and the Star Cat L and XL, by their running
(inner) diameter, checked against the makers' pages. The same history should
also be on a web page on the home network, from a C++ server light enough
for a Pi Zero 2 W, with `.page` files that call C++ directly. The rules are in [data/wheels.md](../intents/data/wheels.md),
[web/dashboard.md](../intents/web/dashboard.md) and
[web/pages.md](../intents/web/pages.md).

## Tasks

- [x] Sizes checked against the makers: Ferris G5 48/51", Grand Ferris 60/63", One Fast Cat Gen 7 43/48"; Star Cat publishes only 41.7/47.2" outer, so its inners are estimates and marked as such
- [x] Presets in `data/wheels.ini`, read by the Python display and the C++ dashboard alike; parsing tested in both
- [x] `pagec`: `.page` files compiled to C++ functions, with `#line` mapping
- [x] `purrdometer-web` on cpp-httplib: the dashboard, `/api/stats`, `/healthz`; `--selftest` through a real database
- [x] Page checked by eye from a headless screenshot; ~35 ms a page, ~9 MB RSS on the Pi
- [x] Hardened `purrdometer-web.service` in the package; installed as an upgrade; reachable at `http://whiskerwheel:8080/`
- [x] Intents, guide, README and skills updated
- [x] `[wheel] units = cm | in`: a bare diameter is in those units, sizes are shown in them first, cm by default; implausible sizes refused with a hint at the other unit; tested in Python and C++
- [ ] The dashboard looks right on a phone and a laptop (the user)
- [x] The dashboard is open to the home network: decided
