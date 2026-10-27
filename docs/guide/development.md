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
| `src/` | `purrdometer-logger.cpp` (the BlueZ client, rotation accounting, SQLite writer); `purrdometer-web.cpp` (the dashboard server); `web/` (reading the database, the dashboard's data, HTML helpers); `common/` (config, wheels, calendar days); `tools/pagec.cpp` (the template compiler) |
| `web/pages/` | The dashboard's pages as `.page`, compiled to C++ ([templates](../intents/web/pages.md)) |
| `python/purrdometer/` | The screen: `render.py` (the screens and touch bar), `data.py` (reading the database), `wheels.py`, `config.py`, `hw.py` (panel and touch driver), `display.py` (refresh loop and CLI) |
| `python/tests/` | Python unit tests |
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

On the Pi:

```sh
make            # the logger, the template compiler, the dashboard (no -j: the Zero swaps)
make test       # both self-tests and the Python unit tests
make preview    # every screen as a PNG in preview/, from made-up history
make serve      # the dashboard on :8081 over made-up history
make deb        # the package, in dist/ (runs make test first)
```

A first build takes about 7 minutes on a Pi Zero, nearly all of it
compiling `httplib.h`; after that, editing a template rebuilds only that page.

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

## Versions

`VERSION` holds the release version. `RELEASE=1 packaging/build-deb.sh` builds
exactly it; a plain build appends `+git<timestamp>.<sha>`, so every dev build
installs over the one before.
