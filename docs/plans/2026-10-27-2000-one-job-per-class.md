# One job per class, one folder per program

Date: 2026-10-27 20:00 EDT

## Context

The programs had grown single classes and files that did everything: the
display's app ran the boot window, touch, holds, setup mode, config reloads
and the refresh policy; the logger's monitor ran discovery, connections,
settings and accounting; the web server's one file held every route and the
self-test. The source is divided into a folder per program and a class per
job, as [workspace/source-layout.md](../intents/workspace/source-layout.md)
states, with no change in behaviour: every self-test passes before and after.

## Tasks

- [x] `src/common/`: file watching and key=value files on their own; setup mode's state and form split
- [x] `src/logger/`: accounting, store, BlueZ adapter, sensor connection, the loop, `main.cpp`, `selftest.cpp`
- [x] `src/display/`: gpio, panel, touch and orientation split; fonts; screens one file each; taps and holds; refresh policy; the app; `main.cpp`, `selftest.cpp`
- [x] `src/web/`: the server and a module per group of routes; `main.cpp`, `selftest.cpp`
- [x] `src/setup/`: commands, NetworkManager, login, the state machine; `main.cpp`, `selftest.cpp`
- [x] Makefile builds each program from its folder and `src/common/`; warning-free; every self-test passes with the same checks
- [x] The code-style skill, AGENTS.md and the development guide state the layout
- [x] `tools/check-includes.sh` in `make test`: no program includes another's folder
- [x] Installed on whiskerwheel and running as before
