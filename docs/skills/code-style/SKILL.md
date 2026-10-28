---
name: code-style
description: Comment, prose, naming and coding conventions for purrdometer (C++26 source, plus the docs/intents corpus) - use before writing or reviewing comments, before writing or editing any intent doc, when choosing a name for a new file, function or type, and before checking BLE or D-Bus handling against the BlueZ reference.
---

# Code style

## This applies to `docs/intents` prose, not just code comments

Every rule below — no history narration, only state the non-obvious,
keep it concise — governs `docs/intents/**/*.md` exactly as much as a
code comment. A doc section describing a system is a comment at
paragraph scale: it states behaviour and the non-obvious reason for it,
never "before the rework it was X" or "this is a real improvement over
what it replaced". The only place history is told is
`docs/intents/bugs/`.

## Comments are sparse

Add one only where the intent is not obvious from the code, as a short
one-liner:

```cpp
// clamp to valid CSC wheel revolution range: sensor wraps at 2^16
rev_count = static_cast<uint16_t>(raw & 0xFFFF);
```

No block comments, no explanation of the obvious, no banner
separators. Leave existing comments as they are unless a change makes
one false.

## Comments describe behaviour, not history

A comment states what the code does and why that is right — never what
it used to do, what bug it fixed, or what was reported. That reasoning
lives in the commit message.

**Violating patterns** — if about to write one, stop and rephrase:
- `used to` / `previously` / `no longer` / `the old <thing>` /
  `replaces` / `instead of the`
- quoting a bug report or a remark from a session
- `was tried first`, `turned out to be`, `confirmed by running`,
  `observed crashing` — how it was found, not what is true
- comparing against an implementation that is no longer in the file
- a commit reference on a line of code (the commit message says that)

```cpp
// BAD - narrates history
// used to poll every 100 ms, which drained the battery faster;
// now subscribes to CSC notifications

// GOOD - states behaviour and why
// subscribe to CSC notifications: the sensor pushes data only when
// the wheel is turning, so polling would wake the CPU for nothing
```

**A fix often makes the surrounding comment false.** Check comments
near a change for claims about behaviour that no longer hold.

**Fix by asking**: what does this code do, and the non-obvious reason
it does it that way? Write only that.

## Keep it concise

Before finishing a comment or a doc paragraph, cut:

- **Self-evaluation** — "this cleanly solves", "the right pattern".
- **The same fact from a second angle** — if a clause starts "in other
  words", the first one was enough.
- **Stacked qualifiers** — say "X because Y", not three clauses
  building up to it.
- **Narrating a table before the table** — point at it.

## C++26 conventions

- The build is `-std=c++26` on GCC 14 (Raspberry Pi OS trixie). Use the
  standard library where it says the thing directly: `std::format` for
  text (never `snprintf` into a buffer), `std::chrono` calendar types for
  dates, `starts_with` / `contains`, `std::ranges` algorithms,
  `std::from_chars` for numbers, `std::scoped_lock`, `std::numbers::pi`.
- A newer feature is used where it makes the code clearer, not to show
  it off; GCC 14's C++26 support is partial, so check before relying on
  a C++26 library facility.
- D-Bus signal handlers are registered through sdbus-c++ proxy objects
  and deregistered when the proxy is destroyed; never hold a raw
  callback pointer past the object's lifetime.
- Every object that owns a resource has exactly one owner
  (`std::unique_ptr<T>`); resource release belongs in the destructor,
  not scattered across call sites.
- State, kind or mode is an `enum class`; `nullptr`, never `NULL`;
  named casts, never C casts; file-local code in an unnamed namespace;
  no macros of our own.
- **No magic numbers.** A bare `60` tells the reader nothing;
  `MAX_REVS_PER_SECOND` in the relevant source file does.
  BLE characteristic UUIDs and SQLite column names are named constants,
  not string literals scattered through the code.
- **Read the BlueZ D-Bus API** (the `org.bluez.GattCharacteristic1`
  interface and the Cycling Speed and Cadence profile specification)
  before writing BLE handling code.
- Third-party library headers (`sdbus-c++`, `sqlite3`) are not
  restyled.

## Templates (`web/pages/*.page`)

- `<%= %>` by default; `<%== %>` only for markup a C++ function built on
  purpose. A double, a date or a distance goes through a named
  formatter (`km`, `fixed`, `thousands`, `cal::longDate`), never
  printed raw ([web/pages.md](../../intents/web/pages.md)).
- Decide what to show in C++ (`buildDashboard`); keep drawing
  arithmetic (SVG coordinates) in the template.
- End control-only lines with `-%>` so the output has no stray blank
  lines.

## Guard clauses

Handle the exceptional case first and return, so the main path stays flat.

## Naming

- A name says what the thing is: `todayRevs`, `diameterM`, `Dashboard`.
- **C++:** types are `PascalCase` (`Wheel`, `Dashboard`); functions,
  variables and members are `camelCase` (`loadStats`, `diameterM`);
  private data members end in an underscore (`connected_`); constants
  are `UPPER_SNAKE_CASE` (`MAX_REVS_PER_SECOND`); namespaces are
  lowercase (`purr`, `purr::web`); files are `snake_case`, named for
  the class or concept they hold (`touch_input.cpp`, `sensor_link.cpp`). A
  quantity's unit ends its name (`diameterM`, `lastWall`). No type prefix.
- **A new name must not collide with an existing one.** Grep the
  source first — the headers and module exports are the contract.
- Rename only when a name is actively misleading; grep every reference
  first.

## Structure

- **One job per class, one class per header.** A class that gains a
  second reason to change, or a file past about 300 lines, is split
  while writing it, never later and never only when asked. The display's
  loop decides what to show; the refresh policy, touch input, the boot
  window and each screen are their own classes and files.
- A program lives in its own folder (`src/logger/`, `src/display/`,
  `src/web/`, `src/setup/`) with a `main.cpp` that only parses options,
  builds the parts and runs them, and a `selftest.cpp`. What more than
  one program uses is in `src/common/`. A program never includes
  another program's folder. The full rules are in
  [workspace/source-layout.md](../../intents/workspace/source-layout.md).
- Two features that need the same behaviour share the implementation;
  the NOW screen and the 30-DAYS screen both read from SQLite through
  the same query helper.
