---
name: robust-architecture
description: Two patterns for less fragile code in purrdometer - derive from the source of truth for the thing in question (each display screen reads its own SQLite query, the logger writes one authoritative rotation count per minute), and give every consumer the same shared mechanism instead of a special path (no special-cased screens; all four screens read from the same query layer). Use when deciding where a piece of state lives, when writing anything that reads from SQLite or renders a display screen, and when tempted to add a one-screen or one-sensor special case.
---

# Robust architecture: one source of truth, one shared mechanism

## 1. Read the source of truth for the thing itself

A per-screen value read from a neighbouring screen's cache, or a
global computed "because it was at hand", is right for the first screen
and wrong the moment a second screen needs it differently.

**The rule.** Whatever is derived for a screen — its distance total,
its date range, its rotation counts — is derived from a SQLite query
that screen owns, never from a sibling screen's last-computed value or
a remembered global. The logger writes one authoritative value per
minute to SQLite; everything else reads from there. If the needed fact
is not reachable from the query, extend the schema or the query rather
than approximating from what happens to be nearby.

Corollaries:

- **The display reads the same row the logger wrote.** If the logger
  and the display compute distance differently from the same rotation
  count, the number shown is wrong. One conversion factor, one place.
- **Don't cache a query result across screen transitions unless the
  data cannot have changed.** The e-paper refresh is slow, but holding
  a stale query result is worse than a fresh query on each transition.
- **`make preview` with the fake DB hides stale-cache bugs** the same
  way a single-sensor run hides reconnect bugs: the fake DB never
  changes, so any caching logic that breaks on live updates looks fine.

## 2. One mechanism for every consumer

Every display screen reads through the same query helpers; every
SQLite write goes through the same write path.

**The rule.** When a new screen or a new data view is added, route it
through the mechanism the existing ones use. If the existing mechanism
cannot carry it, extend the mechanism for everyone. A special path is
allowed only for something that is genuinely a different thing (the
ALL TIME screen aggregating across all rows is not a special case of
the 30-DAYS screen; it is a different query).

This pays off twice: a bug in the query layer is a bug every screen
hits, so an odd screen's wrong number becomes a test of the shared
path; and a schema change made once changes for all consumers with no
second edit.

The same holds outside screens:

- The BLE reconnect path, the initial connection path and the
  selftest path all parse CSC notification bytes through the same
  parser function — no parser-inline copy in the reconnect handler.
- Every SQLite write (normal operation and `--selftest`) goes through
  the same write helper; there is no selftest-only schema shortcut.

## Before shipping

- Would this still be correct after a logger restart while the display
  is showing data?
- Is there a copy of this fact somewhere that can now disagree with it?
- Is this path only for one screen, only for the selftest, only for the
  initial connection? Why?
