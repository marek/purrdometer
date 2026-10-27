# Packaging

> Owns the `.deb`: what it contains, its version, its maintainer scripts, the
> system settings it owns, and the install, remove and purge guarantees.

## Requirement

One `.deb` installs a working purrdometer on a Raspberry Pi OS (trixie or
later) Pi: all three services enabled and running, and every system setting they
need applied. No setting outside the package's own files is ever made by
hand. Remove stops the services; purge also undoes the system settings. The
rotation history survives both.

## Package

| Subject | Rule |
|---|---|
| Name, architecture | `purrdometer`, the build machine's architecture (`arm64` on the Pi); built natively on the Pi |
| Build | `packaging/build-deb.sh`: runs `make test` first and refuses to package a failing tree; artifacts in `dist/`. Run in `packaging/build-in-container.sh` on a developer machine, in the release workflow, or on the Pi. The build dependencies are listed once, in `packaging/build-deps` |
| Version | The git tag, said by `tools/version.sh`: a commit tagged `v<version>` is that version, the highest of several tags on one commit; a commit past the last tag is `<version>.r<commits since>.g<short hash>`, which dpkg orders after that tag and before the next; a commit with no tag behind it is `0.0.0.r<commits>.g<short hash>`. A tag dpkg cannot take fails the build |
| Contents | `/usr/bin/purrdometer-logger`, `/usr/bin/purrdometer-web`, `/usr/bin/purrdometer-display`, the wheel presets in `/usr/share/purrdometer/wheels.ini`, `/usr/sbin/purrdometer-btdiag`, both systemd units in `/lib/systemd/system`, `/etc/purrdometer/purrdometer.conf`, `/etc/NetworkManager/conf.d/purrdometer-wifi-powersave.conf`, the README in `/usr/share/doc/purrdometer` |
| Conffiles | `/etc/purrdometer/purrdometer.conf` and the NetworkManager drop-in: local edits survive upgrades |
| Depends | the three binaries' shared libraries computed by `dpkg-shlibdeps`, plus BlueZ, `adduser` and the display's font; `network-manager` and `sqlite3` are Recommends |
| Prompts | none: the maintainer scripts are non-interactive |

## Services

| Unit | Runs | As |
|---|---|---|
| `purrdometer-logger.service` | `purrdometer-logger` against `/var/lib/purrdometer` | `purrdometer`, with `bluetooth` |
| `purrdometer-display.service` | `purrdometer-display` | `purrdometer`, with `spi`, `i2c`, `gpio` |
| `purrdometer-web.service` | `purrdometer-web` | `purrdometer`, read-only system, no devices, memory and CPU capped ([../web/dashboard.md](../web/dashboard.md)) |

All restart on failure, start at boot, and share `/var/lib/purrdometer` as
their state directory.

## System settings the package owns

| Setting | Where | Why |
|---|---|---|
| `ConnectionSupervisionTimeout = 600` under `[LE]`, after a `# purrdometer:` marker line | `/etc/bluetooth/main.conf` | [../ble/link.md](../ble/link.md); BlueZ 5.66 reads no drop-in directory, so the line goes into `main.conf` itself |
| `Privacy = device` under `[General]`, after its own `# purrdometer:` marker line | `/etc/bluetooth/main.conf` | A random address for every connection: a sensor that kept a pairing with the Pi's own address sends it nothing ([../ble/link.md](../ble/link.md#security)) |
| Cached `[ConnectionParameters]` cleared whenever a BlueZ setting is added | `/var/lib/bluetooth/*/*/info` | A cached per-device value overrides the default |
| `wifi.powersave = 2` | the NetworkManager drop-in | [../ble/link.md](../ble/link.md) |
| Bluetooth not soft-blocked (rfkill), on every install and upgrade | `/sys/class/rfkill`, kept by systemd-rfkill | Without it BlueZ cannot power the adapter; an OS upgrade can leave it blocked |
| The `purrdometer` system user and group | `adduser` | Services run unprivileged |

## Guarantees

| Action | Result |
|---|---|
| Install, upgrade | User and state directory exist, each BlueZ setting is present exactly once, all three services enabled and restarted |
| Remove | Services stopped and disabled; conffiles, system settings and data kept |
| Purge | Every marker line and the setting after it removed from `main.conf` and BlueZ restarted; the history in `/var/lib/purrdometer` and the `purrdometer` user that owns it are kept, and the purge says so |
| Reinstall after purge | Same as a first install; the kept history is picked up |

A later `bluez` upgrade may ask about the modified `main.conf`; keeping the
local version keeps the setting.

## The Raspberry Pi image

| Subject | Rule |
|---|---|
| Base | Raspberry Pi OS Lite, 64-bit, trixie: the exact image and its SHA-256 pinned in `packaging/image-base`, checked before use. A newer base is a deliberate edit of that file |
| Build | `sudo packaging/build-image.sh <deb>`: installs the package and its dependencies in a chroot, turns on SPI and I²C in `config.txt`, shrinks the root filesystem to its minimum (repeating `resize2fs -M` until it stops shrinking) plus 128 MiB and the image to match, zeroes its free blocks, compresses with `xz`; arm64 host, root |
| In the chroot | The package's `postinst` writes its files and enables the services but changes nothing live (rfkill, WiFi power-save, restarts): the first boot runs them |
| First boot | Raspberry Pi OS grows the root filesystem to fill the card |
| Users | None: the user, WiFi and SSH are set when flashing, by Raspberry Pi Imager's OS customisation |
| Output | `dist/purrdometer_<version>_raspios-trixie-arm64.img.xz` and its `.sha256` |

## Release builds

A pushed `v<version>` tag, or a manual run from the Actions tab, runs
`.github/workflows/packages.yml` on GitHub's arm64 runners, the Pi's
architecture, so nothing is emulated. The `deb` job builds the package in a
`debian:trixie` container with `packaging/build-deb.sh`; the `image` job builds
the image from that package with `packaging/build-image.sh`; on a tag the
`release` job drafts a GitHub release with both. The jobs call the same
scripts a contributor calls locally.
