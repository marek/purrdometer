# Working on purrdometer

How the repo is laid out and how to build, test and package it. For using it,
start at the [guide front page](README.md).

## Layout

Development rules and the design corpus live beside the code: see
[AGENTS.md](../../AGENTS.md), the triple helix of **intents** (what
purrdometer must satisfy), **tests** and **implementation**, with plans,
skills and roles beside it.

| Folder | What |
|---|---|
| `src/` | A folder per program, each with `main.cpp` and `selftest.cpp`: `logger/` (measurement parsing, accounting, the SQLite store, the BlueZ adapter and sensor link, the monitor loop); `display/` (pins, panel and touch drivers, canvas and fonts, `screens/` one file each, taps and holds, the refresh policy, the boot window, the app loop); `web/` (the server, a module per group of routes, the save guard); `setup/` (commands, NetworkManager, the login account, setup mode's state machine). `common/` holds what several use (config, wheels, the database reader, formats, calendar days, file watching, setup mode's shared files); `tools/pagec.cpp` is the page compiler. [workspace/source-layout.md](../intents/workspace/source-layout.md) has the rules |
| `web/pages/` | The dashboard's pages as `.page`, compiled to C++ ([templates](../intents/web/pages.md)) |
| `data/wheels.ini` | The wheel presets, read by both the screen and the dashboard |
| `packaging/` | `build-deb.sh`, the Debian control file and maintainer scripts, the systemd units, the default config |
| `system/` | Files the package installs into the system (`btdiag.sh`, the NetworkManager drop-in) |
| `tools/` | `check-plans.sh` and `plan-items.sh` (the plans), `hooks/` |
| `scripts/` | `agent-setup.sh` (links skills and roles into `.claude/`) |
| `docs/intents/` | Normative design specs (the intent corpus) |
| `docs/plans/` | Dated, tracked work plans |
| `docs/skills/` | How to work here |
| `docs/roles/` | Working personas (developer, tester, reviewer, planner) |
| `docs/guide/` | This user guide |
| `docs/media/` | The README's pictures, made by `make media` |
| `tmp/` | Git-ignored scratch space (`PURR_TMP` overrides) |

## Build and test

On any arm64 machine with podman or docker (an Apple-silicon Mac is ideal),
in a Debian trixie container that matches the Pi:

```sh
packaging/build-in-container.sh             # make test, then the .deb in dist/
packaging/build-in-container.sh make test   # any command
scp dist/purrdometer_*.deb whiskerwheel: && ssh whiskerwheel 'sudo apt install ./purrdometer_*.deb'
```

A full build there takes about 20 seconds. On the Pi itself it works the same
way, but slowly:

```sh
make            # the logger, the template compiler, the dashboard (no -j: the Zero swaps)
make test       # the three programs' self-tests
make preview    # every screen as a PNG in preview/, from made-up history
make serve      # the dashboard on :8081 over made-up history
make deb        # the package, in dist/ (runs make test first)
```

A first build on a Pi Zero takes over 10 minutes and swaps hard, nearly all of
it compiling `httplib.h`; after that, editing a page rebuilds only that page.

## Writing a page

Pages are HTML with C++ in them, with `<% %>` tags:

```html
<div class="big"><%= km(v.totalKm) %> km</div>
<% for (auto& b : v.last30) { -%>
<rect height="<%= fixed(b.height * 120, 1) %>"><title><%= b.label %></title></rect>
<% } -%>
```

`<%= %>` escapes, `<%== %>` does not, `<% %>` is any C++. `make` compiles each
`web/pages/<name>.page` into a function `purr::pages::<name>()`; a
mistake is a compiler error at the template's line. The full syntax is in
[templates](../intents/web/pages.md).

`make preview` needs no panel, so screen work can be checked by eye before it
goes near the hardware. `make media` writes the same renders to `docs/media/`
for the README.

The testing venues, including the checks on the Pi itself, are in
[workspace/build-and-run.md](../intents/workspace/build-and-run.md).

## Versions and releases

The version is the git tag (`tools/version.sh`): a commit tagged `v1.2.0` is
1.2.0, a commit past it is `1.2.0.r<commits>.g<hash>`, so every build after a
release installs over it.

Pushing a `v*` tag builds the `.deb` and the Raspberry Pi image on GitHub and
drafts a release with both (`.github/workflows/packages.yml`). The image can
also be built locally, as root on an arm64 Linux machine:

```sh
sudo packaging/build-image.sh dist/purrdometer_*_arm64.deb
```
