---
name: verification
description: How to actually verify a change in purrdometer before calling it done - make test (the four programs' --selftest, pagec and the maintainer scripts), make preview (look at the PNGs), and on-device testing on the Pi (whiskerwheel) via journalctl and btmon - plus the hygiene every test run must keep (nothing left running, nothing left in the tree) and the traps of BLE and display testing. Use before reporting any fix or feature as complete, before ticking a plan box, and before and after any on-device test.
---

# Verification

Compiling means the change did not break the build. `make test`
passing means it did not break what is already covered. Neither means
the change does what was asked.

## Trace a fix to its real endpoint

A fix that writes the right state can still leave what the user sees
unchanged. Trace forward from the code you changed:

- Who reads this state, and when? At the next BLE notification, at the
  next minute boundary, at the next display refresh?
- Does it cross a layer? Then the display sees it only in what the
  SQLite query returns; a value correct in the logger can be invisible
  on screen.
- Is there a cache in between (the display's rendered image buffer, the
  fake-DB fixture values in `make preview`)?

Ask "then what happens?" one step past the code you touched. Don't
claim completeness from "it runs": compare against the BlueZ D-Bus API
reference for BLE handling, and look at the preview PNGs or the
on-device screen for display changes.

## Venues

| Venue | Command | Proves |
|---|---|---|
| All tests | `make test` | Runs `--selftest` of `bin/purrdometer-logger`, `bin/purrdometer-web`, `bin/purrdometer-display` and `bin/purrdometer-setup`, `bin/pagec --selftest`, and `packaging/test-maintainer-scripts.sh` |
| Sanitizers | `make test-address`, `make test-thread` | The same tests built with AddressSanitizer and UndefinedBehaviorSanitizer, or ThreadSanitizer, in `build/<name>/` |
| Display preview | `make preview` | Renders all four screens (NOW, 30 DAYS, CALENDAR, ALL TIME) to `preview/*.png` from a fake DB; open the PNGs and look |
| Web page | `make serve`, open `http://whiskerwheel:8081/` or screenshot it headless | The dashboard over a fake DB; look at it |
| On device — logs | `ssh marek@whiskerwheel 'journalctl -u purrdometer-logger -u purrdometer-display -n 100'` | What the running services report; the first place to check after a .deb install |
| On device — BLE | `ssh marek@whiskerwheel 'sudo purrdometer-btdiag'` | Live btmon output showing BLE advertisement, connection and CSC notification traffic |
| On device — full cycle | Install the .deb, spin the wheel by hand, check journalctl for rotation counts, check the display | End-to-end path: BLE → logger → SQLite → display |
| Package build | `make deb` | Builds the .deb; confirms the packaging scripts, systemd units and postinst are well-formed |

What each kind of change needs:

| Change | Minimum |
|---|---|
| Any C++ code | `make test`; `make deb` if packaging changed |
| A template or the web dashboard | `make test` and `make serve`: look at the page |
| Display code | `make test` and `make preview` — open the PNGs and look |
| BLE handling, logger reconnect logic | `make test` + on-device BLE check (`purrdometer-btdiag`); a code-only `make test` pass is not enough |
| SQLite schema, query logic | `make test` and `make preview`; verify the numbers shown match the fake DB |
| Display layout, fonts, touch bar | `make preview` and look at every affected screen; on-device check owed if the layout is hard to judge from PNGs |
| Packaging, systemd units, postinst | `make deb`; install on whiskerwheel and check journalctl |
| Comments only | No test run: the diff with comments stripped is empty |

A run is repeated only when something it covers changed since it last
passed.

Every run is read whole: its log stays in `tmp/` until it has been
read, and an exit code alone is not a reading — a compiler error or a
selftest assertion passes straight through such a filter.

## On-device feel

- A preview PNG shows layout; it does not show touch response or
  e-paper refresh artefacts. **An on-device check is the only proof of
  real display behaviour.**
- What cannot be shown that way (a sensor in range, actual wheel
  rotations) is owed: it is a plan box that says so, and only the user
  can close it.
- After a change to what is drawn, look at the screen (or the preview
  PNG) and show the user the picture.

## Hygiene for every run

- **Nothing left running.** `make test` cleans up after itself; if a
  test helper process is started manually, kill it by PID when done.
  Never kill by pattern in case another session runs the same binary.
- **Runs quit themselves.** An on-device test that might hang should be
  started under `timeout` on the Pi: a hung display process locks the
  e-paper.
- **Nothing temporary stays in the tree.** Debug prints and test
  commands are removed before the turn ends; another session may
  commit them.
- **Scratch.** Logs and preview PNGs for review go under the repo's
  `tmp/` (the `scratch-space` skill); `make preview` writes to
  `preview/` at the repo root (git-ignored, rewritten on every run).

## Traps in BLE and display testing

- **BLE notifications arrive asynchronously.** A `--selftest` that only
  parses a static byte buffer may pass while live reconnect logic is
  broken; test the reconnect path on the device.
- **The fake DB fixture has fixed data.** `make preview` will always
  produce the same PNGs from the same fixture; a query bug that returns
  zero rows shows as blank screens, not a crash.
- **The e-paper has a full-refresh flicker.** Layout that looks fine in
  a PNG can be unreadable during the refresh cycle on device; the
  partial-refresh path needs an on-device check.
- **WiFi power-save affects BLE.** If the postinst disables WiFi
  power-save but a reinstall does not re-run it, BLE latency may appear
  to regress; check `iwconfig` on the Pi.

## Reporting

Say what was run and what it showed, with the log line or picture that
shows it. Say what was not run and why ("not seen on device"). A box is
ticked for what was verified, not for what was edited.
