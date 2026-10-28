# Wheel profiles

Date: 2026-10-28 08:46 EDT

## Context

Distance was every rotation ever × the current wheel's circumference, so
changing the wheel rewrote all past distance: right for a mistake, wrong for
a new wheel. Wheels become profiles in `/var/lib/purrdometer/profiles.conf`;
the logger stores each rotation under the profile current when it arrives,
and distance is each profile's rotations × its own circumference. Editing a
profile is the correction; a new profile is a new wheel. The screens and the
dashboard look as before; only the settings page changes. Settled in
[data/wheels.md](../intents/data/wheels.md),
[data/storage.md](../intents/data/storage.md) and
[web/settings.md](../intents/web/settings.md).

Alongside: the files take Linux's `.conf` name. The catalogue is
`/usr/share/purrdometer/wheels.conf`; the page's settings are
`/var/lib/purrdometer/settings.conf`, and the package moves an existing
`settings.ini` there on upgrade.

## Tasks

- [x] Intents: wheels (profiles, the catalogue), storage (minutes by wheel, schema 4), settings (the profile list, settings.conf), packaging (the rename on upgrade)
- [x] `wheels.ini` becomes `wheels.conf` in the source, the package and the docs
- [x] `settings.ini` becomes `settings.conf`; preinst moves an existing file once, never over a newer one
- [x] Profiles: read, written, seeded as profile 1 from `[wheel]` when the file is missing; a profile has a preset, and a diameter only when measured or custom
- [x] Logger: each minute's rotations stored under the current profile; a switch never resets the count; a version 3 database moves to version 4 with all history under profile 1
- [x] Distance everywhere (screens, dashboard, API) is per-profile rotations × that profile's circumference; speed uses the current profile
- [x] Settings page: the profile list, edit, new wheel, use this wheel, delete an unused one; `[wheel]` model and diameter no longer in settings.conf
- [x] Self-tests: seeding, a switch mid-run loses and moves nothing, totals across two wheels, an edit recomputes only that wheel, migration, the rename
- [x] Guide: configuration, web settings, upgrade notes
- [x] Installed on whiskerwheel: settings.ini moved with its values, profile 1 is the wheel set before, totals unchanged
