# Hardening after the codebase review

Date: 2026-10-28 16:35 EDT

## Context

purrdometer's production source is divided cleanly by program and job, and its
intent corpus states the main data, BLE, display, web and packaging contracts
well. The next body of work hardens the places where failures cross those
boundaries: a measurement reaching SQLite, a D-Bus callback outliving its
sensor link, an unprivileged setup request reaching the root helper, concurrent
web saves, and package changes to the host system. Each fix keeps the existing
user-visible behaviour unless the owning intent needs a more precise failure
contract. The existing raw-sensor-log plan is separate.

## Review findings

### High: a storage error can lose rotations and poison later writes

`Store::record()` opens a transaction without a rollback path
([store.cpp](../../src/logger/store.cpp#L132-L154)). If any statement after
`BEGIN` fails, the connection can remain in a transaction and reject later
`BEGIN`s. `RevolutionCounter::onMeasurement()` catches the error but still
advances the in-memory sample, wall time and missed-turn state
([revolution_counter.cpp](../../src/logger/revolution_counter.cpp#L55-L81)).
The failed delta is then treated as already accounted on the next notification.
A full disk, SQLite I/O error or failed commit can therefore lose rotations and
leave writes broken until restart. This conflicts with the accounting contract
that a rotation is neither lost nor counted twice.

### High: the root helper follows a user-controlled SSH-key path

`LoginAccount::addSshKey()` runs as root and opens, changes the mode of and
changes ownership of `<home>/.ssh/authorized_keys` by pathname
([login_account.cpp](../../src/setup/login_account.cpp#L41-L51)). A login user
can make `.ssh` or `authorized_keys` a symlink to another root-writable path.
The helper would follow it, append the submitted key and chown the target. The
privileged boundary needs descriptor-relative, no-symlink filesystem
operations and tests against hostile path layouts.

### High: sensor-link ownership crosses threads without protection

The BlueZ `InterfacesRemoved` callback reads and dereferences `Monitor::link_`
on the asynchronous D-Bus event-loop thread
([monitor.cpp](../../src/logger/monitor.cpp#L13-L24)). The main loop resets and
replaces the same `std::unique_ptr` when the chosen sensor changes
([monitor.cpp](../../src/logger/monitor.cpp#L69-L95)). `SensorLink`'s flags are
atomic, but ownership is not. A sensor switch coinciding with object removal can
race `link_.reset()` against `link_->path()` or `link_->lost()`, which is
undefined behaviour.

### Medium-high: command failure can be mistaken for an unconfigured Pi

`NetworkManager::hasWifiProfile()` ignores the command status and interprets
empty stdout as no saved profile
([network_manager.cpp](../../src/setup/network_manager.cpp#L31-L39)). A timeout
or transient NetworkManager failure can therefore start setup mode on a
configured device, contrary to the rule that setup never takes a working Pi
off its network by itself.

### Medium: setup publishes success without proving each operation succeeded

`SetupMode::openHotspot()` ignores the result of `startHotspot()` and publishes
`Hotspot` regardless ([setup_mode.cpp](../../src/setup/setup_mode.cpp#L72-L87)).
The password, SSH key and SSH enablement results are also discarded
([setup_mode.cpp](../../src/setup/setup_mode.cpp#L14-L25)). After a successful
WiFi command, an empty address is accepted as a successful join
([network_manager.cpp](../../src/setup/network_manager.cpp#L71-L88),
[nmcli_parse.cpp](../../src/setup/nmcli_parse.cpp#L55-L59)). The page and display
can consequently report a hotspot or completed setup that does not exist or is
only partly applied.

### Medium: the command timeout can still wait forever

`runCommand()` kills the child only while an output pipe remains open, then
calls blocking `waitpid()`
([command.cpp](../../src/setup/command.cpp#L37-L85)). A child that closes stdout
and stderr but continues running reaches `waitpid()` without being killed. The
function also does not handle `fork()` failure or complete a short stdin write.
The root setup service can hang despite the timeout in its command abstraction.

### Medium: concurrent web saves are not serialized

The web server has two request workers
([server.cpp](../../src/web/server.cpp#L15-L28)). Profile and settings writes use
fixed `.tmp` paths without a shared lock
([profiles.cpp](../../src/common/profiles.cpp#L88-L112),
[config.cpp](../../src/common/config.cpp#L230-L256)). Profile changes are also
read-modify-write operations. Overlapping requests can race on the temporary
file, return an error, or silently replace one another's changes. Atomic rename
protects readers from a partial final file; it does not serialize writers.

### Medium: package configuration is not self-repairing

The BlueZ maintainer-script helper returns as soon as its marker exists and
does not verify the setting that follows it
([postinst](../../packaging/debian/postinst#L7-L18)). If a value is edited or
removed while the marker remains, an upgrade leaves the wrong value in place.
The broad claim that purge undoes every system setting is also stronger than
what can be restored: cached connection parameters are deleted and the prior
rfkill state is not recorded
([packaging.md](../intents/workspace/packaging.md#system-settings-the-package-owns)).

### Low-medium: the page compiler can report success with stale output

`pagec` does not check whether an output file opened, wrote or flushed
successfully ([pagec.cpp](../../src/tools/pagec.cpp#L145-L153)). It then exits
zero after both writes ([pagec.cpp](../../src/tools/pagec.cpp#L187-L190)). If an
old generated file exists but is unwritable, the build can use stale generated
C++ after a `.page` change.

### Structural: tests and working rules have begun to drift

The production folders, entry points and class names follow the source-layout
intent well. The web self-test is now 377 lines and combines parsing,
configuration, SQLite, HTTP, wheel, setup and redirect cases. The rule that
each program has one `selftest.cpp` now conflicts with the rule to split files
past about 300 lines
([source-layout.md](../intents/workspace/source-layout.md#classes-and-files)).
The highest-risk paths above have no failure-injection coverage: there is no
Monitor/SensorLink lifecycle test, no failing NetworkManager behind SetupMode,
no SQLite mid-transaction failure, no hostile SSH path and only a happy-path
`runCommand({"cat"}, ...)` test.

The working agreement also says there are three programs and no remote even
though setup is a fourth tested program and the checkout has `origin` and
`whiskerwheel` remotes ([AGENTS.md](../../AGENTS.md#the-triple-helix),
[Makefile](../../Makefile#L80-L86)). The packaging intent says both "three
services" and lists four. These contradictions weaken the rule that intent is
the contract. The absolute ban on historical comments also needs a narrow
exception for migration input schemas: a migration must say what old shape it
accepts without turning the comment into a changelog.

## Order of work

The fixes proceed from data integrity and privilege boundaries through
concurrency and tooling, then reconcile the documentation. Each behavioural
change updates its owning intent and gains a failure-focused test before the
implementation is called complete.

## Tasks

- [ ] Storage contract and tests: define failed-write behaviour in the accounting and storage intents; inject a failure after `BEGIN` and prove no delta is lost, duplicated or allowed to poison later writes
- [ ] Storage implementation: make `Store::record()` transaction-safe and advance the revolution and missed-turn baselines only after the durable write succeeds
- [ ] Root filesystem boundary: write and test a no-symlink, descriptor-relative SSH-key installation path with correct ownership, permissions, deduplication and failure reporting
- [ ] BLE ownership: give the D-Bus callback and main loop one safe ownership mechanism for the active `SensorLink`; test removal during a sensor switch and run the logger test build under ThreadSanitizer where supported
- [ ] Command runner: handle pipe and fork failures, complete stdin writes, enforce the deadline through process exit, kill the process group when required, drain output and test every timeout/failure shape
- [ ] Setup state machine: distinguish command failure from an empty result, require hotspot and address success, propagate login/SSH failures, and prove a configured Pi stays on its network when profile discovery fails
- [ ] Web writes: serialize settings and profile mutations across both workers, use collision-free temporary files, preserve atomic reader-visible replacement and test overlapping requests
- [ ] Package lifecycle: make each BlueZ setting verify and repair its value, narrow or implement the purge guarantees, and test configure/upgrade/remove/purge/reinstall idempotence in an isolated package venue
- [ ] Page compiler: make generated-file writes atomic and fail loudly on open, write, flush or rename errors; add a build-tool self-test for an unwritable destination
- [ ] Test structure: split web test cases and fixtures into focused files while keeping one `--selftest` entry point; add sanitizer targets without weakening the Raspberry Pi GCC 14 build
- [ ] Documentation: reconcile three/four-program wording, remotes, service counts and migration-comment guidance; remove duplicated rules where one intent or skill can own them
- [ ] Full verification: run `make test`, the sanitizer builds, `make preview`, `make serve`, package lifecycle checks and the BLE/setup checks on whiskerwheel; record any physical checks that remain for the user
