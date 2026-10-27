# Packaging

> Owns the `.deb`: what it contains, its version, its maintainer scripts, the
> system settings it owns, and the install, remove and purge guarantees.

## Requirement

One `.deb` installs a working purrdometer on a Raspberry Pi OS (bookworm or
later) Pi: all three services enabled and running, and every system setting they
need applied. No setting outside the package's own files is ever made by
hand. Remove stops the services; purge also undoes the system settings. The
rotation history survives both.

## Package

| Subject | Rule |
|---|---|
| Name, architecture | `purrdometer`, the build machine's architecture (`arm64` on the Pi); built natively on the Pi |
| Build | `packaging/build-deb.sh` (`make deb`): runs `make test` first and refuses to package a failing tree; artifacts in `dist/`. A release build starts from `make clean`; a dev build reuses what is built |
| Version | `VERSION` for a release build (`RELEASE=1`); otherwise `VERSION+git<UTC timestamp>.<short sha>[.dirty]`, which dpkg orders after the release and after every earlier dev build |
| Contents | `/usr/bin/purrdometer-logger`, `/usr/bin/purrdometer-web`, `/usr/bin/purrdometer-display` (a wrapper over the Python package in `/usr/lib/purrdometer`), the wheel presets in `/usr/share/purrdometer/wheels.ini`, `/usr/sbin/purrdometer-btdiag`, both systemd units in `/lib/systemd/system`, `/etc/purrdometer/purrdometer.conf`, `/etc/NetworkManager/conf.d/purrdometer-wifi-powersave.conf`, the README in `/usr/share/doc/purrdometer` |
| Conffiles | `/etc/purrdometer/purrdometer.conf` and the NetworkManager drop-in: local edits survive upgrades |
| Depends | both binaries' shared libraries computed by `dpkg-shlibdeps`, plus BlueZ, `adduser`, Python 3 and the display's Python packages and font; `network-manager` and `sqlite3` are Recommends |
| Prompts | none: the maintainer scripts are non-interactive |

## Services

| Unit | Runs | As |
|---|---|---|
| `purrdometer-logger.service` | `purrdometer-logger` against `/var/lib/purrdometer` | `purrdometer`, with `bluetooth` |
| `purrdometer-display.service` | `purrdometer-display` | `purrdometer`, with `spi`, `i2c`, `gpio`; working directory under `/run` because lgpio writes notification FIFOs there |
| `purrdometer-web.service` | `purrdometer-web` | `purrdometer`, read-only system, no devices, memory and CPU capped ([../web/dashboard.md](../web/dashboard.md)) |

All restart on failure, start at boot, and share `/var/lib/purrdometer` as
their state directory.

## System settings the package owns

| Setting | Where | Why |
|---|---|---|
| `ConnectionSupervisionTimeout = 600` under `[LE]`, after a `# purrdometer:` marker line | `/etc/bluetooth/main.conf` | [../ble/link.md](../ble/link.md); BlueZ 5.66 reads no drop-in directory, so the line goes into `main.conf` itself |
| Cached `[ConnectionParameters]` cleared, once, when the setting is first added | `/var/lib/bluetooth/*/*/info` | A cached per-device value overrides the default |
| `wifi.powersave = 2` | the NetworkManager drop-in | [../ble/link.md](../ble/link.md) |
| The `purrdometer` system user and group | `adduser` | Services run unprivileged |

## Guarantees

| Action | Result |
|---|---|
| Install, upgrade | User and state directory exist, the BlueZ setting is present exactly once, all three services enabled and restarted |
| Remove | Services stopped and disabled; conffiles, system settings and data kept |
| Purge | The marker line and its setting removed from `main.conf` and BlueZ restarted; the history in `/var/lib/purrdometer` is kept and the purge says so |
| Reinstall after purge | Same as a first install; the kept history is picked up |

A later `bluez` upgrade may ask about the modified `main.conf`; keeping the
local version keeps the setting.
