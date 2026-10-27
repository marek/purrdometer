# C++26 on trixie, and settings from the dashboard

Date: 2026-10-27 14:20 EDT

## Context

The C++ programs are written in C++26, which needs GCC 14: whiskerwheel moves
from Raspberry Pi OS bookworm to trixie, and the code follows trixie's
libraries (sdbus-c++ 2, cpp-httplib 0.18). Builds move off the Pi Zero into a
trixie arm64 container, where they take seconds instead of minutes. The
dashboard gains a gear page that sets the wheel and the screen, saving to a
file that wins over `/etc`, with who may save set in `/etc` and the home
network by default. The rules are in
[web/settings.md](../intents/web/settings.md),
[workspace/build-and-run.md](../intents/workspace/build-and-run.md) and
[workspace/packaging.md](../intents/workspace/packaging.md).

## Tasks

- [x] Code pushed to `github.com/marek/purrdometer` and the Pi's state backed up before the upgrade
- [x] bookworm fully updated (new Bluetooth firmware, BlueZ, kernel), then upgraded to trixie (GCC 14, BlueZ 5.82, kernel 6.18); the BlueZ timeout and WiFi power-save setting kept
- [x] Logger ported to sdbus-c++ 2; `-std=c++26`; `std::format`, `std::chrono` calendar, ranges, `starts_with` where they read better
- [x] `packaging/build-in-container.sh`: build, test and package in a trixie arm64 container
- [x] The package unblocks Bluetooth (rfkill), which the upgrade left blocked
- [x] Gear page: wheel (cm/in toggle, preset, measured diameter) and screen settings, saved to `settings.ini`; `settings_access` lan (default), pin, open or off; form token; tested end to end over HTTP
- [x] The screen picks up saved settings before its next redraw
- [x] Intents, guide, README and skills updated
- [ ] Saving the wheel from a phone changes the dashboard and the screen (the user)
- [ ] History starts from the files as they were before this work, and unused prototype code is removed
