# Build and run

> Owns the build, the testing venues and running on the Pi. The package is
> owned by [packaging.md](packaging.md).

## Requirement

Everything builds with Debian trixie packages only (no pip, no vendored
libraries, no PHP), either in a trixie arm64 container
(`packaging/build-in-container.sh`, the normal way) or on the Pi itself, where
the same commands work but a full build takes over ten minutes. Every behaviour is
proven on a venue below before it is called done.

## Build dependencies

| For | Debian packages |
|---|---|
| Toolchain | GCC 14 from trixie, `-std=c++26`; trixie is the oldest release that has it |
| Logger | `build-essential`, `pkg-config`, `libsdbus-c++-dev`, `libsqlite3-dev` | (sdbus-c++ 2) |
| Web dashboard | `libcpp-httplib-dev`, `nlohmann-json3-dev`, `libsqlite3-dev` |
| Display | `libfreetype-dev`, `libharfbuzz-dev`, `libgpiod-dev`, `libpng-dev`, `libqrencode-dev`, `fonts-dejavu-core` |
| Package | `dpkg-dev` |

## Build rules

| Subject | Rule |
|---|---|
| Warnings | `-Wall -Wextra -Werror`: a warning fails the build. Library headers are included as system headers (`-isystem`), so only our code is held to it |
| Install directories | Named as autotools names them and set on the `make` line: `PREFIX` (`/usr`), `SYSCONFDIR` (`/etc`), `LOCALSTATEDIR` (`/var`), `RUNSTATEDIR` (`/run`), `DATADIR` (`$(PREFIX)/share`), `FONTDIR` (the system's DejaVu, `/usr/share/fonts/truetype/dejavu`, whatever `PREFIX` is). The defaults are the Debian layout, and `packaging/build-deb.sh` names them explicitly |
| Paths in the code | Only in `src/common/paths.h`, built from those directories: the config, the catalogue, the state directory's files, setup mode's run files, the fonts. No other file of `src/` or `web/pages/` spells out a path |
| Linking | `LDFLAGS` reach every link line, so a sanitizer's flags given in the environment apply to compiling and linking alike; `BIN` and `BUILD` name the output folders (`bin`, `build`) |
| A failed recipe | Leaves no half-made target: `.DELETE_ON_ERROR` |
| Clock | `build-in-container.sh` steps a podman VM's clock before building, so make never sees sources from the future |

## Make targets

| Target | Does |
|---|---|
| `make` | Builds `bin/purrdometer-logger`, `bin/purrdometer-display`, then `bin/pagec`, the pages in `build/gen/`, and `bin/purrdometer-web` |
| `make test` | The include check, the maintainer scripts' test ([packaging.md](packaging.md#proven-by)), `pagec --selftest`, and the four programs' `--selftest`: the logger (accounting, storage, a failing write, the link's ownership), the web server (wheels, calendar, pages, settings over HTTP, overlapping saves, a database through the page), the display (panel and touch mapping, text, every screen, PNG), the setup helper (commands, the SSH key, the state machine over a scripted `nmcli`) |
| `make test-address`, `make test-thread` | The same `make test` built apart (`build/<name>/`, `bin/<name>/`) with AddressSanitizer and UndefinedBehaviorSanitizer, or ThreadSanitizer; any report fails the run. They need a machine whose kernel the sanitizer runtime supports (GCC 14 on trixie; an arm64 container on a Mac qualifies, a Pi Zero 2 W may lack the memory); the default build and `make test` are unchanged |
| `make preview` | Renders every screen to `preview/*.png` from a fake-history database; no hardware |
| `make media` | Same renders into `docs/media/` for the README |
| `make serve` | The web dashboard on port 8081 over fake history, from the checkout |
| `make deb` | `packaging/build-deb.sh` ([packaging.md](packaging.md)) |

## Testing venues

| Venue | Command | When |
|---|---|---|
| Unit | `make test` | Before each commit of code |
| Sanitizers | `make test-address`, `make test-thread` | Any change to storage, ownership across threads, buffers or the setup helper's descriptors |
| Screens | `make preview`, then look at the PNGs | Any change to what is drawn |
| Web page | `make serve`, then open `http://whiskerwheel:8081/` | Any change to a template or what the dashboard shows |
| On device | build and install the `.deb` on whiskerwheel; `journalctl -u purrdometer-logger -u purrdometer-display -f` | Anything touching BLE, the panel, touch, systemd units or maintainer scripts |
| BLE trace | `sudo purrdometer-btdiag` | Anything touching the link: prints connection parameters and disconnect reasons from `btmon` |
| Database | `sqlite3 /var/lib/purrdometer/purrdometer.db` | Checking what the logger stored |

There is no CI; these venues run locally. A test never writes to the real
database: the previews and unit tests use their own files under a temporary
directory.

## Running

The four programs run as systemd services: the logger, the screen and the web
server as the `purrdometer` system user, the setup helper as root. For
development the logger, screen or web server can be stopped (`sudo systemctl stop purrdometer-display`)
and run in a terminal from the checkout:

```sh
bin/purrdometer-display -v --db /var/lib/purrdometer/purrdometer.db
bin/purrdometer-logger --db tmp/test.db --sensor-file tmp/sensor
```

Only one process may drive the panel, and only one logger may own the
sensor's notifications, so stop the service before running its program by
hand.
