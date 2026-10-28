# Install paths from the build, and no warnings

Date: 2026-10-28 08:59 EDT

## Context

Paths were literals scattered through the source: the config in `/etc`,
the state in `/var/lib/purrdometer`, the catalogue and fonts in
`/usr/share`, setup mode's files in `/run`. They come from one header,
`src/common/paths.h`, built from the install directories the Makefile sets
as autotools names them (`PREFIX`, `SYSCONFDIR`, `LOCALSTATEDIR`,
`RUNSTATEDIR`, `DATADIR`, `FONTDIR`), defaulting to the Debian layout the
package installs. And the build is warning-free, with warnings made errors
so it stays that way. Settled in
[workspace/build-and-run.md](../intents/workspace/build-and-run.md).

## Tasks

- [x] `src/common/paths.h`: every path from the build's install directories; no path literal left in `src/` or `web/pages/`
- [x] Makefile: `PREFIX`, `SYSCONFDIR`, `LOCALSTATEDIR`, `RUNSTATEDIR`, `DATADIR`, `FONTDIR`, passed to the compiler
- [x] The packaging builds with the directories it installs to, and the systemd units and maintainer scripts agree with them
- [x] The build-and-run intent and the development guide name the variables
- [x] Every warning in a full build fixed, make's included; `-Werror` on our code so a new one fails the build
- [x] A build with a different `PREFIX` and `LOCALSTATEDIR` puts those paths in the programs (checked with `--check-config` and `strings`)
