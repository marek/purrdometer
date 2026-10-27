---
name: fresh-deploy
description: purrdometer is installed fresh from a .deb onto the Pi - there is no upgrade path to stay compatible with, no old config format to preserve, no clients in the field depending on internal behaviour. Default to the clean shape of the code. But removing old code is never licence to remove the capability it provided. Use before deleting a file, function, constant or feature, before writing a compatibility path or alias, and when restructuring code that carries features.
---

# Assume a fresh deployment

Each install is a fresh `.deb` onto the Pi. There are no clients
depending on internal BLE handling behaviour, no saved state in an old
format, no installed config that a user has tuned. Default to the clean
shape of the code.

## Removing old code must never remove what it did

This is the load-bearing rule.

If the polling-based BLE path is replaced by a notification-based one,
the capability — counting rotations — carries over, and the old path
goes only once its replacement covers it. Nobody decides to remove a
capability; what removes them is replacing the code that carried them
with code that does not.

"No backward compatibility" is about not keeping duplicate old
implementations beside their replacement. It is never licence to drop a
capability because carrying it over is more work. Before deleting or
replacing anything, list what it does beyond the obvious path — every
error recovery path, every selftest case, every display screen it
feeds — and confirm each still works on what replaces it. If re-homing
one of them is real work, that is a reason to plan it, not to drop it.

Two checks that apply here often:

- **Commented-out or disabled code may have been broken, not
  unwanted.** Restoring a feature means validating that its logic makes
  sense, not pasting it back.
- **Restructuring changes no behaviour.** A file split, a rename or a
  helper move lands with `make test` passing and the same numbers on
  the display.

## What "remove, don't deprecate" means here

- **No aliases for old names.** A renamed SQLite column or a renamed
  C++ function leaves no old name behind; the schema has one name per
  field.
- **No compatibility shims in the D-Bus handling.** The BlueZ API
  policy has one form; there is no legacy mode beside it.
- **Third-party libraries** (`sdbus-c++`, `sqlite3`, Pillow) are not
  edited or restyled; if they lack something, our code works around it
  on our side of the boundary.
- **Rename freely** when a name is wrong, without leaving the old one.

## The exception: what a person has made

- **The SQLite database at `/var/lib/purrdometer/purrdometer.db`**
  holds data the user cannot reconstruct: months of rotation counts.
  A schema change that would destroy or silently corrupt existing data
  is not a "fresh deploy" operation; it requires a migration and the
  user's awareness.
- **A contributor's uncommitted files** in any working tree.

## What does not count as data

Build artefacts, `tmp/`, the `.deb` before install, preview PNGs,
test fixtures. Change their shape freely and rebuild.

## When in doubt

If the change touches the live SQLite database, treat it as data. If
it only touches source code or something rebuilt from source — make the
change and remove what it replaces, once the replacement does
everything the old thing did.
