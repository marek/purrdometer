---
name: debugging-discipline
description: Lessons from debugging purrdometer - run the exact command that failed, whole, before tracing anything; a repeated report is signal, not flakiness; check the environment before blaming the code and the code before blaming the environment; a second attempt at a timing or state bug should redesign rather than re-patch; fix a bug at the shared layer, not at the call site that reported it; drive a state transition explicitly. Use whenever the user reports a failing command or test, whenever a bug is reported again after a fix, whenever a test fails in a way that looks like a regression, whenever a first fix does not hold, and when hunting BLE disconnects, SQLite corruption or display artefacts.
---

# Debugging discipline

## 1. A repeated report is signal, not noise

When the user reports the same failure again after it was called fixed,
revise the theory; do not explain the report away. They have more reps
than one captured log, and they are the one who has to keep
reproducing it. Go back to the literal request and to the code as it
stands, and start from there — each round that patches the last attempt
instead of the cause produces a different wrong thing.

Trust the report as stated. "The display shows zero rotations after a
BLE reconnect" is a fact to root-cause with `make test`, journalctl
and btmon, not a claim to confirm.

## 2. Run the command that failed, whole, before anything else

When the user says a command fails — `make test`, `make preview`,
`make deb`, `purrdometer-btdiag` — the first step is that command,
exactly as they run it, start to finish, its whole output kept in
`tmp/` and read. It states the failure in its own words.

What it is not:

- **Not the pieces.** Reading the Makefile and re-running its steps
  one by one, or a nearby command (`make` for `make test`, a single
  Python import for the full test suite), runs in a different
  environment and finds nothing: incremental builds have no work to do,
  a Python syntax error surfaces only at import time.
- **Not a summary.** An exit code or a `grep -c FAIL` is not reading
  the output; a Python traceback or a selftest assertion message passes
  straight through such a filter.
- **Not a guess.** Theories about what the command might hit come after
  its output, from it.

Then fix what it said, and run the same command again as the proof.

## 3. Environment or code: check, don't assume — in both directions

- **A failure that looks like a regression may be the environment.**
  Before blaming a code change, check: the BLE sensor out of range, the
  Pi's Bluetooth service in a bad state (`sudo systemctl restart
  bluetooth`), a stale SQLite lock from a crashed logger, WiFi
  power-save re-enabled after an OS update.
- **A failure blamed on the environment may be the code.** "Probably
  the sensor was out of range" is a theory that lets the investigation
  stop; it needs evidence like any other. Check with btmon and see
  whether the sensor is advertising and whether the logger is
  connecting.

## 4. BLE and timing bugs: use the tools built for it

A reconnect loop, missed CSC notifications, or rotation counts that
stop incrementing are timing and state bugs until shown otherwise.

- Run `sudo purrdometer-btdiag` (`btmon` wrapper) on the Pi and read
  the connection, service discovery and notification traffic.
- Check `journalctl -u purrdometer-logger` for D-Bus errors and
  reconnect events.
- A crash in a D-Bus signal handler is usually a lifetime bug: the
  proxy object was destroyed while a callback still held a reference to
  it.
- Suspect any state kept past a BLE disconnection event (a
  characteristic handle remembered from a previous connection, a
  running total not reset on reconnect).

## 5. A second attempt should redesign, not re-patch

If a fix for a reconnect race or "it works sometimes" does not hold on
the next report, ask what design would make the whole class of bug
impossible before reaching for another targeted patch. A reconnect path
that assumes the previous connection's state is still valid cannot be
patched into being safe; resetting all BLE state on every disconnection
event removes the class.

## 6. Fix it at the layer every caller shares

Before wiring a fix into the call site that reported the bug, ask
whether every other call site of the same shape has the same gap. If
one screen's SQLite query returns stale data after a logger restart,
every screen reading the same table has the same gap: the fix belongs
at the query layer, once. If one touch handler leaves the display in a
bad state on error, ask which other handlers do the same — the answer
is usually "all of them".

## 7. Drive a state transition explicitly

A real transition — a BLE connect or disconnect, a SQLite write
completing, a display refresh starting or ending — gets its own
explicit step that the rest waits on, not "leave the state half valid
and let an error path notice". A display refresh that ends while a
touch event is in flight ends through a path that clears its state; it
does not leave partial state for the next refresh to trip over.

## 8. Log enough to see it

When behaviour is wrong and the cause is not visible, add logging for
the transition in question — BLE connection state, CSC notification
values, SQLite row counts — and read it, rather than guessing from the
outcome. Temporary logging comes out of the source before the turn
ends.
