# Source layout

> Owns how the C++ source is divided: one folder per program, one class per
> concept, one header per class. The `code-style` skill is the short form.

## Requirement

Each piece of the source does one thing and can be read on its own. A
program's entry point only parses its arguments and wires its parts
together; everything it does lives in a class named for that job. A class
that starts doing a second job is split before the second job is finished.

## Folders

| Folder | Holds |
|---|---|
| `src/common/` | What more than one program uses: config and settings, wheels, calendar days, number formats, the database reader, key=value files, file watching, setup mode's shared files |
| `src/logger/` | `purrdometer-logger`: revolution accounting, the SQLite writer, BlueZ discovery, the sensor's connection, the logger's loop |
| `src/display/` | `purrdometer-display`: the panel, touch and pins; the canvas and text; taps and holds; the refresh policy; the screens, one file each (`src/display/screens/`); the app's loop |
| `src/web/` | `purrdometer-web`: the server and one module per group of routes (dashboard, API, settings, setup, the port-80 portal), access rules, the pages' view models |
| `src/setup/` | `purrdometer-setup`: running commands, NetworkManager, the login account, the setup state machine |
| `src/tools/` | Build tools (`pagec`) |
| `web/pages/` | The `.page` files |

A program's folder has a `main.cpp` and a `selftest.cpp`; nothing else in it
has a `main`. Includes are written from `src/` (`#include "logger/store.h"`).
A program includes from `src/common/` and its own folder, never another
program's; `tools/check-includes.sh`, run by `make test`, fails on one.

## Classes and files

| Subject | Rule |
|---|---|
| A class | One concept or job: the SQLite writer, the BlueZ adapter, one sensor's connection, the panel, the refresh policy, a group of routes. It is asked through queries and changed through its members |
| A file | One class, or a close group of functions on one concept (one screen's drawing); a header per class |
| Size | A file that passes about 300 lines, or a class with a second reason to change, is split along that reason |
| Entry points | `main.cpp` parses arguments, builds the parts and runs them; no logic of its own |
| Self-tests | In the program's `selftest.cpp`, through the parts' public members |
| Values | Plain structs with public fields (`Sample`, `Wheel`, `State`) |
| Dependencies | One way: `main` → the program's loop → its parts → `src/common/` |
