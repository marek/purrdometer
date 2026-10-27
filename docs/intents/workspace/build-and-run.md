# Build and run

> Owns the build, the testing venues and running on the Pi. The package is
> owned by [packaging.md](packaging.md).

## Requirement

Everything builds on the Pi itself (Raspberry Pi OS bookworm, arm64) with
Debian packages only: no pip, no vendored libraries, no PHP. Every behaviour is
proven on a venue below before it is called done.

## Build dependencies

| For | Debian packages |
|---|---|
| Logger | `build-essential`, `pkg-config`, `libsdbus-c++-dev`, `libsqlite3-dev` |
| Web dashboard | `libcpp-httplib-dev`, `nlohmann-json3-dev`, `libsqlite3-dev` |
| Display | `python3-pil`, `python3-spidev`, `python3-smbus2`, `python3-gpiozero`, `python3-lgpio`, `fonts-dejavu-core` |
| Package | `dpkg-dev` |

## Make targets

| Target | Does |
|---|---|
| `make` | Builds `bin/purrdometer-logger`, then `bin/pagec`, the templates in `build/gen/`, and `bin/purrdometer-web`. Run it without `-j`: compiling `httplib.h` twice at once makes the Zero swap |
| `make test` | `purrdometer-logger --selftest` (accounting and storage), `purrdometer-web --selftest` (wheels, calendar, templates, a database through the page) and the Python unit tests in `python/tests` |
| `make preview` | Renders every screen to `preview/*.png` from a fake-history database; no hardware |
| `make media` | Same renders into `docs/media/` for the README |
| `make serve` | The web dashboard on port 8081 over fake history, from the checkout |
| `make deb` | `packaging/build-deb.sh` ([packaging.md](packaging.md)) |

## Testing venues

| Venue | Command | When |
|---|---|---|
| Unit | `make test` | Before each commit of code |
| Screens | `make preview`, then look at the PNGs | Any change to what is drawn |
| Web page | `make serve`, then open `http://whiskerwheel:8081/` | Any change to a template or what the dashboard shows |
| On device | build and install the `.deb` on whiskerwheel; `journalctl -u purrdometer-logger -u purrdometer-display -f` | Anything touching BLE, the panel, touch, systemd units or maintainer scripts |
| BLE trace | `sudo purrdometer-btdiag` | Anything touching the link: prints connection parameters and disconnect reasons from `btmon` |
| Database | `sqlite3 /var/lib/purrdometer/purrdometer.db` | Checking what the logger stored |

There is no CI; these venues run locally. A test never writes to the real
database: the previews and unit tests use their own files under a temporary
directory.

## Running

All three programs run as systemd services under the `purrdometer` system user.
For development either can be stopped (`sudo systemctl stop purrdometer-display`)
and run in a terminal from the checkout:

```sh
cd python && python3 -m purrdometer.display -v --db /var/lib/purrdometer/purrdometer.db
bin/purrdometer-logger --db tmp/test.db --sensor-file tmp/sensor
```

Only one process may drive the panel, and only one logger may own the
sensor's notifications, so stop the service before running its program by
hand.
